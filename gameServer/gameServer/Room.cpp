#include "pch.h"
#include "Room.h"

#include"User.h"
#include "GameSession.h"
#include "PacketHandler.h"
#include "Game.h"
#include "LobbySession.h"

Room::Room(unsigned roomID) : _roomID(roomID),userCount(0), _maxUserCount(0), isStart(false),_game(make_shared<Game>(this))
{
    
}

Room::~Room()
{
    _game = nullptr;
}

void Room::Enter(GameSession* session, unsigned int userId, string userName)
{
    if(isStart==true)
    {
        return;
    }
    userCount.fetch_add(1);
    {
        LOCK_GUARD;
        _gameSessions[userId] = session;
    }
    _game->AddUser(userId, userName);
    session->userId = userId;
    session->game = _game.get();
}

void Room::Leave(unsigned int userId)
{
    // _gameSessions이 떠났는지 확인하는 변수 및 기능 추가 필요 
    LOCK_GUARD
    auto count = _gameSessions.erase(userId);
    if (count)
    {
        userCount.fetch_sub(1);
    }
}


void Room::Broadcast(shared_ptr<char>& buffer)
{
    
    if (isStart == false)
    {
        return;
    }

    for (auto& session : _gameSessions)
    {
        //if session이 Leave 했다면 continue 하는 로직 추가 필요
        session.second->RegisterSend(buffer);
    }
}

unsigned Room::GetRoomID()
{
    return _roomID;
}

void Room::SetMaxUserCount(unsigned number)
{
    _maxUserCount = number;
}

void Room::AddUserID(unsigned int userID)
{
    LOCK_GUARD
    _userList.push_back(userID);
}

bool Room::HasUser(unsigned userID)
{
    LOCK_GUARD
    return _gameSessions.count(userID) > 0;
}

bool Room::HasUserID(unsigned userID)
{
    auto it = find(_userList.begin(), _userList.end(), userID);
    if (it != _userList.end()) {
        return true;
    }
    return false;
}

bool Room::CanStart()
{
    return (_maxUserCount == userCount.load())&&(isStart==false);
}

bool Room::CanEnd()
{
    return (userCount.load() < 1 && isStart==true)||_game->isEnd;
}

void Room::InitGame()
{
    {
        LOCK_GUARD;
        isStart = true;
    }
    
    
    _game->Init(_maxUserCount);

    thread gamethread([this](){
        GameLoop();
        EndGame();
        

        _game = nullptr;
        RoomManager::DeleteRoom(_roomID);
    });
    gamethread.detach();
}

void Room::EndGame()
{
    LOCK_GUARD;

    Protocol::S_GameEnd packet;
    packet.set_messagetype(7);
    packet.set_winnerid(to_string(_game->winner));

    for( auto i :_userList)
    {
        packet.add_userid(to_string(i));
    }

    g_lobbySession.RegisterSend(packet);
    

    cout << "end" << endl;
    for (auto& session : _gameSessions)
    {
        if(session.second!=nullptr)
        {
            session.second->game = nullptr;
        }
        
    }

}


void Room::GameLoop()
{
    chrono::milliseconds loopDuration(33);

    while (CanEnd()==false)
    {
        auto startTime = chrono::steady_clock::now();
        _game->Tick();

        auto elapsedTime = chrono::steady_clock::now() - startTime;

        if(elapsedTime<loopDuration)
        {
            //cout << "sleep" << endl;
            this_thread::sleep_for(loopDuration - elapsedTime);
        }
        
    }

    _game->AttackedBroadcast();
    _game->DeadBroadcast();

   
}

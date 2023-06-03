#include "pch.h"
#include "Session.h"

#include "PacketHandler.h"
#include "Server.h"

Session::Session(unsigned sessionID, boost::asio::io_context& io_context, Server* server)
    : socket(io_context),_sessionID(sessionID),_server(server)
{

}

void Session::RegisterReceive()
{
    

    socket.async_read_some(
        boost::asio::buffer(_receiveBuffer),
        [this] (boost::system::error_code error,size_t transferedBytes){ AfterReceive(error, transferedBytes); }

    );
}

void Session::RegisterSend(char* buffer)
{
    const int size = reinterpret_cast<PacketHeader*>(buffer)->size;
    boost::asio::async_write(
        socket, 
        boost::asio::buffer(buffer,size),
        [this, buffer](boost::system::error_code error, size_t transferredBytes) {AfterSend(error, transferredBytes, buffer); }
    );
}

void Session::RegisterSend(shared_ptr<char>& buffer)
{
    const int size = reinterpret_cast<PacketHeader*>(buffer.get())->size;
    boost::asio::async_write(
        socket,
        boost::asio::buffer(buffer.get(), size),
        [this, buffer](boost::system::error_code error, size_t transferredBytes) {AfterSend(error, transferredBytes, buffer); }
    );
}

void Session::AfterConnect()
{
    Init();
    OnConnect();
    RegisterReceive();
}


void Session::AfterSend(const boost::system::error_code& error, size_t transferredBytes, char* sendBuffer)
{
    if(sendBuffer!=nullptr)
    {
        delete[] sendBuffer;
        sendBuffer = nullptr;
    }
    
    OnSend();
}

void Session::AfterSend(const boost::system::error_code& error, size_t transferredBytes, const shared_ptr<char>& sendBuffer)
{
    OnSend();
}

void Session::AfterReceive(const boost::system::error_code& error, size_t transferredBytes)
{
    if(error)
    {
        if( error == boost::asio::error::eof)
        {
            
        }
        else
        {
            //또다른 에러 발생
            std::cout << "error No: " << error.value() << " error Message: " << error.message() << std::endl;
        }
        OnDisconnect();
        _server->CloseSession(_sessionID);

        return;
    }
    else
    {
        OnReceive(transferredBytes,_receiveBuffer.data());

        RegisterReceive();
    }
}


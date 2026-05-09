#pragma once

#include<stdio.h>
#include<memory>
#include<boost/asio.hpp>
#include<vector>
#include"Encryptor.hpp"

using namespace boost::asio;
using namespace std;

enum ROLE{
    SENDER,
    ACCEPTER
};

class UdpBroadcaster{

    public:
        UdpBroadcaster(io_context &_io, int port=12345);
        void broadcast(const string& msg);
        void wait(const boost::system::error_code& e, std::shared_ptr<string> shared_msg, short time=1);
    
        void stop_broadcasting();
        
    private:
        ip::udp::socket socket;
        ip::udp::endpoint remote_endpoint;
        steady_timer timer;
        bool if_still_broadcasting = true;
        
};

class UdpListener{
    private:
        ip::udp::socket socket;
        ip::udp::endpoint remote_endpoint;
        std::vector<char> recv_buffer;
        std::function<void(const std::vector<char>&)> msg_handler;
    public:
        UdpListener(io_context &io, std::function<void(const std::vector<char>&)> msg_handler ,short port);

        void listen();
};

class UdpManager{
    private:
        UdpListener listener;
        UdpBroadcaster broadcaster;
    public:
        UdpManager(io_context& _io, std::function<void(const std::vector<char>&)> msg_handler, Encryptor &encryptor);
};
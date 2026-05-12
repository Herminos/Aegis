#pragma once

#include<boost/asio.hpp>
#include<stdio.h>
#include<vector>
#include<memory>
#include<functional>


using namespace boost::asio;
using namespace std;

using AcceptHandler = function<void(ip::tcp::socket)>;

class TcpSender{

    private:
        ip::tcp::socket socket;
        ip::tcp::endpoint remote_endpoint;
        ip::tcp::acceptor acceptor;
        vector<char> recv_buffer;

        AcceptHandler accept_handler;
    public:
        TcpSender(io_context &_io, short port);
        void accept();

        void set_accept_handler(AcceptHandler handler) { accept_handler = handler; }
};
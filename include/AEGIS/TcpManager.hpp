#pragma once

#include<boost/asio.hpp>
#include<vector>
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
        TcpSender(io_context &_io, short port=0, const string& host="0.0.0.0");
        void accept();
        void set_peer_socket(ip::tcp::socket peer_socket);

        void set_accept_handler(AcceptHandler handler) { accept_handler = handler; }
        unsigned short get_listen_port() { return acceptor.local_endpoint().port(); }
};
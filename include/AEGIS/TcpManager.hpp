#pragma once

#include<boost/asio.hpp>
#include<vector>
#include<functional>

using AcceptHandler = std::function<void(boost::asio::ip::tcp::socket)>;

class TcpSender{

    private:
        boost::asio::ip::tcp::socket socket;
        boost::asio::ip::tcp::endpoint remote_endpoint;
        boost::asio::ip::tcp::acceptor acceptor;
        std::vector<char> recv_buffer;

        AcceptHandler accept_handler;
    public:
        TcpSender(boost::asio::io_context &_io, short port=0, const std::string& host="0.0.0.0");
        void accept();
        void set_peer_socket(boost::asio::ip::tcp::socket peer_socket);

        void set_accept_handler(AcceptHandler handler) { accept_handler = handler; }
        unsigned short get_listen_port() { return acceptor.local_endpoint().port(); }
};
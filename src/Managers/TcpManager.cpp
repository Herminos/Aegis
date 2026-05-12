#include<boost/asio.hpp>
#include<stdio.h>
#include<vector>
#include<memory>
#include<functional>
#include<AEGIS/TcpManager.hpp>

using namespace boost::asio;
using namespace std;



TcpSender::TcpSender(io_context &_io, short port) : acceptor(_io, ip::tcp::endpoint(ip::tcp::v4(), port)), socket(_io) {

};

void TcpSender::accept() {

    acceptor.async_accept([this](const boost::system::error_code& e, ip::tcp::socket peer_socket) {
        if (e) {
            printf("[ERROR] Accept error: %s\n", e.message().c_str());
            return;
        }
        printf("[INFO] Client connected: %s\n", peer_socket.remote_endpoint().address().to_string().c_str());
        if (accept_handler) {
            accept_handler(move(peer_socket));
        }
        accept();
    });
};
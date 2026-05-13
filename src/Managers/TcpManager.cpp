#include<boost/asio.hpp>
#include<stdio.h>
#include<vector>
#include<memory>
#include<functional>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/Utility.hpp>

using namespace boost::asio;
using namespace std;



TcpSender::TcpSender(io_context &_io, short port) : acceptor(_io, ip::tcp::endpoint(ip::tcp::v4(), port)), socket(_io) {

};

void TcpSender::accept() {

    acceptor.async_accept([this](const boost::system::error_code& e, ip::tcp::socket peer_socket) {
        if (e) {
            log_error(string("[ERROR] Failed to accept incoming connection: ") + e.message());
            return;
        }
        success_info(string("[INFO] Client connected: ") + peer_socket.remote_endpoint().address().to_string());
        if (accept_handler) {
            accept_handler(move(peer_socket));
        }
        accept();
    });
};
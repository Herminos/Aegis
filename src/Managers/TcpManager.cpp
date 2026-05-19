#include<boost/asio.hpp>
#include<stdio.h>
#include<functional>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/Utility.hpp>

using namespace boost::asio;
using namespace std;



TcpSender::TcpSender(io_context &_io, short port, const string& host) : acceptor(_io, ip::tcp::endpoint(ip::make_address(host), port)), socket(_io) {

};

void TcpSender::set_peer_socket(ip::tcp::socket peer_socket) {
    remote_endpoint = peer_socket.remote_endpoint();
    socket = std::move(peer_socket);
}

void TcpSender::accept() {

    acceptor.async_accept([this](const boost::system::error_code& e, ip::tcp::socket peer_socket) {
        if (e) {
            log_error(string("[ERROR] Failed to accept incoming connection: ") + e.message());
            return;
        }
        success_info(string("[INFO] Peer connected: ") + peer_socket.remote_endpoint().address().to_string());
        if (accept_handler) {
            accept_handler(std::move(peer_socket));
        }
        accept();
    });
};
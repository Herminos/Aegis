#include<boost/asio.hpp>
#include<stdio.h>
#include<vector>
#include<memory>
#include<functional>
#include"TcpManager.hpp"

using namespace boost::asio;
using namespace std;



TcpSender::TcpSender(io_context &_io, short port) : acceptor(_io), socket(_io) {
    acceptor.open(ip::tcp::v4());
};

void TcpSender::accept() {
    acceptor.bind(ip::tcp::endpoint(ip::tcp::v4(), 8023));
    acceptor.listen();
    acceptor.async_accept(socket, [this](const boost::system::error_code& e) {
        if (e) {
            printf("[ERROR] Accept error: %s\n", e.message().c_str());
            return;
        }
        printf("[INFO] Client connected: %s\n", socket.remote_endpoint().address().to_string().c_str());

    });
};
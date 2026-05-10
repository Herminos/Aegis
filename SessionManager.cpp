#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<charconv>
#include"Encryptor.hpp"
#include"TcpManager.hpp"
#include"SessionManager.hpp"

using namespace boost::asio;
using namespace std;


Session::Session(io_context &_io, const string& ip_addr, string port, const string& name, const string& hash) :
    socket(_io), remote_endpoint(ip::make_address_v4(ip_addr), stoi(port)), session_name(name), session_hash(hash)
{
    socket.open(ip::tcp::v4());
}

void Session::start(){
    printf("[INFO] Starting session: %s\n", session_name.c_str());
};

void Session::on_read_loop(){
    auto self(shared_from_this());

};

void Session::send_msg(const string& msg) {   
    boost::asio::post(socket.get_executor(), [self=shared_from_this(), msg]() {
        self->msg_queue.push_back(msg);
        if(!self->msg_queue.empty()) 
            self->do_write();//如果消息队列不为空就开始调用
        
    });
};

void Session::do_write() {
    auto self(shared_from_this());
    async_write(socket, buffer(msg_queue.front()), [self](const boost::system::error_code& e, size_t bytes_transferred) {
        if (e) {
            printf("[ERROR] Write error: %s\n", e.message().c_str());
            return;
        }
        self->msg_queue.pop_front();
        if(!self->msg_queue.empty()) 
            self->do_write();//递归调用
    });
}

inline ip::tcp::socket& Session::get_socket() {
    return socket;
};

SessionManager::SessionManager(io_context &_io) : io(_io) {

};

void SessionManager::new_session(const string& ip_addr, const string& port, const string& session_hash, const string& name) {

    auto session=make_shared<Session>(io, ip_addr, port, name, session_hash);
    
    session_map[session_hash]=session;
    session->start(); //start the session

}


#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include"Encryptor.cpp"
#include"TcpManager.hpp"

using namespace boost::asio;
using namespace std;

class Session:public enable_shared_from_this<Session>
{
    private:
        ip::tcp::socket socket;
        ip::tcp::endpoint remote_endpoint;
        void init();//初始化会话，初始化TcpManager开始通信，包括交换密钥
        void start(); //开始对话
        Encryptor encryptor; //加密器实例，处理数据加密解密逻辑
        //TcpManager tcp_manager; //TCP管理器实例，处理TCP连接和数据传输逻辑
    public:
        Session(io_context &_io, const string& ip_addr, short port);//port=8023
        inline ip::tcp::socket& get_socket();
        
};

class SessionManager{
    private:
        map<string, shared_ptr<Session>> session_map;
        io_context &io;
    public:
        SessionManager(io_context &_io);
        void new_session(io_context &_io, const string& ip_addr, short port);
        inline io_context& get_io_context() { return io; }
        
};

Session::Session(io_context &_io, const string& ip_addr, short port) :
    socket(_io), remote_endpoint(ip::make_address_v4(ip_addr), port)
{
    socket.open(ip::tcp::v4());
    
    
}
    

void Session::init() {
    
};

inline ip::tcp::socket& Session::get_socket() {
    return socket;
};

SessionManager::SessionManager(io_context &_io) : io(_io) {

};

void SessionManager::new_session(io_context &_io, const string& ip_addr, short port) {

    auto session=make_shared<Session>(io, ip_addr, port);
    string session_id=session->get_socket().remote_endpoint().address().to_string();
    session_map[session_id]=session;


}
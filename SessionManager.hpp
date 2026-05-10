#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<deque>
#include"Encryptor.hpp"
#include"TcpManager.hpp"

using namespace boost::asio;
using namespace std;

class Session:public enable_shared_from_this<Session>
{
    private:
        ip::tcp::socket socket;
        ip::tcp::endpoint remote_endpoint;
        
        Encryptor encryptor; //加密器实例，处理数据加密解密逻辑

        const string& session_name;
        const string& session_hash;

        void on_read_loop();
        boost::asio::streambuf read_buffer;

        void send_msg(const string& msg);
        std::deque<std::string> msg_queue;
        void do_write();
    public:
        void start(); //开始对话
        Session(io_context &_io, const string& ip_addr, string port, const string& name, const string& hash);
        ip::tcp::socket& get_socket();
        
};

class SessionManager{
    private:
        map<string, shared_ptr<Session>> session_map;
        io_context &io;
    public:
        SessionManager(io_context &_io);
        void new_session(const string& ip_addr, const string& port, const string& session_hash, const string& name);
        inline io_context& get_io_context() { return io; }
        
};

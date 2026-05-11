#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<deque>
#include<functional>
#include"Encryptor.hpp"
#include"TcpManager.hpp"

using namespace boost::asio;
using namespace std;

enum class SessionState{
    PUBKEY_EXCHANGING,
    WAITING_FOR_SESSION_KEY,
    VERIFYING_PEER,
    ACTIVE,
};

class Session:public enable_shared_from_this<Session>
{
    private:
        ip::tcp::socket socket;
        ip::tcp::endpoint remote_endpoint;
        SessionState state=SessionState::PUBKEY_EXCHANGING;//会话状态机，初始状态为公钥交换阶段
        Encryptor &encryptor; //加密器实例，处理数据加密解密逻辑

        const string session_name;
        const string session_hash;

        
        boost::asio::streambuf read_buffer;

        void send_msg(const string& msg);
        std::deque<std::string> msg_queue;
        void do_write();
        std::function<void(std::string)> on_close_handler;
        void close_session(const boost::system::error_code& ec);

        void handle_incoming_message(const string& msg);
       
    public:
        void start(); //开始对话（仅限发起连接段）
        void on_read_loop();
        Session(io_context &_io, const string& ip_addr, string port, const string& name, const string& hash, Encryptor &encryptor);
        Session(io_context &_io, ip::tcp::socket socket, const string& name, const string& hash, Encryptor &encryptor);
        ip::tcp::socket& get_socket();
        void set_on_close_handler(std::function<void(std::string)> handler) { on_close_handler = handler; }
        
};

class SessionManager{
    private:
        Encryptor &encryptor;
        map<string, shared_ptr<Session>> session_map;
        io_context &io;
    public:
        SessionManager(io_context &_io, Encryptor &encryptor);
        void new_session(const string& ip_addr, const string& port, const string& session_hash, const string& name);
        void new_session_from_socket_and_start(ip::tcp::socket socket, const string& session_hash, const string& name);//从socket创建会话并直接发起会话
        inline io_context& get_io_context() { return io; }
        
};

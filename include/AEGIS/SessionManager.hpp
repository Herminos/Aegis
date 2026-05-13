#pragma once

#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<deque>
#include<functional>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/Utility.hpp>

using namespace boost::asio;
using namespace std;

enum class SessionState{
    IDLE,
    PUBKEY_EXCHANGING,
    ACTIVE,
};

enum class SessionRole{
    CLIENT,
    SERVER
};

struct AETPHeader{
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t payload_length;
    uint16_t crc;
};

class Session:public enable_shared_from_this<Session>
{
    private:
        std::deque<std::vector<uint8_t>> outgoing_package_queue;
        ip::tcp::socket socket;
        ip::tcp::endpoint remote_endpoint;
        SessionState state;//会话状态机
        Encryptor &encryptor; //加密器实例，处理数据加密解密逻辑

        string session_id;

        EphemeralKeyPair my_ephemeral_keypair;
        vector<uint8_t> peer_ephemeral_public_key;
        SessionKeyPair session_key_pair;
        std::vector<uint8_t> build_package_from_payload(const std::vector<uint8_t>& payload, uint8_t type);
        
        boost::asio::awaitable<void> process_encrypted_data_coroutine(std::vector<uint8_t> data);

        std::function<void(std::string)> on_close_handler;
        std::function<void(std::string, std::string)> on_session_promotion_handler;
        
        void send_package(vector<uint8_t> package);
        void start_write_loop();

        void handle_incoming_message(const string& msg);
        void handle_handshaking(vector<uint8_t> handshaking_payload);
       
    public:        
        SessionRole role;
        void start(); //开始对话（仅限发起连接段）
        Session(io_context &_io, const string& ip_addr, string port, const string& id, Encryptor &encryptor);
        Session(io_context &_io, ip::tcp::socket socket, const string& id, Encryptor &encryptor);
        ~Session();
        ip::tcp::socket& get_socket();
        void set_on_close_handler(std::function<void(std::string)> handler) { on_close_handler = handler; }
        void set_on_session_promotion_handler(std::function<void(std::string, std::string)> handler) { on_session_promotion_handler = handler; }
        boost::asio::awaitable<void> start_read_loop_coroutine();
        
        void close_session(const boost::system::error_code& ec);

        void send_message(const string& msg);
        
};

class SessionManager{
    private:
        Encryptor &encryptor;
        string current_session_id;
        map<string, shared_ptr<Session>> session_map;
        io_context &io;
        
    public:
        SessionManager(io_context &_io, Encryptor &encryptor);
        void new_session(const string& ip_addr, const string& port);
        void new_session_from_socket_and_start(ip::tcp::socket socket);//从socket创建会话并直接发起会话
        inline io_context& get_io_context() { return io; }
        std::function<void(std::string)> on_receive_input_handler;
        void set_on_receive_handler(std::function<void(std::string)> handler) {
            on_receive_input_handler = handler;
        }
        void promote_session(const string& tmp_id, const string& actual_id);
        bool if_has_session(const string& id) const {
            return session_map.find(id) != session_map.end();
        }
};        

#pragma once

#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
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
    TERMINATING,
    TERMINATED
};

enum class SessionRole{
    CLIENT,
    SERVER
};

enum AETPPackageType: uint8_t{
    MESSAGE=0x01,
    TERMINATION=0xFF
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
        uint64_t tx_counter=0;
        uint64_t rx_counter=0;

        EphemeralKeyPair my_ephemeral_keypair;
        vector<uint8_t> peer_ephemeral_public_key;
        SessionKeyPair session_key_pair;
        std::vector<uint8_t> build_package_from_payload(const std::vector<uint8_t>& payload, const uint8_t &type);
        
        boost::asio::awaitable<void> process_encrypted_data_coroutine(std::vector<uint8_t> data, std::array<uint8_t, 10> header);

        std::function<void(std::string)> on_close_handler;
        std::function<void(std::string, std::string)> on_session_promotion_handler;
        std::function<void(std::string)> on_session_cleaned_handler;

        void send_message_with_tag(const string& msg, const AETPPackageType &type);
        
        void send_termination_package(string end_message="");
        void send_package(vector<uint8_t> package);
        void start_write_loop();

        void handle_incoming_message(const string& msg);
        void handle_handshaking(vector<uint8_t> handshaking_payload);
       
    public:        
        SessionRole role;
        void start(); //开始对话（仅限发起连接段）
        Session(io_context &_io, const string& ip_addr, string port, const string& id, Encryptor &encryptor, std::function<void(std::string)> on_session_cleaned_handler);
        Session(io_context &_io, ip::tcp::socket socket, const string& id, Encryptor &encryptor, std::function<void(std::string)> on_session_cleaned_handler);
        ~Session();
        ip::tcp::socket& get_socket();
        void set_on_close_handler(std::function<void(std::string)> handler) { on_close_handler = handler; }
        void set_on_session_promotion_handler(std::function<void(std::string, std::string)> handler) { on_session_promotion_handler = handler; }
        void set_on_session_cleaned_handler(std::function<void(std::string)> handler) { on_session_cleaned_handler = handler; }
        boost::asio::awaitable<void> start_read_loop_coroutine();
        
        void close_session(const boost::system::error_code& ec);
        void shutdown_session(const string &end_message);

        void send_message(const string& msg);
        string get_session_addr_and_port() const {
            return remote_endpoint.address().to_string() + ":" + to_string(remote_endpoint.port());
        }
        void clean_session();
        
};

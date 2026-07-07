#pragma once

#include<memory>
#include<boost/asio.hpp>
#include<boost/json.hpp>
#include<vector>
#include<AEGIS/Encryptor.hpp>



class UdpBroadcaster{

    public:
        UdpBroadcaster(boost::asio::io_context &_io, int port=12345);
        void broadcast(const std::string& msg);
        void wait(const boost::system::error_code& e, std::shared_ptr<std::string> shared_msg, short time=1);
        void send_reply(const std::string& msg);
        
        void stop_broadcasting();
        
    private:
        boost::asio::ip::udp::socket socket;
        boost::asio::ip::udp::endpoint remote_endpoint;
        boost::asio::steady_timer timer;
        bool if_still_broadcasting = true;
        
};

class UdpListener{
    private:
        boost::asio::ip::udp::socket socket;
        boost::asio::ip::udp::endpoint remote_endpoint;
        std::vector<char> recv_buffer;
        std::function<void(const std::vector<char>&, const boost::asio::ip::udp::endpoint&)> msg_handler;
    public:
        UdpListener(boost::asio::io_context &io, std::function<void(const std::vector<char>&, const boost::asio::ip::udp::endpoint&)> ,short port);

        void listen();
};

class UdpManager{
    private:
        UdpListener listener;
        UdpBroadcaster broadcaster;
        void on_broadcast_handler(const boost::json::object& msg_obj, const boost::asio::ip::udp::endpoint& sender_ep);
        void on_listened_handler(const std::vector<char>& msg, const boost::asio::ip::udp::endpoint& sender_ep);
        std::function<void(const boost::asio::ip::tcp::endpoint&, const std::string&)> on_session_handler;

        std::string make_reply_content();
        std::string make_broadcast_content();
        bool check_if_AUP(const boost::json::object &obj);
        const std::string my_id;
        const std::string my_tcp_port;
    public:
        UdpManager(boost::asio::io_context& _io, Encryptor &encryptor, const std::string& available_tcp_port, const bool& if_do_udp_broadcast);
        void set_on_session_handler(std::function<void(const boost::asio::ip::tcp::endpoint&, const std::string&)> handler) { on_session_handler = handler; };
        void stop();
};
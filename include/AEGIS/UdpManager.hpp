#pragma once

#include<stdio.h>
#include<memory>
#include<boost/asio.hpp>
#include<boost/json.hpp>
#include<vector>
#include<AEGIS/Encryptor.hpp>

using namespace boost::asio;
using namespace std;


class UdpBroadcaster{

    public:
        UdpBroadcaster(io_context &_io, int port=12345);
        void broadcast(const string& msg);
        void wait(const boost::system::error_code& e, std::shared_ptr<string> shared_msg, short time=1);
        void send_reply(const string& msg);
        
        void stop_broadcasting();
        
    private:
        ip::udp::socket socket;
        ip::udp::endpoint remote_endpoint;
        steady_timer timer;
        bool if_still_broadcasting = true;
        
};

class UdpListener{
    private:
        ip::udp::socket socket;
        ip::udp::endpoint remote_endpoint;
        std::vector<char> recv_buffer;
        function<void(const std::vector<char>&, const ip::udp::endpoint&)> msg_handler;
    public:
        UdpListener(io_context &io, function<void(const std::vector<char>&, const ip::udp::endpoint&)> ,short port);

        void listen();
};

class UdpManager{
    private:
        UdpListener listener;
        UdpBroadcaster broadcaster;
        void on_broadcast_handler(const boost::json::object& msg_obj, const ip::udp::endpoint& sender_ep);
        void on_listened_handler(const std::vector<char>& msg, const ip::udp::endpoint& sender_ep);
        function <void(const ip::tcp::endpoint&, const string&)> on_session_handler;

        string make_reply_content();
        string make_broadcast_content();
        bool check_if_AUP(const boost::json::object &obj);
        const string my_id;
        const string my_tcp_port;
    public:
        UdpManager(io_context& _io, Encryptor &encryptor, const string& available_tcp_port, const bool& if_do_udp_broadcast);
        void set_on_session_handler(function<void(const ip::tcp::endpoint&, const string&)> handler) { on_session_handler = handler; };
        void stop();
};
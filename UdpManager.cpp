#include"UdpManager.hpp"
#include<stdio.h>
#include<memory>
#include<boost/asio.hpp>
#include<vector>
#include<string>

using namespace boost::asio;
using namespace std;


UdpBroadcaster::UdpBroadcaster(io_context &_io, int port) 
    : socket(_io), timer(_io) 
{
    socket.open(ip::udp::v4());
    socket.set_option(socket_base::broadcast(true));
    socket.bind(ip::udp::endpoint(ip::udp::v4(), 0));
    remote_endpoint = ip::udp::endpoint(ip::address_v4::broadcast(), port);
}

void UdpBroadcaster::broadcast(const string& msg) 
{
    if (!if_still_broadcasting) {
        printf("[INFO] Stopped broadcasting.\n");
        socket.close();
        timer.cancel();
        return;
    }

    auto shared_msg = make_shared<string>(msg);

    socket.async_send_to(buffer(*shared_msg), remote_endpoint, 
        [shared_msg, this](const boost::system::error_code& e, std::size_t bytes_transferred) {
            if (e) {
                printf("[ERROR] Error broadcasting: %s\n", e.message().c_str());
                return;
            } else {
                printf("[INFO] Broadcasted %zu bytes\n", bytes_transferred);
                printf("[INFO] Message: %s\n", shared_msg->c_str());
                
                // 调用类成员 wait 继续定时任务
                this->wait(e, shared_msg); 
            }
        }
    );
}


void UdpBroadcaster::wait(const boost::system::error_code& e, std::shared_ptr<string> shared_msg, short time) 
{
    if (e) {
        printf("[ERROR] Timer error: %s\n", e.message().c_str());
        if_still_broadcasting = false;
        return;
    }
    
    timer.expires_after(std::chrono::seconds(time));
    timer.async_wait([this, shared_msg](const boost::system::error_code& e) {
        if (e) {
            printf("[ERROR] Timer error: %s\n", e.message().c_str());
            if_still_broadcasting = false;
            return;
        }
        printf("[INFO] Continuing broadcast...\n");
        this->broadcast(*shared_msg); // 继续广播同一消息
    });
}

void UdpBroadcaster::stop_broadcasting() 
{
    if_still_broadcasting = false;
}


UdpListener::UdpListener(io_context &io, std::function<void(const std::vector<char>&)> msg_handler, int port)
    : socket(io), msg_handler(msg_handler) 
{
    socket.open(ip::udp::v4());
    socket.bind(ip::udp::endpoint(ip::udp::v4(), port));
    recv_buffer.resize(1024);
}

void UdpListener::listen() 
{
    socket.async_receive_from(buffer(recv_buffer), remote_endpoint, 
        [this](const boost::system::error_code& e, std::size_t bytes_transferred) {
            if (e) {
                printf("[ERROR] Error receiving: %s\n", e.message().c_str());
                return;
            } else {
                printf("[INFO] Received %zu bytes from %s:%d\n", 
                       bytes_transferred, 
                       remote_endpoint.address().to_string().c_str(), 
                       remote_endpoint.port());
                
                msg_handler(recv_buffer); // 调用消息处理函数
                this->listen();           // 递归调用，继续监听下一条
            }
        }
    );
}
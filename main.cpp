#include<stdio.h>
#include<functional>
#include<thread>
#include"UdpManager.hpp"
#include<boost/asio.hpp>

using namespace boost::asio;

/*
int main() {
    io_context io;
    UdpBroadcaster broadcaster(io);
    broadcaster.broadcast("Hello, network!");

    UdpListener listener(io, [](const std::vector<char>& msg) {
        printf("[RECEIVED] Received message: %s\n", std::string(msg.begin(), msg.end()).c_str());
    });
    listener.listen();

    try{
        io.run();
    } catch (const std::exception& e) {
        printf("[ERROR] Exception in io_context: %s\n", e.what());
    }
}*/

int true_main(){


    /*
    新建udp广播接收
    收到广播：
    通知SessionManager新建Session实例
        if 我是发送者
            发送
        if 我是接收者
            接受
        如果五秒内未连接，超时处理
    
    
    */
}
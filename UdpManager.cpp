#include"UdpManager.hpp"
#include"Utility.hpp"
#include<stdio.h>
#include<memory>
#include<boost/asio.hpp>
#include<boost/json.hpp>
#include<vector>
#include<string>

using namespace boost::asio;
namespace json = boost::json;
using namespace std;




UdpBroadcaster::UdpBroadcaster(io_context &_io, int port) 
    : socket(_io), timer(_io) 
{
    socket.open(ip::udp::v4());
    socket.set_option(socket_base::broadcast(true));
    remote_endpoint = ip::udp::endpoint(ip::address_v4::broadcast(), port);
    //socket.bind(ip::udp::endpoint(ip::udp::v4(), port));
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


UdpListener::UdpListener(io_context &io, std::function<void(const std::vector<char>&)> msg_handler, short port)
    : socket(io), msg_handler(msg_handler) 
{
    socket.open(ip::udp::v4());
    socket.bind(ip::udp::endpoint(ip::udp::v4(), port));
    recv_buffer.resize(1024);
    socket.set_option(socket_base::reuse_address(true));
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
                
                msg_handler(
                    std::vector<char>(recv_buffer.begin(), recv_buffer.begin() + bytes_transferred)
                ); // 调用消息处理函数
                this->listen();           // 递归调用，继续监听下一条
            }
        }
    );
}

void hello_msg_handler(const std::vector<char>& msg){
    
    try{
        json::value val=json::parse(json::string_view(msg.data(), msg.size()));
        json::object obj=val.as_object();
        if(auto app=obj.if_contains("app")){
            if(obj.at["app"].as_string()!="AEGIS")
                return;
        }


    }
    catch(exception e){

    }
    
}

UdpManager::UdpManager(io_context& _io, std::function<void(const std::vector<char>&)> msg_handler, Encryptor &encryptor)
  :  broadcaster(_io, 12345), listener(_io, msg_handler,12345)
{
    string hash="0x00000000";
    //hash=encryptor.get_hash();
    broadcaster.broadcast(make_hello_content(hash, "12345"));
    listener.listen();

};

string make_hello_content(const string& hash, const string& tcp_port){
    boost::json::object obj;
    obj["app"]="AEGIS";
    obj["ver"]=CURRENT_VERSION;
    obj["type"]="HELLO";
    obj["hash"]=hash;
    obj["port"]=tcp_port;
    return boost::json::serialize(obj);
};
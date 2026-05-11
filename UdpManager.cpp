#include"UdpManager.hpp"
#include"Utility.hpp"
#include<stdio.h>
#include<memory>
#include<boost/asio.hpp>
#include<boost/json/src.hpp>
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

void UdpBroadcaster::send_reply(const ip::udp::endpoint& target_ep, const string& msg) {

    auto shared_msg = make_shared<string>(msg);

    socket.async_send_to(
        buffer(*shared_msg), target_ep,
        [shared_msg, target_ep](const boost::system::error_code& e, std::size_t bytes_transferred) {
            if(e){
                printf("[ERROR] Error broadcasting: %s\n", e.message().c_str());
                return;
            }
            printf("[INFO] Broadcasted %zu bytes\n", bytes_transferred);
            printf("[INFO] Message: %s\n", shared_msg->c_str());
        }

        
    );
    
}


UdpListener::UdpListener(io_context &io, function<void(const std::vector<char>&, const ip::udp::endpoint&)> msg_handler ,short port)
    : socket(io), msg_handler(msg_handler)
{
    socket.open(ip::udp::v4());
    socket.set_option(socket_base::reuse_address(true));
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
                
                msg_handler(
                    std::vector<char>(recv_buffer.begin(), recv_buffer.begin() + bytes_transferred),
                    remote_endpoint
                ); // 调用消息处理函数
                this->listen();
            }
        }
    );
}


string UdpManager::make_broadcast_content(){
    boost::json::object obj;
    obj["app"]="AEGIS";
    obj["ver"]=CURRENT_VERSION;
    obj["type"]="BROADCAST";
    obj["hash"]=my_hash;
    obj["port"]=my_tcp_port;
    obj["name"]=my_name;
    return boost::json::serialize(obj);
};

UdpManager::UdpManager(io_context& _io, Encryptor &encryptor, const string& name, const string& available_tcp_port)
  :  broadcaster(_io, 12345), listener(_io,
    [this](const std::vector<char>& msg, const ip::udp::endpoint& sender) {
        this->on_listened_handler(msg, sender);
    }
    ,12345), my_hash(encryptor.get_hash()), my_name(name), my_tcp_port(available_tcp_port)
{
    broadcaster.broadcast(make_broadcast_content());
    listener.listen();

};

bool UdpManager::check_if_AUP(const boost::json::object& obj) {

    auto app = obj.if_contains("app");
    if (!app || !app->is_string() || app->as_string() != "AEGIS") return false;
    auto ver = obj.if_contains("ver");
    if (!ver || !ver->is_string() || ver->as_string() != CURRENT_VERSION) return false;
    if (!obj.contains("type") || !obj.contains("hash") || !obj.contains("port")) return false;
    if (obj.at("hash").as_string() == my_hash) return false; 

    return true;
}

string UdpManager::make_reply_content(){

    boost::json::object obj;
    obj["app"]="AEGIS";
    obj["ver"]=CURRENT_VERSION;
    obj["port"]=my_tcp_port;
    obj["type"]="REPLY";
    obj["hash"]=my_hash;
    obj["name"]=my_name;
    return boost::json::serialize(obj);

};

void UdpManager::on_listened_handler(const std::vector<char>& msg, const ip::udp::endpoint& sender_ep) {

    try{
        boost::json::value val=boost::json::parse(string_view(msg.data(), msg.size()));
        const boost::json::object obj=val.as_object();
        if(!check_if_AUP(obj)) return;

        if(obj.at("type")=="BROADCAST"){
            this->on_broadcast_handler(obj, sender_ep);
        }
        else if(obj.at("type")=="REPLY"){
            this->on_session_handler(
                ip::tcp::endpoint(sender_ep.address(), stoi(obj.at("port").as_string().c_str()))
                , obj.at("hash").as_string().c_str(), obj.at("name").as_string().c_str());
        }


    }
    catch(const exception& e){
        printf("[ERROR]  %s\n", e.what());
    };

}

void UdpManager::on_broadcast_handler(const boost::json::object& msg_obj, const ip::udp::endpoint& sender_ep) {

    if(std::string_view(msg_obj.at("hash").as_string()) > my_hash){//此时对方哈希值比我方大，对方为发送方，我方需REPLY让对方创建连接
        this->broadcaster.send_reply(sender_ep, make_reply_content());
    }
    else{//我方哈希值大于对方，我方创建连接
        this->on_session_handler(
                ip::tcp::endpoint(sender_ep.address(), stoi(msg_obj.at("port").as_string().c_str()))
                , msg_obj.at("hash").as_string().c_str(), msg_obj.at("name").as_string().c_str());
        
    }


};

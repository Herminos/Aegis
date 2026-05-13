#include<AEGIS/UdpManager.hpp>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/Utility.hpp>
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
        log_info(string("[INFO] Stopped broadcasting."));
        socket.close();
        timer.cancel();
        return;
    }

    auto shared_msg = make_shared<string>(msg);

    socket.async_send_to(buffer(*shared_msg), remote_endpoint, 
        [shared_msg, this](const boost::system::error_code& e, std::size_t bytes_transferred) {
            if (e) {
                log_error(string("[ERROR] Error broadcasting: ") + e.message());
                return;
            } else {
                log_info(string("[INFO] Broadcasted %zu bytes") + to_string(bytes_transferred));
                log_info(string("[INFO] Message: ") + *shared_msg);

                // 调用类成员 wait 继续定时任务
                this->wait(e, shared_msg); 
            }
        }
    );
}


void UdpBroadcaster::wait(const boost::system::error_code& e, std::shared_ptr<string> shared_msg, short time) 
{
    if (e) {
        log_error(string("[ERROR] Timer error: ") + e.message());
        if_still_broadcasting = false;
        return;
    }
    
    timer.expires_after(std::chrono::seconds(time));
    timer.async_wait([this, shared_msg](const boost::system::error_code& e) {
        if (e) {
            log_error(string("[ERROR] Timer error: ") + e.message());
            if_still_broadcasting = false;
            return;
        }
        log_info(string("[INFO] Continuing broadcast..."));
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
                log_error(string("[ERROR] Error sending reply: ") + e.message());
                return;
            }
            log_info(string("[INFO] Reply sent %zu bytes to %s:%d") + to_string(bytes_transferred) + target_ep.address().to_string() + to_string(target_ep.port()));
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
                log_error(string("[ERROR] Error receiving UDP message: ") + e.message());
                return;
            } else {
                log_info(string("[INFO] Received %zu bytes from %s:%d") + to_string(bytes_transferred) + remote_endpoint.address().to_string() + to_string(remote_endpoint.port()));
                
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
    obj["app"]="AUP";
    obj["ver"]=CURRENT_VERSION;
    obj["type"]="BROADCAST";
    obj["id"]=my_id;
    obj["port"]=my_tcp_port;
    return boost::json::serialize(obj);
};

UdpManager::UdpManager(io_context& _io, Encryptor &encryptor, const string& available_tcp_port)
  :  broadcaster(_io, 12345), listener(_io,
    [this](const std::vector<char>& msg, const ip::udp::endpoint& sender) {
        this->on_listened_handler(msg, sender);
    }
    ,12345), my_id(encryptor.get_id()), my_tcp_port(available_tcp_port)
{
    broadcaster.broadcast(make_broadcast_content());
    listener.listen();

};

bool UdpManager::check_if_AUP(const boost::json::object& obj) {

    auto app = obj.if_contains("app");
    if (!app || !app->is_string() || app->as_string() != "AUP") return false;
    auto ver = obj.if_contains("ver");
    if (!ver || !ver->is_string() || ver->as_string() != CURRENT_VERSION) return false;
    if (!obj.contains("type") || !obj.contains("id") || !obj.contains("port")) return false;
    if (obj.at("id").as_string() == my_id) return false; 
    log_info(string("[INFO] Received valid AUP message from ") + obj.at("id").as_string().c_str() + ":" + obj.at("port").as_string().c_str());
    return true;
}

string UdpManager::make_reply_content(){

    boost::json::object obj;
    obj["app"]="AUP";
    obj["ver"]=CURRENT_VERSION;
    obj["port"]=my_tcp_port;
    obj["type"]="REPLY";
    obj["id"]=my_id;
    return boost::json::serialize(obj);

};

void UdpManager::on_listened_handler(const std::vector<char>& msg, const ip::udp::endpoint& sender_ep) {

    try{
        boost::json::value val=boost::json::parse(string_view(msg.data(), msg.size()));
        const boost::json::object obj=val.as_object();
        if(!check_if_AUP(obj)){
            //log_info(string("[INFO] Received non-AUP or malformed message from ") + sender_ep.address().to_string() + ":" + to_string(sender_ep.port()));
            return;
        }

        if(obj.at("type")=="BROADCAST"){
            log_info(string("[INFO] Received BROADCAST from ") + obj.at("id").as_string().c_str() + ":" + obj.at("port").as_string().c_str());
            if(obj.at("id").as_string() == my_id) {
                log_info(string("[INFO] Ignoring own BROADCAST message."));
                return;
            } //过滤掉自己发来的广播

            if(obj.at("id").as_string() > my_id) {
                log_info(string("[INFO] Received BROADCAST with larger ID from ") + obj.at("id").as_string().c_str() + ":" + obj.at("port").as_string().c_str() + ", ignoring...");
                return;
            };//我方为接收方，等待对方回复创建连接
            
            this->on_broadcast_handler(obj, sender_ep);
        }
        else if(obj.at("type")=="REPLY"){
            log_info(string("[INFO] Received REPLY from ") + obj.at("id").as_string().c_str() + ":" + obj.at("port").as_string().c_str() + ", creating session...");
            this->on_session_handler(
                ip::tcp::endpoint(sender_ep.address(), stoi(obj.at("port").as_string().c_str())),
                obj.at("id").as_string().c_str()
            );
        }


    }
    catch(const exception& e){
        log_error(string("[ERROR] Failed to parse UDP message: ") + e.what());
    };

}

void UdpManager::on_broadcast_handler(const boost::json::object& msg_obj, const ip::udp::endpoint& sender_ep) {

    if(std::string_view(msg_obj.at("id").as_string()) > my_id){//此时对方哈希值比我方大，对方为发送方，我方需REPLY让对方创建连接
        log_info(string("[INFO] Received BROADCAST with smaller ID from ") + msg_obj.at("id").as_string().c_str() + ":" + msg_obj.at("port").as_string().c_str() + ", sending REPLY...");
        ip::udp::endpoint target_ep(sender_ep.address(), 12345);
        this->broadcaster.send_reply(target_ep, make_reply_content());
    }
    else{//我方哈希值大于对方，我方创建连接
        this->on_session_handler(
                ip::tcp::endpoint(sender_ep.address(), stoi(msg_obj.at("port").as_string().c_str()))
                , msg_obj.at("id").as_string().c_str());
        
    }


};

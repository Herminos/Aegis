#include<stdio.h>
#include<functional>
#include<thread>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/SessionManager.hpp>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/UdpManager.hpp>
#include<boost/asio.hpp>
#include<boost/json/src.hpp>

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

int main(){

    io_context io;
    thread_pool pool;
    string my_name="Herminos";
    Encryptor encryptor(pool);
    SessionManager session_manager(io, encryptor);
    
    UdpManager udp_manager(io, encryptor, my_name, "12345");

    const string my_hash="0x12345678";
    //my_hash=encryptor.get_hash();


    udp_manager.set_on_session_handler(
        [&session_manager, &io](const ip::tcp::endpoint& remote_endpoint, const string& hash, const string& name) {
            
            session_manager.new_session(
                remote_endpoint.address().to_string(),
                to_string(remote_endpoint.port()),
                hash,
                name
            );
        }

    );

}
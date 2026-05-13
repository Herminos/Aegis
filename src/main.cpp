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

int main(int argc, char* argv[]) {

    io_context io;
    thread_pool pool;
    string my_name="Herminos";
    Encryptor encryptor(pool);
    SessionManager session_manager(io, encryptor);
    
    

    if(argc <= 1) {
        printf("Usage: %s <available_tcp_port>\n", argv[0]);
        return 1;
    }
    string available_tcp_port=argv[1];
    my_name += "_"+available_tcp_port;
    UdpManager udp_manager(io, encryptor, my_name, available_tcp_port);

    TcpSender tcp_sender(io, stoi(available_tcp_port));

    tcp_sender.set_accept_handler(
        [&session_manager, &io](ip::tcp::socket peer_socket) {
            session_manager.new_session_from_socket_and_start(move(peer_socket), "", "Unknown");
        }
    );
    tcp_sender.accept();

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
    try{
        io.run();
    } catch (const std::exception& e) {
        printf("[ERROR] Exception in io_context: %s\n", e.what());
    }
    pool.join();
    system("pause");
}
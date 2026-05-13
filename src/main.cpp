#include<stdio.h>
#include<functional>
#include<thread>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/SessionManager.hpp>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/UdpManager.hpp>
#include<AEGIS/InputManager.hpp>
#include<boost/asio.hpp>
#include<boost/json/src.hpp>

using namespace boost::asio;

int main(int argc, char* argv[]) {

    io_context io;
    thread_pool pool;
    Encryptor encryptor(pool);
    SessionManager session_manager(io, encryptor);
    
    

    if(argc <= 1) {
        printf("Usage: %s <available_tcp_port>\n", argv[0]);
        return 1;
    }
    string available_tcp_port=argv[1];

    UdpManager udp_manager(io, encryptor, available_tcp_port);
    TcpSender tcp_sender(io, stoi(available_tcp_port));
    InputManager input_manager;

    tcp_sender.set_accept_handler(
        [&session_manager, &io](ip::tcp::socket peer_socket) {
            session_manager.new_session_from_socket_and_start(move(peer_socket));
        }
    );
    tcp_sender.accept();

    udp_manager.set_on_session_handler(
        [&session_manager, &io](const ip::tcp::endpoint& remote_endpoint, const string& hash) {

            if(session_manager.if_has_session(hash)) {
                log_info(string("[INFO] Session with id ") + hash + " already exists. Skipping session creation.");
                return;
            }
            
            session_manager.new_session(
                remote_endpoint.address().to_string(),
                to_string(remote_endpoint.port())
            );
        }
    );

    input_manager.set_message_sent_callback([session_manager](std::string data) {
        session_manager.on_receive_input_handler(data);
    });

    input_manager.start_input_loop();
    
    try{
        io.run();
    } catch (const std::exception& e) {
        log_error(string("[ERROR] Exception in main io_context: ") + e.what());
    }
    pool.join();
    system("pause");
}
#include<stdio.h>
#include<functional>
#include<thread>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/SessionManager.hpp>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/UdpManager.hpp>
#include<AEGIS/InputManager.hpp>
#include<AEGIS/CommandRouter.hpp>

using namespace boost::asio;

const string banner=R"(
    ___    ______ ______ ____ _____
   /   |  / ____// ____//  _// ___/
  / /| | / __/  / / __  / /  \__ \
 / ___ |/ /___ / /_/ /_/ /  ___/ / 
/_/  |_/_____/ \____//___/ /____/ 
                                     
)";

void print_banner(const string& id) {
    printf("%s\n", banner.c_str());
    printf("AEGIS - A P2P and E2EE communication tool\n");
    printf("Version: %s\n", CURRENT_VERSION);
    printf("Node ID: %s\n", id.c_str());
    printf("AEGIS initialized successfully. Waiting for connections...\n");
}

int main(int argc, char* argv[]) {
    bool if_do_udp_broadcast=true;
    io_context io;
    thread_pool pool;
    Encryptor encryptor(pool);
    SessionManager session_manager(io, encryptor);
    
    string available_tcp_port;

    if(argc <= 1) {
        printf("Usage: %s <available_tcp_port>\n", argv[0]);
        return 1;
    }
    available_tcp_port=argv[1];

    UdpManager udp_manager(io, encryptor, available_tcp_port, if_do_udp_broadcast);
    TcpSender tcp_sender(io, stoi(available_tcp_port));
    InputManager input_manager;
    CommandRouter command_router;

    print_banner(encryptor.get_id());
    tcp_sender.set_accept_handler(
        [&session_manager, &io](ip::tcp::socket peer_socket) {
            session_manager.new_session_from_socket_and_start(move(peer_socket));
        }
    );
    tcp_sender.accept();

    udp_manager.set_on_session_handler(
        [&session_manager, &io](const ip::tcp::endpoint& remote_endpoint, const string& hash) {
            if(session_manager.is_tombstoned(hash)) {
                log_info(string("[INFO] Received session request with id ") + hash + " which is tombstoned. Ignoring.");
                return;
            }

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

    input_manager.set_message_sent_callback([&command_router](const std::string& raw_input) {
        command_router.handle_raw_input(raw_input);
    });

    command_router.set_send_message_handler(session_manager.on_send_message_handler);
    command_router.set_exit_aegis_handler(
        [&session_manager, &udp_manager](){
            udp_manager.stop();
            session_manager.exit_aegis_handler("Fuck you, I'm leaving.");
        }

    );
    command_router.set_list_all_sessions_handler(session_manager.list_all_sessions_handler);
    command_router.set_list_current_session_handler(session_manager.list_current_session_handler);
    

    input_manager.start_input_loop();
    
    try{
        io.run();
    } catch (const std::exception& e) {
        log_error(string("[ERROR] Exception in main io_context: ") + e.what());
    }
    
    pool.join();
    system("pause");
}
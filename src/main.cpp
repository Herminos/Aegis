#include<stdio.h>
#include<functional>
#include<string>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/SessionManager.hpp>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/UdpManager.hpp>
#include<AEGIS/InputManager.hpp>
#include<AEGIS/CommandRouter.hpp>
#include<AEGIS/ArgParser.hpp>

using namespace boost::asio;
using namespace std;

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

    AegisConfig config=ArgParser::parse(argc, argv);
    if(config.is_vaild==false) {
        return 1;
    }
    
    io_context io;
    thread_pool pool;
    Encryptor encryptor(pool);
    SessionManager session_manager(io, encryptor);
    bool if_do_udp_broadcast=(config.listen_host=="0.0.0.0");
    
    TcpSender tcp_sender(io, config.listen_port, config.listen_host);

    unsigned short actual_port = tcp_sender.get_listen_port();
    string available_tcp_port = to_string(actual_port);
    UdpManager udp_manager(io, encryptor, available_tcp_port, if_do_udp_broadcast);
    
    
    InputManager input_manager;
    CommandRouter command_router;

    print_banner(encryptor.get_id());
    tcp_sender.set_accept_handler(
        [&session_manager, &io](ip::tcp::socket peer_socket) {
            session_manager.new_session_from_socket_and_start(std::move(peer_socket));
        }
    );
    if(config.is_client==true){
        print_info("Client Mode, UDP Broadcast is disabled.");
        print_info("Establishing connection with server...");
        session_manager.new_session(config.connect_host, to_string(config.connect_port));
    }
    else if(config.listen_host=="127.0.0.1"){
        print_info("Ghost Tunnel Mode, UDP Broadcast is disabled.");
        print_info("Make sure you have enabled SSH Reversing on your router.");
    }else{
        print_info("LAN Mode, UDP Broadcast is enabled.");
    }
    if(config.is_client == false)
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
#include<stdio.h>
#include<iostream>
#include<functional>
#include<string>
#include<thread>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/SessionManager.hpp>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/UdpManager.hpp>
#include<AEGIS/ArgParser.hpp>
#include<AEGIS/UiDispatcher.hpp>
#include<UI/AegisEngine.hpp>
#include<UI/AegisUserInterface.hpp>

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

// ============================================================
// ConsoleUI
//   引擎 → UI：on_*  虚函数（显示到终端）
//   UI → 引擎：input_loop 线程读取 stdin，调用 engine 方法
// ============================================================
class ConsoleUI : public AegisUserInterface {
public:
    explicit ConsoleUI(AegisEngine& _engine) : AegisUserInterface(_engine) {
        input_thread = std::thread([this]() { input_loop(); });
    }

    ~ConsoleUI() override {
        running = false;
        if (input_thread.joinable()) {
            input_thread.detach(); // getline 阻塞中，detach 让进程退出时清理
        }
    }

    // ========== 引擎 → UI ==========

    void on_banner(std::string node_id, std::string version) override {
        printf("\n%s\n", banner.c_str());
        printf("AEGIS - A P2P and E2EE communication tool\n");
        printf("Version: %s\n", version.c_str());
        printf("Node ID: %s\n", node_id.c_str());
    }
    void on_message_received(std::string session_id, std::string msg) override {
        printf("\n[From %s]: %s\n", session_id.c_str(), msg.c_str());
    }
    void on_message_sent(std::string session_id, std::string msg) override {
        printf("\n[To %s]: %s\n", session_id.c_str(), msg.c_str());
    }
    void on_session_active(std::string session_id, std::string addr_port) override {
        printf("\n[INFO] Session active: [%s] @ %s\n", session_id.c_str(), addr_port.c_str());
    }
    void on_session_closed(std::string session_id, std::string reason) override {
        printf("\n[WARN] Session closed: [%s] — %s\n", session_id.c_str(), reason.c_str());
    }
    void on_session_terminated(std::string session_id, std::string msg) override {
        printf("\n[INFO] Session terminated: [%s] — %s\n", session_id.c_str(), msg.c_str());
    }
    void on_session_list(std::string listing) override {
        printf("\n%s\n", listing.c_str());
    }
    void on_info(std::string msg) override {
        printf("\n%s\n", msg.c_str());
    }
    void on_warning(std::string msg) override {
        printf("\n%s%s%s\n", YELLOW.c_str(), msg.c_str(), RESET.c_str());
    }
    void on_error(std::string msg) override {
        printf("\n%s%s%s\n", RED.c_str(), msg.c_str(), RESET.c_str());
    }
    void on_success(std::string msg) override {
        printf("\n%s%s%s\n", GREEN.c_str(), msg.c_str(), RESET.c_str());
    }

private:
    // ========== UI → 引擎 ==========
    std::thread input_thread;
    std::atomic<bool> running{true};

    void input_loop() {
        std::string line;
        while (running && std::getline(std::cin, line)) {
            if (line.empty()) continue;
            if (line[0] == '/') {
                engine.exec_command(line);
            } else {
                engine.send_message(line);
            }
        }

        
    }
};

int main(int argc, char* argv[]) {

    setbuf(stdout, NULL);  // stdout 无缓冲，确保消息即时显示

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

    // ============================================================
    // UI 层搭建
    //   AegisEngine     = UI → 引擎
    //   UiDispatcher    = 线程安全桥接
    //   ConsoleUI       = 引擎→UI 虚接口实现 + stdin 输入循环
    // ============================================================
    io_context ui_io;
    AegisEngine engine;
    engine.session_manager = &session_manager;
    engine.udp_manager = &udp_manager;

    auto console_ui = std::make_unique<ConsoleUI>(engine);
    UiDispatcher ui_dispatcher(ui_io, std::move(console_ui));

    // SessionManager 引擎事件 → 直接桥接到 UiDispatcher
    session_manager.on_message_received = [&ui_dispatcher](std::string sid, std::string msg) {
        ui_dispatcher.on_message_received(std::move(sid), std::move(msg));
    };
    session_manager.on_message_sent = [&ui_dispatcher](std::string sid, std::string msg) {
        ui_dispatcher.on_message_sent(std::move(sid), std::move(msg));
    };
    session_manager.on_session_active = [&ui_dispatcher](std::string sid, std::string addr) {
        ui_dispatcher.on_session_active(std::move(sid), std::move(addr));
    };
    session_manager.on_session_closed = [&ui_dispatcher](std::string sid, std::string reason) {
        ui_dispatcher.on_session_closed(std::move(sid), std::move(reason));
    };
    session_manager.on_session_terminated = [&ui_dispatcher](std::string sid, std::string msg) {
        ui_dispatcher.on_session_terminated(std::move(sid), std::move(msg));
    };
    session_manager.on_session_list = [&ui_dispatcher](std::string listing) {
        ui_dispatcher.on_session_list(std::move(listing));
    };
    session_manager.on_info = [&ui_dispatcher](std::string msg) {
        ui_dispatcher.on_info(std::move(msg));
    };
    session_manager.on_warning = [&ui_dispatcher](std::string msg) {
        ui_dispatcher.on_warning(std::move(msg));
    };
    session_manager.on_error = [&ui_dispatcher](std::string msg) {
        ui_dispatcher.on_error(std::move(msg));
    };

    // ============================================================
    // 网络层搭建
    // ============================================================
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

    try{
        io.run();
    } catch (const std::exception& e) {
        log_error(string("[ERROR] Exception in main io_context: ") + e.what());
    }

    pool.join();
    system("pause");
}

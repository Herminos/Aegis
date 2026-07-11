#include <UI/AegisEngine.hpp>
#include <AEGIS/SessionManager.hpp>
#include <AEGIS/UdpManager.hpp>
#include <AEGIS/Utility.hpp>

// ============================================================
// UI → 引擎
// ============================================================

void AegisEngine::send_message(std::string msg, std::string session_id) {
    if (!session_manager) return;
    session_manager->send_message(msg, session_id);
}

void AegisEngine::switch_session(std::string session_id) {
    if (!session_manager) return;
    session_manager->switch_session(session_id);
}

void AegisEngine::exec_command(std::string command) {
    if (command.empty() || !session_manager) return;

    if (command[0] == '/') command = command.substr(1);

    if (command == "list") {
        session_manager->on_list_sessions();
        return;
    }

    if (command == "current_session" || command == "current") {
        session_manager->on_show_current_session();
        return;
    }

    if (command == "exit" || command == "quit") {
        if (udp_manager) udp_manager->stop();
        session_manager->shutdown_all("User requested exit.");
        return;
    }

    if (command == "help") {
        print_info("/list             — List all sessions\n"
                   "/current_session  — Show current session\n"
                   "/exit             — Exit AEGIS\n"
                   "/help             — Show this help");
        return;
    }

    print_info(std::string("Unknown command: /") + command + ". Try /help.");
}

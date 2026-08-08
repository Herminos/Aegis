#include <UI/AegisEngine.hpp>
#include <AEGIS/SessionManager.hpp>
#include <AEGIS/UdpManager.hpp>
#include <AEGIS/Encryptor.hpp>
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

    if (command.rfind("seed load ", 0) == 0) {
        if (!encryptor) { log_error("Encryptor not available."); exit(1); }
        std::string path = command.substr(10); // skip "seed load "
        try {
            encryptor->load_seed_from_file(path);
            print_info("Seed loaded. New Node ID: " + encryptor->get_id());
        } catch (const std::exception& e) {
            print_info(std::string("Load seed failed: ") + e.what());
        }
        return;
    }

    if (command.rfind("seed save ", 0) == 0) {
        if (!encryptor) { log_error("Encryptor not available."); exit(1); }
        std::string path = command.substr(10); // skip "seed save "
        try {
            encryptor->save_seed_to_file(path);
            print_info("Seed saved. New Node ID: " + encryptor->get_id());
        } catch (const std::exception& e) {
            print_info(std::string("Save seed failed: ") + e.what());
        }
        return;
    }

    if (command == "seed" || command == "id") {
        if (encryptor) print_info("Node ID: " + encryptor->get_id());
        return;
    }

    if (command == "help") {
        print_info("/list             — List all sessions\n"
                   "/current_session  — Show current session\n"
                   "/seed load <path> — Load identity seed from file\n"
                   "/seed save <path> — Generate & save identity seed to file\n"
                   "/id               — Show current Node ID\n"
                   "/exit             — Exit AEGIS\n"
                   "/help             — Show this help");
        return;
    }

    print_info(std::string("Unknown command: /") + command + ". Try /help.");
}

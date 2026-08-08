#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/SessionManager.hpp>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/Utility.hpp>
#include<AEGIS/Session.hpp>

using namespace boost::asio;
using namespace std;


SessionManager::SessionManager(io_context &_io, Encryptor &encryptor) : io(_io) , encryptor(encryptor) {

};

// ============================================================
// UI → 引擎
// ============================================================

void SessionManager::send_message(const string& msg, const string& session_id) {
    string target_id = session_id.empty() ? current_session_id : session_id;

    boost::asio::post(io, [this, msg, target_id]() {
        if (target_id.empty()) {
            if (on_warning) on_warning("No active session.");
            else print_info("No active session.");
            return;
        }
        auto it = session_map.find(target_id);
        if (it == session_map.end()) {
            if (on_warning) on_warning("Session not found: " + target_id);
            else print_info("Session not found: " + target_id);
            return;
        }
        it->second->send_message(msg);
        if (on_message_sent) on_message_sent(target_id, msg);
        else print_info("\r[To " + target_id + "]: " + msg);
    });
}

void SessionManager::switch_session(const string& id) {
    if (id.empty()) return;
    auto it = session_map.find(id);
    if (it == session_map.end()) {
        if (on_warning) on_warning("Cannot switch: session not found: " + id);
        else print_info("Cannot switch: session not found: " + id);
        return;
    }
    current_session_id = id;
    if (on_info) on_info("Switched to session: " + id);
    else print_info("Switched to session: " + id);
}

void SessionManager::shutdown_all(const string& end_message) {
    for (const auto& [id, session] : session_map) {
        session->shutdown_session(end_message);
    }
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        io.stop();
    }).detach();
}

// ============================================================
// 查询方法
// ============================================================

string SessionManager::get_current_session_id() const {
    return current_session_id;
}

string SessionManager::get_all_sessions_info() const {
    if (session_map.empty()) {
        return "No active sessions.";
    }
    string result;
    for (const auto& [id, session] : session_map) {
        string marker = (id == current_session_id) ? " * " : "   ";
        result += marker + "[" + id + "] " + session->get_session_addr_and_port() + "\n";
    }
    result += "\n(*) = current session";
    return result;
}

bool SessionManager::is_tombstoned(const std::string& id) {
    auto it = tombstoned_ids.find(id);
    if (it != tombstoned_ids.end()) {
        auto tombstone_time = it->second;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - tombstone_time).count() < 10) {
            return true;
        } else {
            tombstoned_ids.erase(it);
            return false;
        }
    }
    return false;
}

string SessionManager::get_current_session_info() const {
    if (current_session_id.empty()) {
        return "No current session selected.";
    }
    auto it = session_map.find(current_session_id);
    if (it == session_map.end()) {
        return "Current session not found in session map.";
    }
    return "Current Session: [" + current_session_id + "] "
           + it->second->get_session_addr_and_port();
}

void SessionManager::on_list_sessions() {
    std::string info = get_all_sessions_info();
    if (on_session_list) {
        on_session_list(info);
    } else {
        print_info(info);
    }
}

void SessionManager::on_show_current_session() {
    std::string info = get_current_session_info();
    if (on_info) {
        on_info(info);
    } else {
        print_info(info);
    }
}

void SessionManager::new_session(const string& ip_addr, const string& port) {

    string map_key = "TEMP_ID_" + ip_addr + ":" + port;

    if(session_map.find(map_key) != session_map.end()) {
        log_info(string("[INFO] Session with id ") + map_key + " already exists. Skipping.");
        return;
    }

    auto session=make_shared<Session>(io, ip_addr, port, map_key, encryptor, on_session_cleaned_handler);
    
    session->set_on_close_handler([this](const string& id) {
        boost::asio::post(io, [this, id](){
            size_t erased = session_map.erase(id);
            if (erased > 0) {
                log_info(string("[INFO] Session with id ") + id + " removed from session manager.");
            } else {
                log_warning(string("[WARN] Attempted to remove session with id ") + id + ", but it was not found in session manager.");
            }
            this->current_session_id = "";
        });
    });

    session->set_on_session_promotion_handler([this](const string& tmp_id, const string& actual_id) {
        promote_session(tmp_id, actual_id);
    });

    // 注入消息接收回调：Session → SessionManager → UiDispatcher → UI
    session->set_on_message_received_handler([this](const string& sid, const string& msg) {
        if (on_message_received) {
            on_message_received(sid, msg);
        } else {
            print_info(string("\r\n[From " + sid + "]: ") + msg);
        }
    });

    session->role = SessionRole::CLIENT;
    session_map[map_key]=session;
    session->start(); //start the session

}

void SessionManager::new_session_from_socket_and_start(ip::tcp::socket &&socket, SessionRole role) {

    string map_key = "TEMP_ID_" + socket.remote_endpoint().address().to_string() + ":" + to_string(socket.remote_endpoint().port());

    if(session_map.find(map_key) != session_map.end()) {
        log_info(string("[INFO] Session with id ") + map_key + " already exists. Skipping.");
        return;
    }
    auto session = make_shared<Session>(io, std::move(socket), map_key, encryptor, on_session_cleaned_handler);
    session->role = role;
    
    session->set_on_close_handler([this](const string& id) {
        boost::asio::post(io, [this, id](){
            size_t erased = session_map.erase(id);
            if (erased > 0) {
                log_info(string("[INFO] Session with id ") + id + " removed from session manager.");
            } else {
                log_warning(string("[WARN] Attempted to remove session id ") + id + ", but not found.");
            }
            this->current_session_id = "";
        });
    });

    session->set_on_session_promotion_handler([this](const string& tmp_id, const string& actual_id) {
        promote_session(tmp_id, actual_id);
    });

    // 注入消息接收回调：Session → SessionManager → UiDispatcher → UI
    session->set_on_message_received_handler([this](const string& sid, const string& msg) {
        if (on_message_received) {
            on_message_received(sid, msg);
        } else {
            print_info(string("\r\n[From " + sid + "]: ") + msg);
        }
    });

    current_session_id = map_key;
    session_map[map_key] = session;

    // 2. 加上异常护盾的协程启动器
    boost::asio::co_spawn(
        io,
        [session]() mutable -> boost::asio::awaitable<void> {
            try {
                co_await session->start_read_loop_coroutine();
            } 
            catch (const std::exception& e) {
                log_error(string("[ERROR] Session connection violently broken: ") + e.what());
                session->close_session(boost::asio::error::connection_aborted); 
            }
        },
        boost::asio::detached
    );
}

void SessionManager::promote_session(const string& tmp_id, const string& actual_id) {
    if(is_tombstoned(actual_id)) {
        log_info(string("[INFO] Attempted to promote session to actual id ") + actual_id + ", but it is tombstoned. Dropping session.");
        auto it = session_map.find(tmp_id);
        if (it != session_map.end()) {
            it->second->close_session(boost::asio::error::connection_aborted);
            session_map.erase(it);
        }
        return;
    }
    if(session_map.find(actual_id) != session_map.end()) {
        log_warning(string("[WARN] Attempted to promote session to actual id ") + actual_id + ", but it already exists. Dropping old session.");
        session_map[actual_id]->close_session(boost::asio::error::connection_aborted);
        session_map.erase(actual_id);
    }
    auto it = session_map.find(tmp_id);
    if (it == session_map.end()) {
        log_warning(string("[WARN] Attempted to promote session with temporary id ") + tmp_id + ", but it was not found.");
        return;
    }
    auto session_ptr = it->second;
    session_map.erase(it);
    session_map[actual_id] = session_ptr;
    current_session_id = actual_id;
    log_info(string("[INFO] Session promoted from temporary id ") + tmp_id + " to actual id " + actual_id);
}
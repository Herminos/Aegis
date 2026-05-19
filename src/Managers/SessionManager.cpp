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
    on_send_message_handler = [this](string raw_input) {
        boost::asio::post(
            io,
            [this, raw_input=std::move(raw_input)](){
                if(current_session_id.empty()){
                    print_info("No active session. Please select a session to send messages.");
                    return;
                }
                auto it = session_map.find(current_session_id);
                if(it == session_map.end()){
                    print_info("Current session not found. Please select a valid session.");
                    current_session_id = "";
                    return;
                }
                it->second->send_message(raw_input);
                print_info("\r[To "+current_session_id+"]: "+raw_input);
                
            }
        );
    };

};

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
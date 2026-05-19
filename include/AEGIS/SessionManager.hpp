#pragma once

#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<functional>
#include<chrono>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/Utility.hpp>
#include<AEGIS/Session.hpp>

using namespace boost::asio;
using namespace std;


class SessionManager{
    private:
        Encryptor &encryptor;
        string current_session_id;
        map<string, shared_ptr<Session>> session_map;
        io_context &io;
        std::map<string, std::chrono::steady_clock::time_point> tombstoned_ids; // 存储已清理会话的ID和清理时间点
        
    public:
        SessionManager(io_context &_io, Encryptor &encryptor);
        void new_session(const string& ip_addr, const string& port);
        void new_session_from_socket_and_start(ip::tcp::socket &&socket, SessionRole role=SessionRole::SERVER);
        inline io_context& get_io_context() { return io; }
        std::function<void(string)> on_send_message_handler;
        bool is_tombstoned(const string& id) {
            auto it = tombstoned_ids.find(id);
            if(it != tombstoned_ids.end()) {
                auto tombstone_time = it->second;
                auto now = std::chrono::steady_clock::now();
                if(std::chrono::duration_cast<std::chrono::seconds>(now - tombstone_time).count() < 10) {
                    return true;
                } else {
                    // 已经过了10秒，移除墓碑记录
                    tombstoned_ids.erase(it);
                    return false;
                }
            }
            return false;
        }
        void promote_session(const string& tmp_id, const string& actual_id);
        bool if_has_session(const string& id) const {
            return session_map.find(id) != session_map.end();
        }
        std::function<void(string id)> on_session_cleaned_handler=[this](const string& id){
            print_info(string("[INFO] Session with id ") + id + " cleaned up. Removing from session manager.");
            boost::asio::post(io, [this, id](){
                size_t erased = session_map.erase(id);
                if (erased > 0) {
                    tombstoned_ids.insert({id, std::chrono::steady_clock::now()});
                    log_info(string("[INFO] Session with id ") + id + " cleaned and removed from session manager.");
                } else {
                    log_warning(string("[WARN] Attempted to clean session with id ") + id + ", but it was not found in session manager.");
                }
                this->current_session_id = "";
            });
        };
        
        std::function<void()> list_all_sessions_handler=[this](){
            for(const auto& [id, session] : session_map) {
                print_info(string("[INFO] Session ID: ") + id + ", Address: " + session->get_session_addr_and_port());
            }
        };

        std::function<void()> list_current_session_handler=[this](){
            if(current_session_id.empty()) {
                print_info("[INFO] No current session.");
                return;
            }
            if(session_map.find(current_session_id) != session_map.end()) {
                auto session = session_map[current_session_id];
                print_info(string("[INFO] Current Session ID: ") + current_session_id + ", Address: " + session->get_session_addr_and_port());
            } else {
                log_warning("[INFO] Current session ID is set but session not found in map.");
            }
         };

        std::function<void(const std::string &end_message)> exit_aegis_handler=[this](const std::string &end_message) {
            print_info("[INFO] Exiting AEGIS. Closing all sessions...");
            for(const auto& [id, session] : session_map) {
                session->shutdown_session(end_message);
            }
            std::thread([this](){
                std::this_thread::sleep_for(std::chrono::seconds(1)); // 等待2秒让所有会话有机会发送结束消息
                io.stop();
            }).detach();
        };

};        

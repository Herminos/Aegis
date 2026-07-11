#pragma once

#include<boost/asio.hpp>
#include<memory>
#include<map>
#include<functional>
#include<chrono>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/Utility.hpp>
#include<AEGIS/Session.hpp>

class SessionManager{
    private:
        Encryptor &encryptor;
        std::string current_session_id;
        std::map<std::string, std::shared_ptr<Session>> session_map;
        boost::asio::io_context &io;
        std::map<std::string, std::chrono::steady_clock::time_point> tombstoned_ids; // 存储已清理会话的ID和清理时间点

    public:
        SessionManager(boost::asio::io_context &_io, Encryptor &encryptor);

        // ============================================================
        // Session 生命周期
        // ============================================================
        void new_session(const std::string& ip_addr, const std::string& port);
        void new_session_from_socket_and_start(boost::asio::ip::tcp::socket &&socket, SessionRole role=SessionRole::SERVER);
        void promote_session(const std::string& tmp_id, const std::string& actual_id);

        // ============================================================
        // UI → 引擎 方法（由 AegisEngine 调用）
        // ============================================================
        void send_message(const std::string& msg, const std::string& session_id = "");
        void switch_session(const std::string& id);
        void shutdown_all(const std::string& end_message);

        // ============================================================
        // 查询方法
        // ============================================================
        inline boost::asio::io_context& get_io_context() { return io; }
        std::string get_current_session_id() const;
        std::string get_all_sessions_info() const;
        std::string get_current_session_info() const;
        bool is_tombstoned(const std::string& id);
        bool if_has_session(const std::string& id) const {
            return session_map.find(id) != session_map.end();
        }
        void on_list_sessions();            // 触发 on_session_list 回调或打印
        void on_show_current_session();     // 触发 on_info 回调或打印

        // ============================================================
        // 引擎 → UI 事件回调
        // 由 main.cpp 设置，桥接到 AegisEngine → UiDispatcher → UI
        // ============================================================
        std::function<void(std::string, std::string)> on_message_received;   // (session_id, msg)
        std::function<void(std::string, std::string)> on_message_sent;       // (session_id, msg)
        std::function<void(std::string, std::string)> on_session_active;     // (session_id, addr_port)
        std::function<void(std::string, std::string)> on_session_closed;     // (session_id, reason)
        std::function<void(std::string, std::string)> on_session_terminated; // (session_id, msg)
        std::function<void(std::string)> on_session_list;                    // (listing)
        std::function<void(std::string)> on_info;
        std::function<void(std::string)> on_warning;
        std::function<void(std::string)> on_error;

        // ============================================================
        // 保留旧式 handler（向后兼容终端模式）
        // ============================================================
        std::function<void(std::string)> on_send_message_handler = [this](std::string raw_input) {
            boost::asio::post(
                io,
                [this, raw_input=std::move(raw_input)](){
                    if(current_session_id.empty()){
                        if (on_warning) on_warning("No active session. Please select a session to send messages.");
                        else print_info("No active session. Please select a session to send messages.");
                        return;
                    }
                    auto it = session_map.find(current_session_id);
                    if(it == session_map.end()){
                        if (on_warning) on_warning("Current session not found. Please select a valid session.");
                        else print_info("Current session not found. Please select a valid session.");
                        current_session_id = "";
                        return;
                    }
                    it->second->send_message(raw_input);
                    if (on_message_sent) on_message_sent(current_session_id, raw_input);
                    else print_info("\r[To "+current_session_id+"]: "+raw_input);
                }
            );
        };

        std::function<void(std::string id)> on_session_cleaned_handler=[this](const std::string& id){
            if (on_info) on_info(std::string("Session with id ") + id + " cleaned up.");
            else print_info(std::string("[INFO] Session with id ") + id + " cleaned up. Removing from session manager.");

            boost::asio::post(io, [this, id](){
                size_t erased = session_map.erase(id);
                if (erased > 0) {
                    tombstoned_ids.insert({id, std::chrono::steady_clock::now()});
                    if (on_info) on_info(std::string("Session with id ") + id + " cleaned and removed from session manager.");
                    else log_info(std::string("[INFO] Session with id ") + id + " cleaned and removed from session manager.");
                } else {
                    if (on_warning) on_warning(std::string("Attempted to clean session with id ") + id + ", but it was not found.");
                    else log_warning(std::string("[WARN] Attempted to clean session with id ") + id + ", but it was not found in session manager.");
                }
                this->current_session_id = "";
            });
        };

        std::function<void()> list_all_sessions_handler=[this](){
            std::string info = get_all_sessions_info();
            if (on_info) on_info(info);
            else print_info(info);
        };

        std::function<void()> list_current_session_handler=[this](){
            std::string info = get_current_session_info();
            if (on_info) on_info(info);
            else print_info(info);
        };

        std::function<void(const std::string &end_message)> exit_aegis_handler=[this](const std::string &end_message) {
            if (on_info) on_info("Exiting AEGIS. Closing all sessions...");
            else print_info("[INFO] Exiting AEGIS. Closing all sessions...");

            for(const auto& [id, session] : session_map) {
                session->shutdown_session(end_message);
            }
            std::thread([this](){
                std::this_thread::sleep_for(std::chrono::seconds(1));
                io.stop();
            }).detach();
        };
};

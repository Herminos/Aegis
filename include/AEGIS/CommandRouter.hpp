#pragma once

#include<string>
#include<functional>

class CommandRouter{
    public:
        void handle_raw_input(const std::string& raw_msg);

        void set_send_message_handler(std::function<void(const std::string& msg)> handler) { send_message = handler; }
        void set_exit_aegis_handler(std::function<void()> handler) { exit_aegis_handler = handler; }
        void set_list_all_sessions_handler(std::function<void()> handler) { list_all_sessions = handler; }
        void set_list_current_session_handler(std::function<void()> handler) { list_current_session = handler; }
    private:
        std::function<void(const std::string& msg)> send_message;
        std::function<void()> exit_aegis_handler;
        std::function<void()> list_all_sessions;
        std::function<void()> list_current_session;
};
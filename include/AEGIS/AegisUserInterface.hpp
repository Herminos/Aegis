#pragma once

#include<string>

struct AegisUserInterface{

    virtual ~AegisUserInterface() = default;
    virtual void on_banner(std::string node_id, std::string version) = 0;
    virtual void on_message_received(std::string session_id, std::string msg) = 0;
    virtual void on_message_sent(std::string session_id, std::string msg) = 0;
    virtual void on_session_active(std::string session_id, std::string addr_port) = 0;
    virtual void on_session_closed(std::string session_id, std::string reason) = 0;
    virtual void on_session_terminated(std::string session_id, std::string msg) = 0;
    virtual void on_session_list(std::string listing) = 0;
    virtual void on_info(std::string msg) = 0;
    virtual void on_warning(std::string msg) = 0;
    virtual void on_error(std::string msg) = 0;
    virtual void on_success(std::string msg) = 0;

};
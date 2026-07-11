#pragma once

#include <string>

class SessionManager;
class UdpManager;

class AegisEngine {
public:
    // ============================================================
    // UI → 引擎（UI 主动调用）
    // ============================================================
    void send_message(std::string msg, std::string session_id = "");
    void switch_session(std::string session_id);
    void exec_command(std::string command);

    // 依赖注入
    SessionManager* session_manager = nullptr;
    UdpManager* udp_manager = nullptr;
};

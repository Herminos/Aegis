#pragma once

#include<AEGIS/AegisUserInterface.hpp>
#include<memory>
#include<boost/asio.hpp>
#include<string>
#include<thread>

class UiDispatcher{

    private:
        std::unique_ptr<AegisUserInterface> ui;
        boost::asio::io_context& ui_io;
        std::thread ui_thread;

    public:
        UiDispatcher(boost::asio::io_context& _ui_io, std::unique_ptr<AegisUserInterface> _ui) : ui(std::move(_ui)), ui_io(_ui_io) {
            ui_thread = std::thread([this]() {
                ui_io.run();
            });
        }

        ~UiDispatcher() {
            ui_io.stop();
            if (ui_thread.joinable()) {
                ui_thread.join();
            }
        }

        void on_banner(std::string node_id, std::string version);
        void on_message_received(std::string session_id, std::string msg);
        void on_message_sent(std::string session_id, std::string msg);
        void on_session_active(std::string session_id, std::string addr_port);
        void on_session_closed(std::string session_id, std::string reason);
        void on_session_terminated(std::string session_id, std::string msg);
        void on_session_list(std::string listing);
        void on_info(std::string msg);
        void on_warning(std::string msg);
        void on_error(std::string msg);
        void on_success(std::string msg);

};
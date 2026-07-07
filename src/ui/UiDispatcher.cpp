#include<AEGIS/UiDispatcher.hpp>

void UiDispatcher::on_banner(std::string node_id, std::string version) {
    boost::asio::post(ui_io, [this, node_id = std::move(node_id), version = std::move(version)]() {
        ui->on_banner(std::move(node_id), std::move(version));
    });
};

void UiDispatcher::on_message_received(std::string session_id, std::string msg) {
    boost::asio::post(ui_io, [this, session_id = std::move(session_id), msg = std::move(msg)]() {
        ui->on_message_received(std::move(session_id), std::move(msg));
    });
};

void UiDispatcher::on_message_sent(std::string session_id, std::string msg) {
    boost::asio::post(ui_io, [this, session_id = std::move(session_id), msg = std::move(msg)]() {
        ui->on_message_sent(std::move(session_id), std::move(msg));
    });
};

void UiDispatcher::on_session_active(std::string session_id, std::string addr_port) {
    boost::asio::post(ui_io, [this, session_id = std::move(session_id), addr_port = std::move(addr_port)]() {
        ui->on_session_active(std::move(session_id), std::move(addr_port));
    });
};

void UiDispatcher::on_session_closed(std::string session_id, std::string reason) {
    boost::asio::post(ui_io, [this, session_id = std::move(session_id), reason = std::move(reason)]() {
        ui->on_session_closed(std::move(session_id), std::move(reason));
    });
};

void UiDispatcher::on_session_terminated(std::string session_id, std::string msg) {
    boost::asio::post(ui_io, [this, session_id = std::move(session_id), msg = std::move(msg)]() {
        ui->on_session_terminated(std::move(session_id), std::move(msg));
    });
};

void UiDispatcher::on_session_list(std::string listing) {
    boost::asio::post(ui_io, [this, listing = std::move(listing)]() {
        ui->on_session_list(std::move(listing));
    });
};

void UiDispatcher::on_info(std::string msg) {
    boost::asio::post(ui_io, [this, msg = std::move(msg)]() {
        ui->on_info(std::move(msg));
    });
};

void UiDispatcher::on_warning(std::string msg) {
    boost::asio::post(ui_io, [this, msg = std::move(msg)]() {
        ui->on_warning(std::move(msg));
    });
};

void UiDispatcher::on_error(std::string msg) {
    boost::asio::post(ui_io, [this, msg = std::move(msg)]() {
        ui->on_error(std::move(msg));
    });
};
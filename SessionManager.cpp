#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<charconv>
#include"Encryptor.hpp"
#include"TcpManager.hpp"
#include"SessionManager.hpp"

using namespace boost::asio;
using namespace std;


Session::Session(io_context &_io, const string& ip_addr, string port, const string& name, const string& id, Encryptor &encryptor) :
    socket(_io), remote_endpoint(ip::make_address_v4(ip_addr), stoi(port)), session_name(name), session_id(id), encryptor(encryptor)
{
    socket.open(ip::tcp::v4());
}

Session::Session(io_context &_io, ip::tcp::socket socket, const string& name, const string& id, Encryptor &encryptor) :
    socket(move(socket)), remote_endpoint(this->socket.remote_endpoint()), session_name(name), session_id(id), encryptor(encryptor)
{
    
};

void Session::start(){
    printf("[INFO] Starting session: %s\n", session_name.c_str());
    socket.async_connect(remote_endpoint, [self=shared_from_this()](const boost::system::error_code& ec) {
        if (ec) {
            printf("[ERROR] Failed to connect to %s: %s\n", self->remote_endpoint.address().to_string().c_str(), ec.message().c_str());
            return;
        }
        printf("[INFO] Session %s connected to %s:%d\n", self->session_name.c_str(), self->remote_endpoint.address().to_string().c_str(), self->remote_endpoint.port());
        self->on_read_loop();
    });
};


void Session::send_msg(const string& msg) {   
    boost::asio::post(socket.get_executor(), [self=shared_from_this(), msg]() {
        bool is_processing = !self->msg_queue.empty();
        self->msg_queue.push_back(msg);
        if(!is_processing) 
            self->do_write();//如果消息队列不为空就开始调用
        
    });
};

void Session::do_write() {
    auto self(shared_from_this());
    async_write(socket, buffer(msg_queue.front()), [self](const boost::system::error_code& e, size_t bytes_transferred) {
        if (e) {
            printf("[ERROR] Write error: %s\n", e.message().c_str());
            return;
        }
        self->msg_queue.pop_front();
        if(self->msg_queue.empty()) 
            self->do_write();//递归调用
    });
}

void Session::close_session(const boost::system::error_code& ec) {
    
    if (socket.is_open()) {
        boost::system::error_code ignored_ec;
        socket.close(ignored_ec); 
    }

    if (on_close_handler) {
        on_close_handler(session_id);

        on_close_handler = nullptr; 
    }

    printf("[WARN] Session %s (%s) disconnected. Reason: %s\n", 
           session_name.c_str(), session_id.c_str(), ec.message().c_str());
}

// 在你的异步读写中，一旦捕获错误，立刻调用它：
void Session::on_read_loop() {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket, read_buffer, '\n',
        [self, this](const boost::system::error_code& ec, size_t bytes_transferred) {
            if (ec) {
                close_session(ec);
                return;
            }
            
            std::istream is(&read_buffer);
            std::string line;
            std::getline(is, line);
            //
            on_read_loop();
        }
    );
};

inline ip::tcp::socket& Session::get_socket() {
    return socket;
};

SessionManager::SessionManager(io_context &_io, Encryptor &encryptor) : io(_io) , encryptor(encryptor) {

};

void SessionManager::new_session(const string& ip_addr, const string& port, const string& session_id, const string& name) {

    if(session_map.find(session_id) != session_map.end()) {
        printf("[WARN] Session with id %s already exists. Skipping creation.\n", session_id.c_str());
        return;
    }

    auto session=make_shared<Session>(io, ip_addr, port, name, session_id, encryptor);
    
    session->set_on_close_handler([this](const string& id) {
        boost::asio::post(io, [this, id](){
            size_t erased = session_map.erase(id);
            if (erased > 0) {
                printf("[INFO] Session with id %s removed from session manager.\n", id.c_str());
            } else {
                printf("[WARN] Attempted to remove session with id %s, but it was not found in session manager.\n", id.c_str());
            }

        });
    });

    session_map[session_id]=session;
    session->start(); //start the session

}

void SessionManager::new_session_from_socket_and_start(ip::tcp::socket socket, const string& session_id, const string& name) {
    if(session_map.find(session_id) != session_map.end()) {
        printf("[WARN] Session with id %s already exists. Skipping creation.\n", session_id.c_str());
        return;
    }
    auto session=make_shared<Session>(io, move(socket), name, session_id, encryptor);
    
    session->set_on_close_handler([this](const string& id) {
        boost::asio::post(io, [this, id](){
            size_t erased = session_map.erase(id);
            if (erased > 0) {
                printf("[INFO] Session with id %s removed from session manager.\n", id.c_str());
            } else {
                printf("[WARN] Attempted to remove session with id %s, but it was not found in session manager.\n", id.c_str());
            }

        });
    });

    session_map[session_id]=session;
    session->on_read_loop();
}

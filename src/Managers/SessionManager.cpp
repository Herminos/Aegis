#include<boost/asio.hpp>
#include<stdio.h>
#include<memory>
#include<map>
#include<charconv>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/SessionManager.hpp>
#include<AEGIS/TcpManager.hpp>
#include<AEGIS/Utility.hpp>

using namespace boost::asio;
using namespace std;

#define __SENDING_PUBKEY 0x01
#define __SENDING_ENCRYPTED_DATA 0x02
#define __TERMINATED 0xFF

inline AETPHeader parse_header(const array<uint8_t, 10> buffer){
    AETPHeader header;
    header.magic = (buffer[0] << 8) | buffer[1];
    header.version = buffer[2];
    header.type    = buffer[3];
    header.payload_length = (static_cast<uint32_t>(buffer[4]) << 24) |
                            (static_cast<uint32_t>(buffer[5]) << 16) |
                            (static_cast<uint32_t>(buffer[6]) << 8)  |
                            (static_cast<uint32_t>(buffer[7]));
    header.crc = (buffer[8] << 8) | buffer[9];

    return header;
}

inline std::vector<uint8_t> parse_nonce_from_payload(const std::vector<uint8_t>& payload){
    std::vector<uint8_t> nonce(payload.begin(), payload.begin()+crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    return nonce;
}

inline std::vector<uint8_t> parse_cipher_and_mac_from_payload(const std::vector<uint8_t>& payload){
    std::vector<uint8_t> cipher_and_mac(payload.begin()+crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, payload.end());
    return cipher_and_mac;
}

inline std::vector<uint8_t> build_handshake_payload(
    const std::vector<uint8_t>& my_long_term_pk, // 32 字节 Ed25519 公钥
    const std::vector<uint8_t>& my_long_term_sk, // 64 字节 Ed25519 私钥
    const std::vector<uint8_t>& my_ephemeral_pk,  // 32 字节 X25519 临时公钥
    Encryptor &e) {

    std::vector<uint8_t> msg_to_sign;
    std::string prefix = "AEGIS";
    msg_to_sign.insert(msg_to_sign.end(), prefix.begin(), prefix.end());
    msg_to_sign.insert(msg_to_sign.end(), my_ephemeral_pk.begin(), my_ephemeral_pk.end());

    std::vector<uint8_t> signature = e.generate_signature(msg_to_sign, my_long_term_sk);

    std::vector<uint8_t> payload;
    payload.reserve(128);

    payload.insert(payload.end(), my_long_term_pk.begin(), my_long_term_pk.end()); // [0~31]
    payload.insert(payload.end(), my_ephemeral_pk.begin(), my_ephemeral_pk.end()); // [32~63]
    payload.insert(payload.end(), signature.begin(), signature.end());             // [64~127]

    return payload;
}

std::vector<uint8_t> Session::build_package_from_payload(const std::vector<uint8_t>& payload, uint8_t type) {
    

    std::array<uint8_t, 10> header;
    header[0] = 0xAE; // Magic 1
    header[1] = 0x47; // Magic 2
    header[2] = 0x01; // Version
    header[3] = type; // Type

    uint32_t length = payload.size();
    
    header[4] = (length >> 24) & 0xFF;
    header[5] = (length >> 16) & 0xFF;
    header[6] = (length >> 8)  & 0xFF;
    header[7] = length & 0xFF;

    uint16_t crc = calculate_header_crc16(header.data(), 8);
    header[8] = (crc >> 8) & 0xFF;
    header[9] = crc        & 0xFF;

    std::vector<uint8_t> package;
    
    package.reserve(header.size() + payload.size());
    
    package.insert(package.end(), header.begin(), header.end());
    package.insert(package.end(), payload.begin(), payload.end());

    return package;
}

Session::Session(io_context &_io, const string& ip_addr, string port, const string& id, Encryptor &encryptor, std::function<void(std::string)> on_session_cleaned_handler) :
    socket(_io), remote_endpoint(ip::make_address_v4(ip_addr), stoi(port)), session_id(id), encryptor(encryptor), state(SessionState::IDLE), on_session_cleaned_handler(on_session_cleaned_handler)
{
    socket.open(ip::tcp::v4());
}

Session::Session(io_context &_io, ip::tcp::socket socket, const string& id, Encryptor &encryptor, std::function<void(std::string)> on_session_cleaned_handler) :
    socket(move(socket)), remote_endpoint(this->socket.remote_endpoint()), session_id(id), encryptor(encryptor), state(SessionState::IDLE), on_session_cleaned_handler(on_session_cleaned_handler)
{
    
};

Session::~Session(){
    log_info(string("[INFO] Session ") + session_id + " is being destroyed, cleaning up resources.");
    
    boost::system::error_code ec;
    if (socket.is_open()) {
        socket.close(ec);
    }

    if (!peer_ephemeral_public_key.empty()) {
        sodium_memzero(peer_ephemeral_public_key.data(), peer_ephemeral_public_key.size());
    }
    if (!session_key_pair.rx_key.empty()) {
        sodium_memzero(session_key_pair.rx_key.data(), session_key_pair.rx_key.size());
    }
    if (!session_key_pair.tx_key.empty()) {
        sodium_memzero(session_key_pair.tx_key.data(), session_key_pair.tx_key.size());
    }
    if (!my_ephemeral_keypair.public_key.empty()) {
        sodium_memzero(my_ephemeral_keypair.public_key.data(), my_ephemeral_keypair.public_key.size());
    }
    if (!my_ephemeral_keypair.private_key.empty()) {
        sodium_memzero(my_ephemeral_keypair.private_key.data(), my_ephemeral_keypair.private_key.size());
    }

};

void Session::start(){
    auto self(shared_from_this());
    print_info("[INFO] Starting session with ID: " + session_id + " to " + remote_endpoint.address().to_string() + ":" + to_string(remote_endpoint.port()));
    socket.async_connect(remote_endpoint, [self](const boost::system::error_code& ec) {
        if (ec) {
            log_error(string("[ERROR] Failed to connect to ") + self->remote_endpoint.address().to_string() + ":" + to_string(self->remote_endpoint.port()) + " - " + ec.message());
            return;
        }
        success_info(string("[INFO] Session ") + self->session_id + " connected to " + self->remote_endpoint.address().to_string() + ":" + to_string(self->remote_endpoint.port()));
        boost::asio::co_spawn(
            self->socket.get_executor(),
            [self]() mutable -> boost::asio::awaitable<void> {
                co_await self->start_read_loop_coroutine();
            },
            boost::asio::detached
        );

    });
};


boost::asio::awaitable<void> Session::start_read_loop_coroutine() {
    auto self(shared_from_this());
    try{
        while(true){    
            if(self->role == SessionRole::CLIENT && self->state == SessionState::IDLE){
                self->my_ephemeral_keypair=self->encryptor.generate_ephemeral_keypair();
                self->send_package(
                    std::move(build_package_from_payload(
                        build_handshake_payload(
                            encryptor.public_key,
                            encryptor.private_key,
                            my_ephemeral_keypair.public_key,
                            encryptor
                        ), __SENDING_PUBKEY
                    ))
                );
                self->state=SessionState::PUBKEY_EXCHANGING;
            }

            std::array<uint8_t, 10> header_buffer;
            co_await boost::asio::async_read(socket, boost::asio::buffer(header_buffer), boost::asio::use_awaitable);

            

            AETPHeader header=parse_header(header_buffer);
            
            if(header.magic != 0xAE47 || header.version != 0x01 || header.crc != calculate_header_crc16(header_buffer.data(), 8)){
                //throw std::runtime_error("Invalid AETP header.");
                log_error(string("[ERROR] Invalid AETP header received, ignoring this packet."));
                if(header.magic != 0xAE47){
                    throw std::runtime_error("Invalid magic number in header.");
                }
                if(header.version != 0x01){
                    throw std::runtime_error("Unsupported AETP version.");
                }
                if(header.crc != calculate_header_crc16(header_buffer.data(), 8)){
                    log_error(string("[ERROR] Header CRC16 mismatch. Expected: %04X, Received: %04X") + to_string(calculate_header_crc16(header_buffer.data(), 8)) + ", " + to_string(header.crc));
                    throw std::runtime_error("Header CRC16 mismatch.");
                }
            }

            if(header.payload_length >= 1*1024*1024*1024){
                throw std::runtime_error("Payload over 1GB, too large");
            }
            if(header.type==0xFF){
                print_info(string("[INFO] Received termination notice from ") + session_id + " (" + remote_endpoint.address().to_string() + ":" + to_string(remote_endpoint.port()) + "). Closing session.");
                self->state = SessionState::TERMINATED;
                clean_session();
                co_return;
            }

            std::vector<uint8_t> payload_buffer(header.payload_length);
            co_await boost::asio::async_read(socket, boost::asio::buffer(payload_buffer), boost::asio::transfer_exactly(header.payload_length), boost::asio::use_awaitable);

            if(header.type==0x01){
                if(self->state == SessionState::ACTIVE){
                    throw std::runtime_error("Session already active, receive PUBKEY_EXCHANGING packet.");
                }
                self->handle_handshaking(std::move(payload_buffer));
            }

            if(header.type==0x02){
                if(self->state == SessionState::PUBKEY_EXCHANGING || self->state == SessionState::IDLE){
                    throw std::runtime_error("Public key not exchaneged yet, receive ACTIVE packet.");
                }
                co_await self->process_encrypted_data_coroutine(payload_buffer);
            }
        }
        
    }
    catch(const exception &e){
        log_warning(string("[WARN] Session ") + session_id + " (" + remote_endpoint.address().to_string() + ":" + to_string(remote_endpoint.port()) + ") encountered an error: " + e.what() + ". Closing session.");
        close_session(boost::asio::error::connection_aborted);
    }
};

void Session::handle_handshaking(std::vector<uint8_t> handshaking_payload) {

    if(handshaking_payload.size() != 128){
        throw std::runtime_error("Invalid handshaking payload.");
    }
    vector<uint8_t> peer_long_term_pk(handshaking_payload.begin(), handshaking_payload.begin()+32);
    vector<uint8_t> peer_ephemeral_pk(handshaking_payload.begin()+32, handshaking_payload.begin()+64);
    vector<uint8_t> signature(handshaking_payload.begin()+64, handshaking_payload.end());
    string calculated_id = bin_to_base64(peer_long_term_pk);
    
    

    std::vector<uint8_t> signed_msg;
    std::string prefix = "AEGIS";
    signed_msg.reserve(prefix.size() + peer_ephemeral_pk.size());
    signed_msg.insert(signed_msg.end(), prefix.begin(), prefix.end());
    signed_msg.insert(signed_msg.end(), peer_ephemeral_pk.begin(), peer_ephemeral_pk.end());

    if(!encryptor.verify_signature(signature, signed_msg, peer_long_term_pk)){
        throw std::runtime_error("Invalid signature.");
    };
        success_info(string("[INFO] Session ") + session_id + ": Ed25519 signature verified.");
        on_session_promotion_handler(session_id, calculated_id);
        session_id = calculated_id;
        if (role == SessionRole::SERVER && state == SessionState::IDLE) {
        // 我是服务端：生成我的临时密钥，算出双向密钥，发回执，进入 ACTIVE
        my_ephemeral_keypair = encryptor.generate_ephemeral_keypair();
        session_key_pair = encryptor.derive_session_keys(false, peer_ephemeral_pk, my_ephemeral_keypair.public_key, my_ephemeral_keypair.private_key);
    
        send_package(
            std::move(build_package_from_payload(
                build_handshake_payload(
                    encryptor.public_key,
                    encryptor.private_key,
                    my_ephemeral_keypair.public_key,
                    encryptor
                ), __SENDING_PUBKEY)
            )
        );
        state = SessionState::ACTIVE;
        success_info(string("[INFO] Server Session ") + session_id + ": Keys derived, ACTIVE.");

    } else if (role == SessionRole::CLIENT && state == SessionState::PUBKEY_EXCHANGING) {
        // 我是客户端：用我之前存好的 my_ephemeral_keypair，算出双向密钥，进入 ACTIVE
        session_key_pair = encryptor.derive_session_keys(true, peer_ephemeral_pk, my_ephemeral_keypair.public_key, my_ephemeral_keypair.private_key);
    
        state = SessionState::ACTIVE;
        success_info(string("[INFO] Client Session ") + session_id + ": Keys derived, ACTIVE.");

    } else {
        throw std::runtime_error("Invalid state transition during handshake.");
    }
}

boost::asio::awaitable<void> Session::process_encrypted_data_coroutine(std::vector<uint8_t> payload) {
    vector<uint8_t> nonce(parse_nonce_from_payload(payload));
    vector<uint8_t> cipher_and_mac(parse_cipher_and_mac_from_payload(payload));
    vector<uint8_t> plain_text = co_await encryptor.async_decrypt(move(cipher_and_mac), move(nonce), session_key_pair.rx_key);
    
    handle_incoming_message(string(plain_text.begin(), plain_text.end()));

};

void Session::handle_incoming_message(const string& msg) {
    print_info(string("\r\n[From " + session_id + "]: ") + msg);
    // 这里可以添加更多的消息处理
};

void Session::send_termination_package() {
    send_package(build_package_from_payload({}, __TERMINATED));
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

    log_warning(string("[WARN] Session ") + session_id + " (" + remote_endpoint.address().to_string() + ":" + to_string(remote_endpoint.port()) + ") disconnected. Reason: " + ec.message());

}

void Session::shutdown_session() {
    send_termination_package();
    clean_session();
}

void Session::clean_session() {
    if(state != SessionState::TERMINATED){
        return;
    }
    outgoing_package_queue.clear();
    peer_ephemeral_public_key.clear();
    session_key_pair.rx_key.clear();
    session_key_pair.tx_key.clear();
    my_ephemeral_keypair.public_key.clear();
    my_ephemeral_keypair.private_key.clear();
    boost::system::error_code ignored_ec;
    if (socket.is_open()) {
        socket.close(ignored_ec);
    }
    on_session_cleaned_handler(session_id);
}

void Session::send_package(vector<uint8_t> package) {
    if(!socket.is_open()){
        return;
    }
    auto self(shared_from_this());
    boost::asio::post(
        socket.get_executor(),
        [self, package=move(package)]() mutable{
            bool write_in_progress = !self->outgoing_package_queue.empty();
            self->outgoing_package_queue.push_back(std::move(package));
            if(!write_in_progress){
                self->start_write_loop();
            }
        }
    );
}

void Session::start_write_loop(){
    auto self=shared_from_this();
    boost::asio::co_spawn(
        socket.get_executor(),
        [self]() mutable -> boost::asio::awaitable<void> {
            try{
                while(!self->outgoing_package_queue.empty()){
                    vector<uint8_t> package = self->outgoing_package_queue.front();
                    co_await boost::asio::async_write(self->socket, boost::asio::buffer(package), boost::asio::use_awaitable);
                    self->outgoing_package_queue.pop_front();
                }
            }
            catch(const std::exception &e){
                log_error(string("[ERROR] Failed to send package: ") + e.what());
                self->close_session(boost::asio::error::fault);
            }
        },boost::asio::detached
    );
}

void Session::send_message(const string& msg){
    if(state != SessionState::ACTIVE){
        print_info(string("[INFO] Session ") + session_id + " is not ACTIVE. Cannot send message.");
        return;
    }
    std::vector<uint8_t> plain_text(msg.begin(), msg.end());
    std::vector<uint8_t> nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    randombytes_buf(nonce.data(), nonce.size());
    if(!socket.is_open()){
        print_info(string("[INFO] Socket is closed. Cannot send message."));
        return;
    }
    auto self(shared_from_this());
    boost::asio::co_spawn(
        socket.get_executor(),
        [self, plain_text=std::move(plain_text), nonce=std::move(nonce)]() mutable -> boost::asio::awaitable<void> {
            try{
                std::vector<uint8_t> cipher_and_mac = co_await self->encryptor.async_encrypt(std::move(plain_text), self->session_key_pair.tx_key, nonce);
                std::vector<uint8_t> payload;
                payload.reserve(nonce.size() + cipher_and_mac.size());
                payload.insert(payload.end(), nonce.begin(), nonce.end());
                payload.insert(payload.end(), cipher_and_mac.begin(), cipher_and_mac.end());

                self->send_package(self->build_package_from_payload(std::move(payload), __SENDING_ENCRYPTED_DATA));
            }
            catch(const std::exception &e){
                log_error(string("[ERROR] Failed to send message: ") + e.what());
                self->close_session(boost::asio::error::fault);
            }
            co_return;
        },
        boost::asio::detached
    );
}


inline ip::tcp::socket& Session::get_socket() {
    return socket;
};

SessionManager::SessionManager(io_context &_io, Encryptor &encryptor) : io(_io) , encryptor(encryptor) {
    on_send_message_handler = [this](string raw_input) {
        boost::asio::post(
            io,
            [this, raw_input=move(raw_input)](){
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

void SessionManager::new_session_from_socket_and_start(ip::tcp::socket socket) {

    string map_key = "TEMP_ID_" + socket.remote_endpoint().address().to_string() + ":" + to_string(socket.remote_endpoint().port());

    if(session_map.find(map_key) != session_map.end()) {
        log_info(string("[INFO] Session with id ") + map_key + " already exists. Skipping.");
        return;
    }
    auto session = make_shared<Session>(io, move(socket), map_key, encryptor, on_session_cleaned_handler);
    session->role = SessionRole::SERVER;
    
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
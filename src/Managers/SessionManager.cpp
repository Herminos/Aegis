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

std::vector<uint8_t> Session::build_handshake_package(const std::vector<uint8_t>& payload) {
    

    if (payload.size() != 128) {
        throw std::invalid_argument("[AETP Packager] Fatal: Handshake payload must be exactly 128 bytes.");
    }

    std::array<uint8_t, 10> header;
    header[0] = 0xAE; // Magic 1
    header[1] = 0x47; // Magic 2
    header[2] = 0x01; // Version
    header[3] = 0x01; // Type: 0x01 (SENDING_PUBKEY)

    uint32_t length = 128;
    header[4] = (length >> 24) & 0xFF;
    header[5] = (length >> 16) & 0xFF;
    header[6] = (length >> 8)  & 0xFF;
    header[7] = length         & 0xFF;

    uint16_t crc = calculate_header_crc16(header.data(), 8);
    header[8] = (crc >> 8) & 0xFF;
    header[9] = crc        & 0xFF;

    std::vector<uint8_t> package;
    // 精准预分配 138 字节内存，零多余开销
    package.reserve(header.size() + payload.size());
    
    package.insert(package.end(), header.begin(), header.end());
    package.insert(package.end(), payload.begin(), payload.end());

    return package;
}

Session::Session(io_context &_io, const string& ip_addr, string port, const string& name, const string& id, Encryptor &encryptor) :
    socket(_io), remote_endpoint(ip::make_address_v4(ip_addr), stoi(port)), session_name(name), session_id(id), encryptor(encryptor), state(SessionState::IDLE)
{
    socket.open(ip::tcp::v4());
}

Session::Session(io_context &_io, ip::tcp::socket socket, const string& name, const string& id, Encryptor &encryptor) :
    socket(move(socket)), remote_endpoint(this->socket.remote_endpoint()), session_name(name), session_id(id), encryptor(encryptor), state(SessionState::IDLE)
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
        
        boost::asio::co_spawn(
            self->socket.get_executor(),
            self->start_read_loop_coroutine(),
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
                    build_handshake_package(
                        build_handshake_payload(
                            encryptor.public_key,
                            encryptor.private_key,
                            my_ephemeral_keypair.public_key,
                            encryptor
                        )
                    )
                );
                self->state=SessionState::PUBKEY_EXCHANGING;
            }

            std::array<uint8_t, 10> header_buffer;
            co_await boost::asio::async_read(socket, boost::asio::buffer(header_buffer), boost::asio::use_awaitable);

            AETPHeader header=parse_header(header_buffer);
            
            if(header.magic != 0xAE47 || header.version != 0x01){
                throw std::runtime_error("Invalid AETP header.");
            }
            if(header.payload_length >= 1*1024*1024*1024){
                throw std::runtime_error("Payload over 1GB, too large");
            }
            if(header.type==0xFF){
                throw std::runtime_error("Session terminated called by remote peer.");
            }

            std::vector<uint8_t> payload_buffer(header.payload_length);
            co_await boost::asio::async_read(socket, boost::asio::buffer(payload_buffer), boost::asio::transfer_exactly(header.payload_length), boost::asio::use_awaitable);

            if(header.type==0x01){
                if(self->state == SessionState::ACTIVE){
                    throw std::runtime_error("Session already active, receive PUBKEY_EXCHANGING packet.");
                }
                self->handle_handshaking(payload_buffer);
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
        printf("[WARN] Session terminated: %s\n", e.what());
        close_session(boost::asio::error::fault);
    }
};

void Session::handle_handshaking(const std::vector<uint8_t>& handshaking_payload) {

    if(handshaking_payload.size() != 128){
        throw std::runtime_error("Invalid handshaking payload.");
    }
    vector<uint8_t> peer_long_term_pk(handshaking_payload.begin(), handshaking_payload.begin()+32);
    vector<uint8_t> peer_ephemeral_pk(handshaking_payload.begin()+32, handshaking_payload.begin()+64);
    vector<uint8_t> signature(handshaking_payload.begin()+64, handshaking_payload.end());
    if(bin_to_base64(peer_long_term_pk)!=this->session_id){
        throw std::runtime_error("Incorrect session ID with UDP Broadcast phase");
    }
    std::vector<uint8_t> signed_msg;
    std::string prefix = "AEGIS";
    signed_msg.reserve(prefix.size() + peer_ephemeral_pk.size());
    signed_msg.insert(signed_msg.end(), prefix.begin(), prefix.end());
    signed_msg.insert(signed_msg.end(), peer_ephemeral_pk.begin(), peer_ephemeral_pk.end());

    if(!encryptor.verify_signature(signature, signed_msg, peer_long_term_pk)){
        throw std::runtime_error("Invalid signature.");
    };
        printf("[INFO] Session %s: Ed25519 signature verified.\n", session_id.c_str());

        if (role == SessionRole::SERVER && state == SessionState::IDLE) {
        // 我是服务端：生成我的临时密钥，算出双向密钥，发回执，进入 ACTIVE
        my_ephemeral_keypair = encryptor.generate_ephemeral_keypair();
        session_key_pair = encryptor.derive_session_keys(false, peer_ephemeral_pk, my_ephemeral_keypair.public_key, my_ephemeral_keypair.private_key);
    
        send_package(
            build_handshake_package(
                build_handshake_payload(
                    encryptor.public_key,
                    encryptor.private_key,
                    my_ephemeral_keypair.public_key,
                    encryptor
                )
            )
        );
        state = SessionState::ACTIVE;
        printf("[INFO] Server Session %s: Keys derived, ACTIVE.\n", session_id.c_str());

    } else if (role == SessionRole::CLIENT && state == SessionState::PUBKEY_EXCHANGING) {
        // 我是客户端：用我之前存好的 my_ephemeral_keypair，算出双向密钥，进入 ACTIVE
        session_key_pair = encryptor.derive_session_keys(true, peer_ephemeral_pk, my_ephemeral_keypair.public_key, my_ephemeral_keypair.private_key);
    
        state = SessionState::ACTIVE;
        printf("[INFO] Client Session %s: Keys derived, ACTIVE.\n", session_id.c_str());

    } else {
    throw std::runtime_error("Invalid state transition during handshake.");
}
}

boost::asio::awaitable<void> Session::process_encrypted_data_coroutine(const std::vector<uint8_t>& payload) {
    vector<uint8_t> nonce(parse_nonce_from_payload(payload));
    vector<uint8_t> cipher_and_mac(parse_cipher_and_mac_from_payload(payload));
    vector<uint8_t> plain_text = co_await encryptor.async_decrypt(cipher_and_mac, nonce, session_key_pair.rx_key);
    
    handle_incoming_message(string(plain_text.begin(), plain_text.end()));

};

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

void Session::send_package(const vector<uint8_t>& package) {
    if(!socket.is_open()){
        return;
    }
    boost::asio::co_spawn(
        socket.get_executor(),
        send_package_coroutine(package),
        boost::asio::detached
    );
}

boost::asio::awaitable<void> Session::send_package_coroutine(const vector<uint8_t>& package) {

    try{
        co_await boost::asio::async_write(socket, boost::asio::buffer(package), boost::asio::use_awaitable);
    }
    catch(const std::exception &e){
        printf("[ERROR] Failed to send package: %s\n", e.what());
        close_session(boost::asio::error::fault);
    }
    co_return;
}


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
    session->role = SessionRole::CLIENT;
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
    session->role = SessionRole::SERVER;
    session_map[session_id]=session;
    boost::asio::co_spawn(
        io,
        session->start_read_loop_coroutine(),
        boost::asio::detached
    );
}

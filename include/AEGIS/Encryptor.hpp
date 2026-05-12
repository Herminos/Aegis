#pragma once

#include<boost/asio.hpp>
#include<boost/asio/use_awaitable.hpp>
#include<sodium.h>

using namespace std;
using namespace boost::asio;

struct EphemeralKeyPair {
    std::vector<uint8_t> public_key;
    std::vector<uint8_t> private_key;
};

struct SessionKeyPair{
    std::vector<uint8_t> rx_key;
    std::vector<uint8_t> tx_key;
};

class Encryptor {
    public:
        Encryptor(thread_pool &pool);
        boost::asio::awaitable<std::vector<uint8_t>> async_encrypt(const std::vector<uint8_t> &data, const std::vector<uint8_t> &key, const std::vector<uint8_t> &nonce);
        boost::asio::awaitable<std::vector<uint8_t>> async_decrypt(const std::vector<uint8_t> &cipher_and_mac, const std::vector<uint8_t> &nonce, const std::vector<uint8_t> &key);
        std::vector<uint8_t> public_key;
        std::vector<uint8_t> private_key;
        bool verify_signature(const vector<uint8_t>& signature, const vector<uint8_t>& signed_msg, const vector<uint8_t>& peer_long_term_pk){
                if(!crypto_sign_verify_detached(
                signature.data(),
                signed_msg.data(),
                signed_msg.size(),
                peer_long_term_pk.data()
            )){
                throw runtime_error("Invaild signature.");
            }
        }//这么简单的函数就写在这里了
        std::vector<uint8_t> generate_signature(const std::vector<uint8_t>& message, const std::vector<uint8_t>& private_key);
        EphemeralKeyPair generate_ephemeral_keypair();
        SessionKeyPair derive_session_keys(
                                bool if_client,
                                const std::vector<uint8_t>& peer_ephemeral_pk,
                                const std::vector<uint8_t>& my_ephemeral_pk,
                                std::vector<uint8_t>& my_ephemeral_sk 
                            );
        std::string get_id();
    private:
        void generate_identity_key_pair();
        void load_identity_key_pair();
        std::vector<uint8_t> do_encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& public_key, const std::vector<uint8_t>& nonce);
        std::vector<uint8_t> do_decrypt(const std::vector<uint8_t>& cipher_and_mac, const std::vector<uint8_t> nonce, const std::vector<uint8_t> key);
        
        boost::asio::thread_pool &_thread_pool;
};
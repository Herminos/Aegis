#pragma once

#include<boost/asio.hpp>
#include<boost/asio/use_awaitable.hpp>
#include<sodium.h>


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
        Encryptor(boost::asio::thread_pool &pool, const std::string& key_seed_path="");
        boost::asio::awaitable<std::vector<uint8_t>> async_encrypt(std::vector<uint8_t> data, std::vector<uint8_t> key, std::vector<uint8_t> nonce, std::array<uint8_t, 10> header);
        boost::asio::awaitable<std::vector<uint8_t>> async_decrypt(std::vector<uint8_t> cipher_and_mac, std::vector<uint8_t> nonce, std::vector<uint8_t> key, std::array<uint8_t, 10> header);
        std::vector<uint8_t> public_key;
        std::vector<uint8_t> private_key;
        bool verify_signature(const std::vector<uint8_t>& signature, const std::vector<uint8_t>& signed_msg, const std::vector<uint8_t>& peer_long_term_pk){
            return crypto_sign_verify_detached(
                signature.data(),
                signed_msg.data(),
                signed_msg.size(),
                peer_long_term_pk.data())==0;
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
        void load_seed_from_file(const std::string& path);   // 从文件读种子，派生新密钥
        void save_seed_to_file(const std::string& path);     // 生成新种子，写入文件，派生密钥
    private:
        void generate_identity_key_pair();
        void load_identity_key_pair(const std::string& key_seed_path);
        void derive_keys_from_seed();
        std::vector<uint8_t> identity_seed;                  // 32 字节 Ed25519 种子
        std::vector<uint8_t> do_encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& public_key, const std::vector<uint8_t>& nonce, const std::array<uint8_t, 10>& header);
        std::vector<uint8_t> do_decrypt(const std::vector<uint8_t>& cipher_and_mac, const std::vector<uint8_t> nonce, const std::vector<uint8_t> key, const std::array<uint8_t, 10>& header);

        boost::asio::thread_pool &_thread_pool;
};

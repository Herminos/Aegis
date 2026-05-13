#include<boost/asio.hpp>
#include<boost/asio/use_awaitable.hpp>
#include<sodium.h>
#include<AEGIS/Encryptor.hpp>
#include<AEGIS/Utility.hpp>

using namespace std;
using namespace boost::asio;

Encryptor::Encryptor(thread_pool& pool) : _thread_pool(pool)
{
    sodium_init();
    
    load_identity_key_pair();

}


string Encryptor::get_id() {
    return bin_to_base64(public_key);
}

void Encryptor::generate_identity_key_pair() {
    public_key.resize(crypto_sign_PUBLICKEYBYTES);
    private_key.resize(crypto_sign_SECRETKEYBYTES);
    crypto_sign_keypair(public_key.data(), private_key.data());
}

void Encryptor::load_identity_key_pair() {
    //写一个判断是否有现成的PEM来加载
    generate_identity_key_pair();
}

EphemeralKeyPair Encryptor::generate_ephemeral_keypair() {
    EphemeralKeyPair keypair;

    keypair.public_key.resize(crypto_kx_PUBLICKEYBYTES);
    keypair.private_key.resize(crypto_kx_SECRETKEYBYTES);

    int result = crypto_kx_keypair(
        keypair.public_key.data(), 
        keypair.private_key.data()
    );

    if (result != 0) {
        throw std::runtime_error("[ERROR] Critical failure: Unable to generate ephemeral keypair.");
    }

    return keypair;
}

std::vector<uint8_t> Encryptor::do_encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key, const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> cipher_text(data.size()+crypto_aead_xchacha20poly1305_ietf_ABYTES);
    unsigned long long len;

    int result=crypto_aead_xchacha20poly1305_ietf_encrypt(
        cipher_text.data(),
        &len,
        data.data(),
        data.size(),
        nullptr, 0,
        nullptr,
        nonce.data(),
        key.data()
    );
    if(result!=0){
        throw std::runtime_error("Encryption failed");
    }
    cipher_text.resize(len);
    return cipher_text;
}

std::vector<uint8_t> Encryptor::do_decrypt(const std::vector<uint8_t>& cipher_and_mac, const std::vector<uint8_t> nonce, const std::vector<uint8_t> key) {

    if (cipher_and_mac.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        throw std::runtime_error("[Encryptor] Ciphertext too short to contain a valid MAC.");
    }

    std::vector<uint8_t> plaintext(cipher_and_mac.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES);
    unsigned long long actual_plaintext_len = 0;

    int result=crypto_aead_xchacha20poly1305_ietf_decrypt(
        plaintext.data(),               // [OUT] 存放明文的缓冲区
        &actual_plaintext_len,          // [OUT] 实际解密出的明文长度
        nullptr,                        // [OUT] 这里不用管
        cipher_and_mac.data(),          // [IN]  收到的密文（包含末尾的 MAC）
        cipher_and_mac.size(),          // [IN]  密文总长度
        nullptr,                        // [IN]  附加认证数据 (AD) - 你的协议目前没用到，填 nullptr
        0,                              // [IN]  附加认证数据的长度
        nonce.data(),                   // [IN]  24 字节的 Nonce
        key.data()                      // [IN]  32 字节的 Session Key
    );

    if(result!=0){
        throw std::runtime_error("[Encryptor] MAC verification failed! Payload tampered or invalid context.");
    }

    plaintext.resize(actual_plaintext_len);
    return plaintext;
}

boost::asio::awaitable<std::vector<uint8_t>> Encryptor::async_encrypt(std::vector<uint8_t> data, std::vector<uint8_t> key, std::vector<uint8_t> nonce) {
    auto io_executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::post(_thread_pool, boost::asio::use_awaitable);

    std::vector<uint8_t> cipher_text= do_encrypt(data, key, nonce);
    
    co_await boost::asio::post(io_executor, boost::asio::use_awaitable);
    co_return cipher_text;
}

boost::asio::awaitable<std::vector<uint8_t>> Encryptor::async_decrypt(std::vector<uint8_t> cipher_and_mac, std::vector<uint8_t> nonce, std::vector<uint8_t> key) {
    auto io_executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::post(_thread_pool, boost::asio::use_awaitable);

    std::vector<uint8_t> plain_text= do_decrypt(cipher_and_mac, nonce, key);
    
    co_await boost::asio::post(io_executor, boost::asio::use_awaitable);
    co_return plain_text;
};

SessionKeyPair Encryptor::derive_session_keys(
    bool if_client,
    const std::vector<uint8_t>& peer_ephemeral_pk,
    const std::vector<uint8_t>& my_ephemeral_pk,
    std::vector<uint8_t>& my_ephemeral_sk 
) {
    if (peer_ephemeral_pk.size() != crypto_kx_PUBLICKEYBYTES ||
        my_ephemeral_pk.size() != crypto_kx_PUBLICKEYBYTES ||
        my_ephemeral_sk.size() != crypto_kx_SECRETKEYBYTES) {
        throw std::invalid_argument("[Crypto] Invalid ephemeral key size provided to key derivation.");
    }

    SessionKeyPair keys;
    keys.rx_key.resize(crypto_kx_SESSIONKEYBYTES);
    keys.tx_key.resize(crypto_kx_SESSIONKEYBYTES);

    int kx_result = -1;

    if (if_client) {
        kx_result = crypto_kx_client_session_keys(
            keys.rx_key.data(), keys.tx_key.data(),
            my_ephemeral_pk.data(), my_ephemeral_sk.data(),
            peer_ephemeral_pk.data()
        );
    } else {
        kx_result = crypto_kx_server_session_keys(
            keys.rx_key.data(), keys.tx_key.data(),
            my_ephemeral_pk.data(), my_ephemeral_sk.data(),
            peer_ephemeral_pk.data()
        );
    }

    sodium_memzero(my_ephemeral_sk.data(), my_ephemeral_sk.size());

    if (kx_result != 0) {
        throw std::runtime_error("[INFO]: ECDH Key Exchange failed. Weak peer key or bad entropy.");
    }

    return keys;
}

std::vector<uint8_t> Encryptor::generate_signature(const std::vector<uint8_t>& message, const std::vector<uint8_t>& private_key){

    if (private_key.size() != crypto_sign_SECRETKEYBYTES) {
        throw std::invalid_argument("[Crypto] Invalid Ed25519 secret key size.");
    }

    std::vector<uint8_t> signature(crypto_sign_BYTES);
    unsigned long long sig_len = 0;

    // 3. 执行签名动作
    int result = crypto_sign_detached(
        signature.data(),      // [OUT] 存放签名的缓冲区
        &sig_len,              // [OUT] 实际生成的签名长度
        message.data(),        // [IN]  要签名的原文数据
        message.size(),        // [IN]  原文长度
        private_key.data()    // [IN]  你的长期身份私钥
    );

    if (result != 0) {
        throw std::runtime_error("[Crypto] Critical error: Ed25519 signature generation failed.");
    }

    signature.resize(sig_len);
    return signature;
}
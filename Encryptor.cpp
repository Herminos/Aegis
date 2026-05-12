#include<boost/asio.hpp>
#include<boost/asio/use_awaitable.hpp>
#include<sodium.h>
#include"Encryptor.hpp"

using namespace std;
using namespace boost::asio;

Encryptor::Encryptor(thread_pool& pool) : _thread_pool(pool)
{
    sodium_init();
    
    load_identity_key_pair();

}

std::string bin_to_base64(const std::vector<uint8_t>& bin) {

    size_t b64_len = sodium_base64_encoded_len(bin.size(), sodium_base64_VARIANT_ORIGINAL);
    
    std::string b64_str;
    b64_str.resize(b64_len);

    sodium_bin2base64(b64_str.data(), b64_str.size(), bin.data(), bin.size(), sodium_base64_VARIANT_ORIGINAL);

    if (!b64_str.empty() && b64_str.back() == '\0') {
        b64_str.pop_back();
    }
    return b64_str;
}

std::vector<uint8_t> base64_to_bin(const std::string& b64) {
    if (b64.empty()) {
        return {};
    }

    std::vector<uint8_t> bin(b64.size());
    size_t actual_bin_len = 0;

    int result = sodium_base642bin(
        bin.data(),
        bin.size(),
        b64.data(),
        b64.size(),
        nullptr,
        &actual_bin_len,
        nullptr,
        sodium_base64_VARIANT_ORIGINAL
    );

    if (result != 0) {
        throw std::runtime_error("[Encryptor Error] Invalid Base64 string received.");
    }

    bin.resize(actual_bin_len);
    
    return bin;
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

boost::asio::awaitable<std::vector<uint8_t>> Encryptor::encrypt(const std::vector<uint8_t> &data, const std::vector<uint8_t> &key, const std::vector<uint8_t> &nonce) {
    auto io_executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::post(_thread_pool, boost::asio::use_awaitable);

    std::vector<uint8_t> cipher_text= do_encrypt(data, key, nonce);
    
    co_await boost::asio::post(io_executor, boost::asio::use_awaitable);
    co_return cipher_text;
}


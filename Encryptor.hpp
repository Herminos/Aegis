#include<boost/asio.hpp>
#include<boost/asio/use_awaitable.hpp>
#include<sodium.h>

using namespace std;
using namespace boost::asio;

class Encryptor {
    public:
        Encryptor(thread_pool &pool);
        boost::asio::awaitable<std::vector<uint8_t>> encrypt(const std::vector<uint8_t> &data, const std::vector<uint8_t> &key, const std::vector<uint8_t> &nonce);
        std::vector<uint8_t> public_key;
        std::vector<uint8_t> private_key;
        std::string get_id();
    private:
        void generate_identity_key_pair();
        void load_identity_key_pair();
        std::vector<uint8_t> do_encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& public_key, const std::vector<uint8_t>& nonce);
        
        boost::asio::thread_pool &_thread_pool;
};
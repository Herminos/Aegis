#include<boost/asio.hpp>
#include<stdio.h>
#include<vector>
#include<memory>
#include<functional>


using namespace boost::asio;
using namespace std;



class TcpSender{

    private:
        ip::tcp::socket socket;
        ip::tcp::endpoint remote_endpoint;
        ip::tcp::acceptor acceptor;
        vector<char> recv_buffer;
    public:
        TcpSender(io_context &_io, short port);
        void accept();

};
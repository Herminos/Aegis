#include<boost/asio.hpp>
#include<stdio.h>

using namespace boost::asio;
using namespace std;

class Session:public enable_shared_from_this<Session>
{
    private:
        ip::tcp::socket socket;
        void init();//初始化会话，处理数据收发等逻辑
        void start(); //开始会话，调用init并处理异常等逻辑
    public:
        Session(ip::tcp::socket &_socket);
        
};

Session::Session(ip::tcp::socket &_socket) : socket(move(_socket)) {

}

void Session::init() {
    socket.async_read_some
}
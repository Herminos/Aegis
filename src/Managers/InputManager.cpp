#include<AEGIS/InputManager.hpp>
#include<iostream>
#include<boost/asio.hpp>
#include<thread>

using namespace boost::asio;
using namespace std;

void InputManager::start_input_loop() {
    std::thread([this](){
        input_loop();
    }).detach();
}

void InputManager::input_loop() {
    std::string line;
    while(std::getline(std::cin, line)){
        
        if(line.empty()) continue;
        message_sent_callback(std::move(line));
    }
}
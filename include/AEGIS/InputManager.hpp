#pragma once

#include<stdio.h>
#include<string>
#include<functional>
#include<thread>

class InputManager{

    private:
        std::thread input_thread;
        std::function<void(std::string raw_msg)> message_sent_callback;
        void input_loop();

    public:
        InputManager()=default;

        void set_message_sent_callback(std::function<void(std::string raw_msg)> callback){
            message_sent_callback = callback;
        }
        void start_input_loop();

};
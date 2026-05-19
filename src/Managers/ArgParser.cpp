#include<AEGIS/ArgParser.hpp>
#include<string_view>
#include<vector>
#include<iostream>
#include<AEGIS/Utility.hpp>

void ArgParser::print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --lan PORT        Set into UDP Broadcast Mode (ONLY FOR LAN)" << std::endl;
    std::cerr << "  --listen PORT     Set the host to listen on localhost (ssh -R PORT:127.0.0.1:REMOTE_PORT user@host)" << std::endl;
    std::cerr << "  --connect IP:PORT Set the host to connect to a remote AEGIS server." << std::endl;
    std::cerr << "  --help            Show this help message" << std::endl;
}

AegisConfig ArgParser::parse(int argc, char** argv) {
    AegisConfig config;
    config.is_vaild=false;
    if(argc==1){
        print_usage(argv[0]);
        return config;
    }
    std::vector<std::string_view> args(argv + 1, argv + argc);
    for(int i=0; i<args.size(); i++){

        if(args[i]=="-h" || args[i]=="--help"){
            print_usage(argv[0]);
            return config;
        }
        else if(args[i]=="--lan" && i+1<args.size()){
            config.listen_host="0.0.0.0";
            config.listen_port=std::stoi(std::string(args[++i]));
        }
        else if(args[i]=="-l" || args[i]=="--listen"){
            config.listen_host="127.0.0.1";
            config.listen_port=std::stoi(std::string(args[++i]));
        }
        else if((args[i]=="--connect" || args[i]=="-c") && i+1<args.size()){
            config.is_client=true;
            size_t pos=args[++i].find(':');
            config.connect_host=args[i].substr(0, pos);
            config.connect_port=std::stoi(std::string(args[i].substr(pos+1)));
        }
        else{
            print_usage(argv[0]);
            return config;
        }

    }

    config.is_vaild=true;
    return config;
}
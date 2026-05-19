#include<stdint.h>
#include<string>


struct AegisConfig {
    bool is_vaild=false;
    bool is_client=false;
    std::string listen_host="0.0.0.0";
    uint16_t listen_port=0;

    std::string connect_host;
    uint16_t connect_port;
};


class ArgParser{
    public:
        static AegisConfig parse(int argc, char** argv);
    private:
        static void print_usage(const char* program_name);
};

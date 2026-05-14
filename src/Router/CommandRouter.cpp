#include<AEGIS/CommandRouter.hpp>
#include<AEGIS/Utility.hpp>

using namespace std;

void CommandRouter::handle_raw_input(const string& raw_msg) {
    if(raw_msg.empty()) return;
    if(raw_msg[0]=='/'){
        if(raw_msg == "/list"){
            if(list_all_sessions) list_all_sessions();
            return;
        }
        if(raw_msg == "/current_session"){
            if(list_current_session) list_current_session();
            return;
        }
        if(raw_msg == "/exit"){
            if(exit_aegis_handler) exit_aegis_handler();
        }
        print_info(string("[INFO] Unknown command: ") + raw_msg+". Run /help ");
        return;
    }
    send_message(raw_msg);
}
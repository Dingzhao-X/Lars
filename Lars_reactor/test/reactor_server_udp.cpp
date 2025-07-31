#include "udp_server.h"
#include "config_file.h"
#include <string>
#include <cstring>
#include <iostream>
using namespace std;

void callback_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    cout << "Received and send back." << endl;
    conn->conn_write2fd(data, len, msgid);
}

int main(){
    event_loop loop;

    config_file::setPath("../conf/server.ini");
    string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    uint16_t port = config_file::instance()->GetNumber("reactor", "port", 8888);

    udp_server server(&loop, ip.c_str(), port);

    server.add_msg_router(1, callback_busi);

    loop.event_process();

    return 0;
}



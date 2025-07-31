#include "udp_client.h"
#include <cstring>
#include <iostream>
using namespace std;


void callback_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    cout << "Received data: " << data << endl;
}

int main(){
    event_loop loop;

    udp_client client(&loop, "127.0.0.1", 7777);

    //注册回显业务函数
    client.add_msg_router(1, callback_busi);

    //主动发一个消息
    int msgid = 1;
    const char* msg = "Hello, I'm UDP client.";
    client.conn_write2fd(msg, strlen(msg)+1, msgid);

    loop.event_process();

    return 0;
}

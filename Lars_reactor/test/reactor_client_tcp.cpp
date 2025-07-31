#include "tcp_client.h"
#include <cstring>
#include <iostream>
using namespace std;


void callback_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    //本业务将数据发回。
    cout << "Received and send back." << endl;
    conn->conn_write2fd(data, len, msgid);
}

void print_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    cout << "Receive from server: " << data << " ,msgid = "<< msgid << " ,len = " << len << endl;
}

void on_client_build(net_connection* conn, void* args){
    cout << "===>On_client_build is called!" << endl;
    int msgid = 2;
    const char* msg = "Hello, world!";
    conn->conn_write2fd(msg, strlen(msg), msgid);
}

void on_client_lost(net_connection* conn, void* args){
    cout << "===>On_client_lost is called!" << endl;
    cout << "You have disconncted." << endl;
}

int main(){
    event_loop loop;

    tcp_client client(&loop, "127.0.0.1", 7777);

    //注册回显业务函数
    client.add_msg_router(1, callback_busi);
    client.add_msg_router(2, print_busi);
    client.add_msg_router(200, print_busi);
    client.add_msg_router(404, print_busi);

    //注册链接、断链Hook函数
    client.set_conn_start(on_client_build);
    client.set_conn_close(on_client_lost);

    loop.event_process();

    return 0;
}

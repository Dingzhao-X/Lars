#include "tcp_server.h"
#include "config_file.h"
#include "msg.pb.h"
#include <string>
#include <cstring>
#include <iostream>
using namespace std;

void callback_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    qps_test::echo_message request, response;

    //解包，data中的proto解析出来放入request
    request.ParseFromArray(data, len);

    //回显业务，制作新的protobuf并发送
    //赋值
    response.set_id(request.id());
    response.set_content(request.content());

    //将response序列化
    string response_string;
    response.SerializeToString(&response_string);

    //发送给对端
    conn->conn_write2fd(response_string.c_str(), response_string.size(), msgid);
}

int main(){
    event_loop loop;

    config_file::setPath("../../conf/server.ini");
    string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    uint16_t port = config_file::instance()->GetNumber("reactor", "port", 8888);

    tcp_server server(&loop, ip.c_str(), port);

    //注册回显业务函数
    server.add_msg_router(1, callback_busi);

    pthread_setname_np(pthread_self(), "Main Thread");

    loop.event_process();

    return 0;
}



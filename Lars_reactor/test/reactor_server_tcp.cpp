#include "tcp_server.h"
#include "config_file.h"
#include <string>
#include <cstring>
#include <iostream>
using namespace std;

unique_ptr<tcp_server> server;

//模拟一个异步任务
void print_task(event_loop* loop, void*args){
    cout << "==========Active task callback============" << endl;

    listen_fds fds; //传出参数
    loop->get_listen_fds(fds);//从当前线程loop中获取，每个线程监听fd是不同的

    for(auto fd : fds){
        tcp_conn* conn = tcp_server::conns[fd];     //链接池中取出该链接

        if(conn){
            int msgid = 404;
            const char* msg = "Hello. I'm a task.";
            conn->conn_write2fd(msg, strlen(msg), msgid);
        }
    }
}

void callback_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    //本业务将数据发回。conn此时为tcp_conn，在do_read中调用tcp_server::call
    cout << "Received and send back." << endl;
    conn->conn_write2fd(data, len, msgid);
}

void print_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    cout << "Receive from client: " << data << " ,msgid = "<< msgid << " ,len = " << len << endl;
}

void on_client_build(net_connection* conn, void* args){
    cout << "===>On_client_build is called!" << endl;
    int msgid = 200;
    const char* msg = "Welcome. You are online!";
    conn->conn_write2fd(msg, strlen(msg), msgid);

    server->get_thread_pool()->send_task(print_task);

    //给当前conn绑定一个自定义参数，供回调使用
    const char* conn_param_test = "I'm the conn param for used.";
    conn->param = (void*)conn_param_test;
}

void on_client_lost(net_connection* conn, void* args){
    cout << "===>On_client_lost is called!" << endl;
    cout << "A Connection is lost." << endl;
}

int main(){
    event_loop loop;

    config_file::setPath("../conf/server.ini");
    string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    uint16_t port = config_file::instance()->GetNumber("reactor", "port", 8888);

    server = make_unique<tcp_server>(&loop, ip.c_str(), port);

    //注册回显业务函数
    server->add_msg_router(1, callback_busi);
    server->add_msg_router(2, print_busi);

    //注册链接、断链Hook函数
    server->set_conn_start(on_client_build);
    server->set_conn_close(on_client_lost);

    pthread_setname_np(pthread_self(), "Main Thread");

    loop.event_process();

    return 0;
}



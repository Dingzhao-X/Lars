//tcp_client区别于tcp_conn，主要是包含上connect工作，一个客户端单元。
#pragma once
#include "reactor_buf.h"
#include "event_loop.h"
#include "message.h"
#include "net_connection.h"
#include <arpa/inet.h>
#include <sys/socket.h>

class tcp_client: public net_connection{
    friend void connection_succ(event_loop* _loop, int fd, void* args);
public:
    //构造函数
    tcp_client(event_loop* loop, const char* ip, uint16_t port);

    //发送方法
    virtual int conn_write2fd(const char* data, int msglen, int msgid);

    //处理读业务
    void do_read();
    
    //处理写业务
    void do_write();

    //链接服务器
    void do_connect();

    //释放链接
    void do_disconnect();

    //添加路由的方法，给开发者提供的API
    void add_msg_router(int msgid, msg_callback cb, void* usr_data = NULL){
        _router.register_msg_router(msgid, cb, usr_data);
    }

    //设置连接创建之后的Hook函数。给开发者的API
    void set_conn_start(conn_callback cb, void* args = NULL){
        _conn_start_cb = cb;
        _conn_start_cb_args = args;
    }
    //设置连接创建之后的Hook函数。给开发者的API
    void set_conn_close(conn_callback cb, void* args = NULL){
        _conn_close_cb = cb;
        _conn_close_cb_args = args;
    }
    //Hook函数相关成员变量。肯定是非静态。
    conn_callback _conn_start_cb;
    void* _conn_start_cb_args;
    conn_callback _conn_close_cb;
    void* _conn_close_cb_args;
private:
    //自身cfd
    int _cfd;
    //归属检测的事件堆
    event_loop* _loop;
    //输入缓冲
    input_buf _ibuf;
    //输出缓冲
    output_buf _obuf;
    //客户端ip信息
    struct sockaddr_in _saddr;
    socklen_t _saddrlen;
    //消息分发路由
    msg_router _router;
};


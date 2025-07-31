#pragma once
#include "event_loop.h"
#include "message.h"
#include "net_connection.h"
#include <arpa/inet.h>


class udp_client: public net_connection{
public:
    udp_client(event_loop* loop, const char* ip, uint16_t port);

    //主动发消息方法
    virtual int conn_write2fd(const char* data, int msglen, int msgid);

    //处理客户端消息业务
    void do_read();

    //注册msgid和路由的关系
    void add_msg_router(int msgid, msg_callback cb, void* usrdata = NULL);

    ~udp_client();

private:
    int _sfd;    //udp非面向连接，没有监听、通信fd
                 
    event_loop* _loop;

    //消息路由分发机制
    msg_router _router;

    //没有细分出来的conn，读写缓冲直接在这里
    //UDP不需要缓冲机制，因为每个报文都是原子性的，不需要像tcp处理粘包并adjust
    char _read_buf[MESSAGE_LENGTH_LIMIT];
    char _write_buf[MESSAGE_LENGTH_LIMIT];
};





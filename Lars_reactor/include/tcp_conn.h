#pragma once
#include "event_loop.h"
#include "reactor_buf.h"
#include "net_connection.h"

class tcp_conn: public net_connection{
public:
    //构造函数
    tcp_conn(int cfd, event_loop* loop);
    //被动处理读业务的方法，由事件堆检测到触发
    void do_read();
    //被动处理写业务的方法，由事件堆检测到触发
    void do_write();
    //主动发送消息的方法
    virtual int conn_write2fd(const char*, int, int);
    //销毁当前客户端连接
    void destroy_conn();
private:
    //当前被动接收的cfd
    int _cfd;
    //当前cfd归属于哪个事件堆检测
    event_loop* _loop;
    //输出缓冲区
    output_buf _obuf;
    //输入缓冲区
    input_buf _ibuf;
};


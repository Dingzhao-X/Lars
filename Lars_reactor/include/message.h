#pragma once
#include <unordered_map>
#include <cstdint>
#include <functional>
#include "net_connection.h"
using namespace std;

//一个简单的结构体，定义包头，解决TCP粘包问题。
//UDP为报式协议，要么收要么丢（收到的消息就是数据段，隐去报文头）
struct msg_head{
    int msgid;      //当前消息类型
    int msglen;     //消息体长度
};

//消息头的长度，固定值
#define MESSAGE_HEAD_LEN 8
//消息头+消息体的最大长度限制。防御性工程
#define MESSAGE_LENGTH_LIMIT (65535-MESSAGE_HEAD_LEN)      //udp单包最大64KB

//定义路由回调函数
using msg_callback = function<void(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data)>;

//定义一个消息路由分发机制
class msg_router{
public:
    //构造函数，初始化两个map
    msg_router();

    //注册msgid到对应回调函数的映射
    int register_msg_router(int msgid, msg_callback msg_cb, void* usr_data);

    //调用对应回调函数
    void call(int msgid, uint32_t msglen, const char* data, net_connection* conn);

private:
    //映射一：msgid到回调函数
    unordered_map<int, msg_callback> _msgid2router;
    //映射二：msgid到回调函数形参
    unordered_map<int, void*> _msgid2args;

};

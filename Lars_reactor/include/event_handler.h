#pragma once

// 这里定义io事件“对应触发函数”等信息的封装，并不是事件本身
// 配合fd2handler解决原生 epoll_event 无法直接存储回调的问题。
// epoll_event（内核通知） → 查找 event_handler（业务逻辑） → 执行回调。

class event_loop;   //前置声明，两个头文件不能互相包含

using io_callback = void (event_loop* loop, int fd, void* args);    //定义epoll检测触发的回调

struct event_handler{
    event_handler():mask(0), read_callback(nullptr), write_callback(nullptr), rcb_args(nullptr), wcb_args(nullptr){};

    //事件的位掩码。EPOLLIN、EPOLLOUT
    int mask;
    //读事件触发所绑定的回调函数
    io_callback *read_callback;
    //写事件触发所绑定的回调函数
    io_callback *write_callback;
    //读事件回调函数形参
    void* rcb_args;
    //写事件回调函数形参
    void* wcb_args;
};
















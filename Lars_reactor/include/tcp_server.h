#pragma once
#include <arpa/inet.h>
#include <mutex>
#include <memory>
#include "event_loop.h"
#include "message.h"
#include "tcp_conn.h"
#include "thread_pool.h"

class tcp_server{
public:
    tcp_server(event_loop* loop, const char* ip, uint16_t port);

    //提供创建连接的服务
    void do_accept();
    
    ~tcp_server();

private:
    int _lfd;
    struct sockaddr_in _caddr;
    socklen_t _caddrlen;
    event_loop* _loop;

//===============================消息路由及Hook==============================
public:
    //路由分发机制句柄
    inline static msg_router _router;   

    //添加路由的方法，给开发者的API
    void add_msg_router(int msgid, msg_callback cb, void* usr_data = NULL){
        _router.register_msg_router(msgid, cb, usr_data);
    }

    //设置连接创建之后的Hook函数。给开发者的API
    static void set_conn_start(conn_callback cb, void* args = NULL){
        _conn_start_cb = cb;
        _conn_start_cb_args = args;
    }
    //设置连接创建之后的Hook函数。给开发者的API
    static void set_conn_close(conn_callback cb, void* args = NULL){
        _conn_close_cb = cb;
        _conn_close_cb_args = args;
    }
    //Hook函数相关成员变量
    inline static conn_callback _conn_start_cb = NULL;
    inline static void* _conn_start_cb_args = nullptr;
    inline static conn_callback _conn_close_cb = NULL;
    inline static void* _conn_close_cb_args = nullptr;


//=====================链接池===================
public:
    //所有函数都设为static
    inline static tcp_conn** conns = nullptr;    //链接池。全部在线链接。使用数组查找时间复杂度低，用指针则不浪费内存
    static void increase_conn(int cfd, tcp_conn* conn);     //新增一个链接
    static void decrease_conn(int cfd);         //删除一个链接
    static void get_conn_num(int& cur_conn);    //获取当前链接数量

private:                                            
    inline static int _max_conns = 0;    //当前允许链接的最大数量
    inline static int _cur_conns = 0;   //当前所管理的链接个数
    inline static mutex _mutex;         //保护链接池操作的互斥量

//====================线程池========================
public:
    thread_pool* get_thread_pool() {
        return _thread_pool.get();
    }

private:
    std::unique_ptr<thread_pool> _thread_pool;  //智能指针防止资源泄露
};

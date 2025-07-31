#pragma once
#include <variant>
#include "event_loop.h"

//thread queue消息队列能接收的消息类型

class msg_task{
public:
    //两类task类型：1、新建立链接的任务。2、普通业务任务
    enum TASK_TYPE{
        NEW_CONN,
        NEW_TASK,
    };

    struct busi{    //epoll处理回调
        //void (*task_cb)(event_loop* loop, void* args);
        task_callback cb;
        void* args;
    };

    TASK_TYPE type;     //类成员一

    //任务本身的数据内容。variant是c++17后代替union，前者可以储存类。
    std::variant<int, busi> content;  //类成员二
};




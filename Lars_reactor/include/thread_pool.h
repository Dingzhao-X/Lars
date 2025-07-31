#pragma once 
#include <memory>
#include <vector>
#include "thread_queue.hpp"
#include "msg_task.h"
#include "tcp_conn.h"
#include "event_loop.h"

class thread_pool{
    friend void deal_task(event_loop* loop, int fd, void* args);
public:
    //有参构造，初始化池内多少个工作线程
    thread_pool(int thread_cnt);

    //提供一个获取thread_queue的方法。注意，返回的是消息队列。
    //loop传入工作线程，queue有set_loop方法。主线程只需要推送任务给队列即等于获取一个线程。
    thread_queue<msg_task>* get_thread();

    //发送一个NEW_TASK类型任务的对外接口。主线程业务层调用poll再调用。
    void send_task(task_callback task_cb, void* args = NULL);
private:
    //当前thread_queue的集合，指针数组，注意两次初始化到对象
    //避免使用unique_ptr<**>，需手动删除器
    std::vector<unique_ptr<thread_queue<msg_task>>> _queues;

    //每个工作线程的检测事件堆。
    std::vector<unique_ptr<event_loop>> _loops;

    //线程个数
    int _thread_cnt;

    //线程ID集合，对象数组
    std::vector<pthread_t> _tids;

    //获取线程函数用到的index索引
    int _index;

    std::vector<unique_ptr<tcp_conn>> _conns;
};

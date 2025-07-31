//定义事件堆（一个系统可能有多个事件堆）
#pragma once

#include "event_handler.h"
#include <unordered_map>
#include <unordered_set>
#include <sys/epoll.h>
#define MAX_EVENTS 10
using namespace std;

//优化点之一，建立fd到检测回调的映射
using fd2handler = unordered_map<int, event_handler>;

using listen_fds = unordered_set<int>;

//异步任务回调类型
using task_callback = void (*)(event_loop* loop, void* args);

using ready_tasks = unordered_map<task_callback, void*>;

class event_loop{
public:
    event_loop();

    //循环阻塞监听事件，并处理。事件堆自己调用。
    void event_process();

    //逻辑：所有增删改到事件堆的操作，都要io_handler、epoll_event两步。
    
    //添加一个io事件到事件堆中，或添加一个事件位掩码到已有事件中。
    void add_io_event(int fd, io_callback* io_cb, int mask, void* args);

    //删除一个io事件从事件堆中
    void del_io_event(int fd);

    //删除一个io事件的某个事件位掩码。上个函数的重载版本。
    void del_io_event(int fd, int mask);

    //====================异步任务方法===================
    //添加一个task任务到异步任务集合中
    void add_task(task_callback task_cb, void* args);

    //执行全部异步任务
    void execute_ready_tasks();

    //获取当前loop中监听fd集合
    void get_listen_fds(listen_fds& fds){
        fds = _listen_fds;
    }

private:
    int _epfd;      //epoll_create创建
    
    //当前事件堆中fd到检测函数的映射
    fd2handler _fd2handler;    

    //当前事件堆在检测哪些fd。即wait正在监控哪些fd。
    //作用是服务器可以主动向客户端发消息。以及epoll_wait中便于检测监听fd是否正确
    listen_fds _listen_fds;

    //每次epoll_wait的检测到的事件数组，epoll_ctl传出参数。
    //fired开源项目中的约定俗成，意味就绪。
    struct epoll_event _fired_evs[MAX_EVENTS];

    //异步任务集合
    ready_tasks _ready_tasks;
};








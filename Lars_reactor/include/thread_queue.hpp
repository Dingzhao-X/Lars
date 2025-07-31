#pragma once
#include <queue>
#include <pthread.h>
#include <mutex>
#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>
#include "event_loop.h"
using namespace std;    //queue和mutex都要用

//设为模版类，以防未来任务内容可能不是msg_task
//Tips: 模板类头文件不能分开写,编译时代码才实际生成。
template<typename T>
class thread_queue{
public:
    thread_queue();

    ~thread_queue();
    
    //生产者向队列中加任务（main_thread中调用)
    void send(const T& task);

    //消费者从队列中取数据，将整个queue返回给上层（传出参数），被_evfd激活的读事件业务函数调用
    void recv(queue<T>& queue);

    //设置当前thread_queue被哪个loop监听。loop传入至工作线程，完成绑定。
    void set_loop(event_loop* loop){
        this->_loop = loop;
    }

    //设置_evfd触发读事件的回调函数
    void set_callback(io_callback* cb, void* args = NULL){
        if(_loop){
            _loop->add_io_event(_evfd, cb, EPOLLIN, args);
        }else{
             cerr << "Error: Event loop is null in set_callback" << endl;
        }
    }

private:
    int _evfd;          //事件通知描述符,一个计数器，有新任务时通知工作线程及时处理。和socket无关。
    event_loop* _loop;    //该队列被哪个loop监听。每个工作线程都有一个loop
    queue<T> _queue;    //队列。deque的适配器，用push和pop, 函数名不同
    mutex _mutex;       //保护queue的互斥锁。生产者消费者都可能动队列，不能只看生产者。
};


template<typename T>
thread_queue<T>::thread_queue():_loop(nullptr),_queue(),_mutex(){
    _evfd = eventfd(0, EFD_NONBLOCK);
    if(_evfd == -1){
        cerr << "Init evfd error." << endl;
        return;
    }
}

template<typename T>
thread_queue<T>::~thread_queue(){
    close(_evfd);
    //mutex自动析构。Tips: pthread的锁需要destroy
}

template<typename T>
//生产者向队列中加任务（main_thread中调用)
void thread_queue<T>::send(const T& task){
    lock_guard<mutex> lock(_mutex);
    //将task加入队列中
    _queue.push(task);

    //向_evfd写数据以激活工作线程loop可读事件
    uint64_t evfd_sig = 1;      //必须这个类型，eventfd要求。不在意跨系统可以用unsigned long long
    int ret = write(_evfd, &evfd_sig, sizeof(evfd_sig));
    if(ret == -1)
        cerr << "Evfd write error."<< endl;
}

template<typename T>
//消费者从队列中取数据，将整个queue返回给上层（传出参数），被_evfd激活的读事件业务函数调用
void thread_queue<T>::recv(queue<T>& queue_copy){
    lock_guard<mutex> lock(_mutex);
    
    uint64_t evfd_sig = 0;
    int ret = read(_evfd, &evfd_sig, sizeof(evfd_sig));
    if(ret == -1){
        cerr << "Evfd read error."<< endl;
        return;
    }

    //取出queue操作，通过交换实现。原子操作，并且避免thread_queue阻塞不接收新任务
    swap(queue_copy, _queue);
    //copy(queue_copy, _queue);也可以，但性能没有前者好。
}







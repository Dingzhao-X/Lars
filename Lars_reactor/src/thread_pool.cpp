#include <queue>
#include <iostream>
#include <cstring>
#include "thread_pool.h"
using namespace std;

//一旦有task业务任务过来，loop检测到并执行的回调函数。读出队列里的消息并处理
void deal_task(event_loop* loop, int fd, void* args){
    //1. 从queue中取数据(注意，变量名不能是queue，冲突，否则需要加std::)
    thread_queue<msg_task>* origin_queue = (thread_queue<msg_task>*) args;

    //双缓冲减少锁粒度、减少主线程send阻塞
    queue<msg_task> tmp_queue;
    origin_queue->recv(tmp_queue); //recv内部使用swap，把空线程给到thread_queue继续接收新任务

    //2. 依次处理每个任务
    while(!tmp_queue.empty()){
        //从队列中拿到一个任务，并弹出
        msg_task task = tmp_queue.front();
        tmp_queue.pop();

        if(task.type == msg_task::NEW_CONN){
            //说明为链接业务，取出的数据应该是一个cfd
            tcp_conn* conn = new tcp_conn(get<int>(task.content), loop);
            if(conn == nullptr){
                cerr << "In thread new conn error!"  << endl;
                return;
            }
        }
        else if(task.type == msg_task::NEW_TASK){
            //注意，这里指的其他类型业务，主线程发送给子线程。
            //读写业务在连接成功后即可通信，如Hook的读写，子线程conn和client的通信。
            //将任务添加到loop中，跟随execute执行
            auto task_busi = get<msg_task::busi>(task.content);
            loop->add_task(task_busi.cb, task_busi.args);
        }
        else{
            cerr << "Invalid task type!" << endl;
        }
    }
}

//线程主业务函数
void* thread_main(void*args){
    event_loop* loop = (event_loop*)args;

    loop->event_process();

    return nullptr;
}

//有参构造，初始化池内多少个工作线程
//由于vector不能用负数初始化，在初始化列表这一步就需要参数有效性判断。逗号运算符里最后返回值
thread_pool::thread_pool(int thread_cnt):
    _queues(thread_cnt < 0 ? (cerr << "Invalid thread count. Negative." << endl, exit(1), 0): thread_cnt),
    _loops(thread_cnt),
    _thread_cnt(thread_cnt),
    _tids(thread_cnt),
    _index(0),
    _conns()
{
    //这里不再需要判断thread_cnt是否小于0，初始化列表已判断。

    //开辟_queues指针数组和_tid内存数组也已在初始化列表中完成，vector初始化

    //遍历_queues,_loops并进行线程初始化
    for(int i = 0; i < thread_cnt; ++i){
        //1. 开辟并初始化queue、loop对象。
        //注意：这里事关fd的顺序，谁先初始化，先就在前。但是没有任何运行影响。
        //以下两者反过来会导致fd5是被监听evfd，fd6是子线程epoll。
        _loops[i] = make_unique<event_loop>();
        _queues[i] = make_unique<thread_queue<msg_task>>();  
        //_queue[i] = unique_ptr<thread_queue<msg_task>>(new thread_queue<msg_task>);也可以，但繁琐，写两次类型

        //2. queue绑定到对应loop
        _queues[i]->set_loop(_loops[i].get());
        _queues[i]->set_callback(deal_task, _queues[i].get());

        //3. 开辟线程。启动函数内就是事件堆启动
        //注意，POSIX标准要求新启动的函数必须是void* (*)(void*)，便于返回退出状态，不能是void
        int ret = pthread_create(&_tids[i], 0, thread_main, _loops[i].get());
        if(ret == -1){
            cerr << "Working thread create error." << endl;
            exit(1);
        }

        //设置别名，以下方式也可以
        //string name = "No." + to_string(i+1);
        //pthread_setname_np(_tids[i], name.c_str())); 
        char name[16];
        sprintf(name, "No.%d", i+1);
        pthread_setname_np(_tids[i], name);
        memset(name, 0, sizeof(name));

        cout << "Working thread " << i+1 << " created."<< endl;

        //线程设置detach模式，避免刚需pthread_join
        pthread_detach(_tids[i]);
    }

    cout << "=====================Thread pooll init succ.======================" << endl;
    return;
}

//提供一个循环获取thread_queue的方法
thread_queue<msg_task>* thread_pool:: get_thread(){
    if(_index == _thread_cnt)
        _index = 0;

    cout << "Get working thread num: " << _index+1 << " dealing...."  << endl;

    return _queues[_index++].get();
}

void thread_pool::send_task(task_callback task_cb, void* args){
    msg_task task{msg_task::NEW_TASK, msg_task::busi{task_cb, args}};

    //设定为向每个线程发送任务
    for(int i = 0; i < _thread_cnt; ++i){
        thread_queue<msg_task>* tqueue = _queues[i].get();
        tqueue->send(task);
    }
}


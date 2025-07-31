#include "event_loop.h"
#include <iostream>

event_loop::event_loop(){
    if((_epfd = epoll_create(999)) == -1){
        fprintf(stderr, "Epoll create error.\n");
        exit(1);
    }
}

//循环阻塞监听事件，并处理。事件堆自己调用。                                                                                                 
void event_loop::event_process(){
    while(1){
        //cout << "==============================Waiting IO event...=================================" << endl;
        //获取线程名称
        char thread_name[16];
        pthread_getname_np(pthread_self(), thread_name, size(thread_name));

        //测试时便于观察
        //cerr << "Thread " << thread_name << " is monitoring " << _listen_fds.size() << " counts of fds: " << endl;;
        //for(int x : _listen_fds)    
        //    cout << "fd" << x << "is being listened." << endl;

        int nfds = epoll_wait(_epfd, _fired_evs, MAX_EVENTS, 100);   //nubmer of file descriptors.传出到_fired_evs
        //timeout设为100防止阻塞无法执行异步任务
        for(int i = 0; i < nfds; ++i){
            //从map映射中找到对应事件逻辑
            auto it = _fd2handler.find(_fired_evs[i].data.fd);
            event_handler* hdl = &(it->second);
            
            if(_fired_evs[i].events & EPOLLIN){     //读事件
                void* args = hdl->rcb_args;
                hdl->read_callback(this, _fired_evs[i].data.fd, args);
            }
            else if (_fired_evs[i].events & EPOLLOUT){     //写事件
                void* args = hdl->wcb_args;
                hdl->write_callback(this, _fired_evs[i].data.fd, args);
            }
            else if(_fired_evs[i].events & (EPOLLHUP | EPOLLERR)){  
                //链接挂断、异常崩溃
                //读写，确保不丢包+确认错误类型(0/-1?)
                if(hdl->read_callback != NULL){
                    void* args = hdl->rcb_args;
                    hdl->read_callback(this, _fired_evs[i].data.fd, args);
                }
                else if(hdl->write_callback != NULL){
                    void* args = hdl->wcb_args;
                    hdl->write_callback(this, _fired_evs[i].data.fd, args);
                }else{      //读写掩码都没有，删除
                    cout << "Fd" << _fired_evs[i].data.fd << " :all mask cleared, delete from epoll." << endl;
                    this->del_io_event(_fired_evs[i].data.fd);
                }
            }
        }
        //每次执行完主要io任务后，执行一些其他任务
        //这里是客户端实际执行任务。主线程仅负责推送msg_task，任务由客户端自己管理。
        this->execute_ready_tasks();
    }
}

//添加一个io事件到事件堆中，或添加一个事件位掩码到已有事件中。
void event_loop::add_io_event(int fd, io_callback* io_cb, int mask, void* args){
    int final_mask;
    int op;
    //Tips: 使用mod，event字段会覆盖原先的字段。原生epoll_event不需要保存，上树即可。

    //1.映射中检测当前fd是否已有事件，得到op操作方式。
    auto it = _fd2handler.find(fd);
    if(it == _fd2handler.end()){
        //如果不存在，add方式。
        op = EPOLL_CTL_ADD;
        final_mask = mask;
    }
    else{
        //如果存在，mod方式。
        op = EPOLL_CTL_MOD;
        final_mask = it->second.mask | mask;
    }

    //2.map中添加映射。fd和io_callback绑定。
    if(mask & EPOLLIN){
        _fd2handler[fd].mask = final_mask;
        _fd2handler[fd].read_callback = io_cb;
        _fd2handler[fd].rcb_args = args;
    }
    else{
        _fd2handler[fd].mask = final_mask;
        _fd2handler[fd].write_callback = io_cb;
        _fd2handler[fd].wcb_args = args;
    }

    //3.当前fd加入到正在监听的fd集合中
    _listen_fds.insert(fd);

    //4.原生事件上树。
    struct epoll_event ev;
    ev.events = final_mask;
    ev.data.fd = fd;
    if(epoll_ctl(_epfd, op, fd, &ev) == -1){
        cerr << "Epoll ctl add/mod err." << endl;
        return;
    }
}

//删除一个io事件从事件堆中
void event_loop::del_io_event(int fd){
    auto it = _fd2handler.find(fd);
    if(it == _fd2handler.end()){
        cout << "No such fd." << endl;
        return;
    }

    //在映射中删除该事件。
    _fd2handler.erase(fd);

    //从监听集合中删除该fd。
    _listen_fds.erase(fd);

    //原生事件下树。
    if( epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL) == -1){
        cerr << "Epoll ctl del err" << endl;
        return;
    }
}

//删除一个io事件的某个事件位掩码。上个函数的重载版本。
void event_loop::del_io_event(int fd, int mask){
    auto it = _fd2handler.find(fd);
    if(it == _fd2handler.end()){
        cout << "No such fd." << endl;
        return;
    }

    int final_mask = it->second.mask & (~mask);

    if(final_mask == 0){        //如果事件掩码已经删完
        cout << "No mask left. Delete cfd from epoll." << endl;
        this->del_io_event(fd);
    }else{       //此时就是修改
        struct epoll_event ev;
        ev.events = final_mask;
        ev.data.fd = fd;

        if(epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ev) == -1){
            cerr << "Epoll ctl mod error." << endl;
            return;
        }
    }
}

//添加一个任务到集合中
void event_loop::add_task(task_callback task_cb, void* args){
    if(_ready_tasks.find(task_cb) != _ready_tasks.end())
        cout << "Task callback existed. Covered." << endl;
    
    _ready_tasks[task_cb] = args;
}

//执行全部异步任务
void event_loop::execute_ready_tasks(){
    for(auto it : _ready_tasks){
        it.first(this, it.second);
    }

    //全执行完，清空任务集合
    _ready_tasks.clear();
}



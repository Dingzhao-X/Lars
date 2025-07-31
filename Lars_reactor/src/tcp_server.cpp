#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include "tcp_server.h"
#include "tcp_conn.h"
#include "config_file.h"
using namespace std;

void accept_callback(event_loop* loop, int fd, void* args);

//=======================链接相关函数========================

void tcp_server::increase_conn(int cfd, tcp_conn* conn){
    lock_guard<mutex> lock(_mutex);
    conns[cfd]= conn;
    ++_cur_conns;
}

void tcp_server::decrease_conn(int cfd){
    lock_guard<mutex> lock(_mutex);
    conns[cfd] = nullptr;
    --_cur_conns;
}

void tcp_server::get_conn_num(int& cur_conn){   //传出参数
    lock_guard<mutex> lock(_mutex);
    cur_conn = _cur_conns;
}
                                        
//===============================================================

//构造函数
tcp_server::tcp_server(event_loop* loop, const char* ip, uint16_t port): 
    _lfd(-1), _caddr(), _caddrlen(sizeof(_caddr)), _loop(loop)
{
    //0.忽略一些信号， 防止进程中断
    //  SIGHUP向断开的客户端发送数据，SIGPIPE向关闭的管道写数据
    if(signal(SIGHUP, SIG_IGN) == SIG_ERR)
        cerr << "Signal ingore SIGHUP error." << endl;

    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Signal ingore SIGPIPE error." << endl;

    //1.创建监听套接字
    _lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (_lfd == -1) {
        cerr << "Lfd socket create error." << endl;
        exit(1);
    }

    //2.绑定端口
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &saddr.sin_addr);

    //2.5设置_lfd可以重复监听（解决timewait2状态）
    int op = 1;
    if(setsockopt(_lfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op)) == -1)
        cerr << "Setsocket reusedaddr error." << endl;

    if(bind(_lfd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1){
        cerr << "Lfd bind error." << endl;
        exit(1);
    }

    //3.开始监听
    if(listen(_lfd, 128) == -1){
        cerr << "Listen error." << endl;
        exit(1);
    }

    //4.创建线程池
    int thread_cnt = config_file::instance()->GetNumber("reactor", "threadNums", 5);
    _thread_pool = make_unique<thread_pool>(thread_cnt);    //构造函数里已经有了cnt有效性判断
    if(_thread_pool == nullptr){
        cerr << "Thread pool init error." << endl;
        exit(1);
    }

    //5.创建链接管理
    _max_conns = config_file::instance()->GetNumber("reactor", "maxConns", 20);  

    conns = new tcp_conn*[_max_conns + 5 + 2*thread_cnt];   //标准fd*3, 主线程epoll, lfd + (epoll+evfd)*工作线程数
    if(!conns){
        cerr << "new conns" << _max_conns << "error" << endl;
        exit(1);
    }                                           

    //6.注册lfd读事件。调用do_accpet的就是server，所以参数就是this
    _loop->add_io_event(_lfd, accept_callback, EPOLLIN, this);


    cout << "******************TCP server create succ. Ip:" << ip << " ,port:" << port << "******************"<< endl;
}

//提供创建连接的服务
void tcp_server::do_accept()
{
    int cfd = -1;
    while(1){
        cout << "Start accepting." << endl;
        cfd = accept(_lfd, (struct sockaddr*)&_caddr, &_caddrlen);
        if(cfd == -1){
            if(errno == EINTR){    //非致命信号，可恢复继续。如SIGALRM，SIFCHLD
                cerr << "Accept errno = EINTR." << endl;
                continue;
            }
            else if(errno == EAGAIN){   //循环的出口，无论是LT还是ET
                cerr << "Accept errno = EAGAIN." << endl;
                break;
            }
            else if(errno == EMFILE){
                cerr << "Accept errno = EMFILE." << endl;
                continue;
            }
            else{
                cerr << "Accept error." << endl;
                exit(1);
            }
        }
        else{
            //判断链接个数是否已超最大值
            int cur_conns;
            get_conn_num(cur_conns);
            if(cur_conns >= _max_conns){
                cerr << "Too much connections. Max: " << _max_conns;
                close(cfd);
            }
            else{
                //============新链接将由线程池处理===========
                if(_thread_pool){  
                    //多线程模式
                    //1. 获得一个线程来处理
                    thread_queue<msg_task>* thread_queue = _thread_pool->get_thread();
                    //2. 创建一个新链接的消息任务
                    msg_task task{msg_task::NEW_CONN, cfd};
                    //3. 添加到消息队列中，对应event_loop检测evfd读事件，让thread来处理
                    thread_queue->send(task);
                }
                else{        //这里应该是初始化线程池失败，不用exit而是throw的情况
                    //启动单线程模式
                    tcp_conn* conn= new tcp_conn(cfd, _loop);
                    if(conn == NULL){
                        cerr << "New tcp_conn error!" << endl;
                        break;
                    }
                }
            }
            break;  //水平模式会卡在accept，eagain是到不了的，必须break
        }
    }
}
    
//析构函数，资源释放
tcp_server::~tcp_server()
{
    close(_lfd);
}

void accept_callback(event_loop* loop, int fd, void* args){
    tcp_server* server = (tcp_server*)args;
    server->do_accept();
}


#include "tcp_server.h"
#include "tcp_conn.h"
#include "message.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstring>
using namespace std;

//这两个函数仅为了满足io_callback签名，所以参数是this直接调用相应函数
void conn_rd_callback(event_loop* loop, int fd, void* args){
    tcp_conn* conn = (tcp_conn*)args;
    conn->do_read();
}

void conn_wt_callback(event_loop* loop, int fd, void* args){
    tcp_conn* conn = (tcp_conn*)args;
    conn->do_write();
}

//构造函数
tcp_conn::tcp_conn(int cfd, event_loop* loop):_cfd(cfd), _loop(loop){
    //1. 将cfd设置为非阻塞状态
    int flag = fcntl(_cfd, F_GETFL);
    fcntl(_cfd, F_SETFL, flag|O_NONBLOCK);

    //2. 设置tcp_nodelay状态，禁止读写缓存，降低小包延迟
    //一般读写都要缓存满一定大小才发送，这里禁止，即使1字节。游戏如lol是必设置的。
    int op = 1;
    setsockopt(_cfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));    //需要netinet两个头文件

    //3. 执行链接成功的Hook函数
    if (tcp_server::_conn_start_cb != NULL) 
        tcp_server::_conn_start_cb(this, tcp_server::_conn_start_cb_args);

    //4. 将当前读事件加入事件堆检测
    _loop->add_io_event(_cfd, conn_rd_callback, EPOLLIN, this); 

    //将自己添加到 tcp_server中的conns集合中
    tcp_server::increase_conn(_cfd, this);
}

//被动处理读业务的方法，由事件堆检测到触发
void tcp_conn::do_read(){
    //1. 从cfd中读数据
    int ret = _ibuf.read_data(_cfd);
    if(ret == -1){
        cerr << "Read data from cfd error." << endl;
        this->destroy_conn();
        return;
    }
    else if(ret == 0){
        cout << "Cfd closed. Read failure." << endl;
        this->destroy_conn();
        return;
    }

    //2. 判断长度是否足够有效，要大于等于消息头（8字节）
    //字节序问题应该一取出就转换小端。这个判断完就立刻转换
    //注：使用protobuf后自动处理
    msg_head head;
    while(_ibuf.length() >= MESSAGE_HEAD_LEN){
        //2.1 先读头部，得到msgid,msglen。如果记录的长度已经非法，关闭。
        memcpy(&head, _ibuf.data(), MESSAGE_HEAD_LEN);
        //2.2 立刻转换小端。大端的话，是无法正常使用的
        head.msgid = ntohl(head.msgid);
        head.msglen = ntohl(head.msglen);
        if(head.msglen > MESSAGE_LENGTH_LIMIT || head.msglen < 0){
            cerr << "Invalid data. Too large or negative. Close cfd." << endl;
            this->destroy_conn();
            break;
        }

        //2.3 判断实际_ibuf中数据长度和头部里记录的长度是否一致。
        if(_ibuf.length() < MESSAGE_HEAD_LEN + head.msglen){
            //缓存中buf剩余的数据，应该小于该接收的数据。
            //说明这不是一个完整的包，继续接收。
            break;
        }

        //弹出消息头长度
        _ibuf.pop(MESSAGE_HEAD_LEN);

        //3. 处理业务数据
        //执行回显任务
        tcp_server::_router.call(head.msgid, head.msglen, _ibuf.data(), this); //this是tcp_conn对象

        //整个消息处理完了，弹出
        _ibuf.pop(head.msglen);
    }
    //回收已消费数据
    _ibuf.adjust();
    return;
}

//被动处理写业务的方法，由事件堆检测到触发
void tcp_conn::do_write(){
    //do write就表示_obuf中已经有要写的数据，将_obuf中的数据发送给fd，给到对端
    while(_obuf.length()){
        int ret = _obuf.write2fd(_cfd);
        if(ret == -1){
            cerr << "Tcp_conn write cfd error." << endl;
            this->destroy_conn();
            return;
        }
        else if(ret == 0){
            //当前不可写
            break;
        }
    }

    if(_obuf.length() == 0){
        //数据已经全部写完，将cfd的写事件删掉
        _loop->del_io_event(_cfd, EPOLLOUT);
    }
    
    return;
}

//主动发送消息的方法。
int tcp_conn::conn_write2fd(const char* data, int msglen, int msgid){
    //用于判断是否需要添加cfd的写事件回调。回调是io层到fd。
    //因为如果_obuf不为空，说明还有之前的数据没写到对端，那么就没必要再激活，写完再激活
    bool active_epollout = false;
    if(_obuf.length() == 0){
        active_epollout = true;
    }    
    //1. 封装一个消息头,并写到_obuf中
    msg_head head{msgid, msglen};
    
    //1.1 在进入缓冲区前，要转换大端。protobuf不需要。
    //虽然本项目其实不写也不产生问题，因为双方都小端
    head.msgid = htonl(msgid);
    head.msglen = htonl(msglen);

    int ret = _obuf.write2buf((const char*)&head, MESSAGE_HEAD_LEN);
    if(ret != 0){
        cerr << "Server send head error." << endl;
        return -1;
    }

    //2. 写消息体
    ret = _obuf.write2buf(data, msglen);
    if(ret != 0){
        cerr << "Server send data error." << endl;
        //如果消息体写失败，消息头也要弹出
        _obuf.pop(MESSAGE_HEAD_LEN);
        return -1;
    }

    //3. 将cfd添加写事件EPOLLOUT，回调会将_obuf中的数据写给对端。
    if(active_epollout == true)  _loop->add_io_event(_cfd, conn_wt_callback, EPOLLOUT, this);

    return 0;
}

//销毁当前客户端连接
void tcp_conn::destroy_conn(){
    //执行链接销毁的Hook函数
    if (tcp_server::_conn_close_cb != NULL) 
        tcp_server::_conn_close_cb(this, tcp_server::_conn_close_cb_args);

    //将tcp_server中 把当前连接摘除
    tcp_server::decrease_conn(_cfd);

    //各种清理工作。下树、归还内存、关闭cfd
    _loop->del_io_event(_cfd);

    _ibuf.clear();
    _obuf.clear();

    close(_cfd);
}


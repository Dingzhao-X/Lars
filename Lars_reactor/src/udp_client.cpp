#include "udp_client.h"
#include <iostream>
#include <signal.h>
#include <errno.h>
#include <cstring>
using namespace std;

//epoll回调函数格式包装
void read_callback(event_loop* loop, int sfd, void* args){
    udp_client* client = (udp_client*)args;
    client->do_read();
}


udp_client::udp_client(event_loop* loop, const char* ip, uint16_t port): 
    _sfd(-1),_loop(loop), _router(), _read_buf{}, _write_buf{}{
        // 创建套接字
        _sfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
        if(_sfd == -1){
            cerr << "Create UDP client fd error." << endl;
            exit(1);
        }

        //先初始化服务器地址
        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        inet_pton(AF_INET, ip, &saddr.sin_addr);
        saddr.sin_port = htons(port);

        //连接服务器
        int ret = connect(_sfd, (const struct sockaddr*)&saddr, sizeof(saddr));
        if(ret == -1){
            cerr << "Connect error." << endl;
            exit(1);
        }

        //_sfd读事件上树
        _loop->add_io_event(_sfd, read_callback, EPOLLIN, this);

        cout << "UDP client connect succ. Ip: " << ip << " ,port:" << port << endl;
    }

//主动发消息方法
int udp_client::conn_write2fd(const char* data, int msglen, int msgid){
    memset(_write_buf, 0, sizeof(_write_buf));
    if(msglen > MESSAGE_LENGTH_LIMIT){
        cerr << "Send message too large." << endl;
        return -1;
    }        
    
    msg_head head{msgid, msglen};
    
    //写数据到缓冲中
    memcpy(_write_buf, &head, MESSAGE_HEAD_LEN);
    memcpy(_write_buf + MESSAGE_HEAD_LEN, data, msglen);

    //发送给对端
    int ret =  sendto(_sfd, _write_buf, msglen + MESSAGE_HEAD_LEN, 0, NULL, 0);
    if(ret == -1){
        cerr << "Send to _sfd error." << endl;
        return -1;
    }
    return ret;
}

//处理客户端消息业务
//udp和tcp不一样，不会有粘包问题，UDP是有边界的离散数据包。
//每个 sendto() 对应一个 recvfrom() 完整接收。不存在部分接收，要么就丢弃（不会交付半个包）
//ret = recvfrom(sockfd, buffer, MAX_SIZE, 0, &src_addr, &addr_len);
void udp_client::do_read(){
    while(1){
        int pkg_len = recvfrom(_sfd, _read_buf, sizeof(_read_buf), 0, NULL, 0);
        if(pkg_len == -1){
            if(errno == EINTR)  //中断错误,
                continue;
            else if(errno == EAGAIN) //非阻塞
                break;
            else{
                cerr << "UDP client recvfrom error." << endl;
                break;
            }
        }

        msg_head head;
        //得到消息头（事实上udp应该隐去）
        memcpy(&head, _read_buf, MESSAGE_HEAD_LEN);

        if(head.msglen > MESSAGE_LENGTH_LIMIT || head.msglen < 0 || head.msglen + MESSAGE_HEAD_LEN != pkg_len){
            cerr << "Received invalid data." << endl;
            break;
        }

        _router.call(head.msgid, head.msglen, _read_buf + MESSAGE_HEAD_LEN, this);
        memset(_read_buf, 0, sizeof(_write_buf));
    }
}




//注册msgid和路由的关系
void udp_client::add_msg_router(int msgid, msg_callback cb, void* usr_data){
    _router.register_msg_router(msgid, cb, usr_data);
}

udp_client::~udp_client(){
    _loop->del_io_event(_sfd);
    close(_sfd);
}



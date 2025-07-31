#include "tcp_client.h"
#include "message.h"
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <errno.h>
using namespace std;

//包装成回调需要签名
void cli_rd_callback(event_loop* loop, int fd, void* args){
    tcp_client *cli = (tcp_client*)args;
    cli->do_read();
}

void cli_wt_callback(event_loop* loop, int fd, void* args){
    tcp_client *cli = (tcp_client*)args;
    cli->do_write();
}

//构造函数
tcp_client::tcp_client(event_loop* loop, const char* ip, uint16_t port):
    _cfd(-1), _loop(loop), _ibuf(), _obuf(), _router() {
        //封装客户端ip地址信息
        _saddr.sin_family = AF_INET;
        _saddr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &_saddr.sin_addr.s_addr);
        _saddrlen = sizeof(_saddr);

        //链接客户端
        this->do_connect();
    }

//发送方法
int tcp_client::conn_write2fd(const char* data, int msglen, int msgid){
    bool active_epollout = false;

    if(_obuf.length() == 0)    active_epollout = true;

    msg_head head;
    head.msgid = htonl(msgid);
    head.msglen = htonl(msglen);

    //1. 将消息头写到obuf中
    int ret = _obuf.write2buf((const char*)&head, MESSAGE_HEAD_LEN);
    if(ret != 0){
        cerr << "Client write2buf head  error." << endl;
        return -1;
    }
    //2. 将消息体写到obuf中
    ret = _obuf.write2buf(data, msglen);
    if(ret != 0){
        cerr << "Client write2buf data error." << endl;
        //如果写消息体失败，消息头也要弹出
        _obuf.pop(MESSAGE_HEAD_LEN);
        return -1;
    }

    if(active_epollout)   
        _loop->add_io_event(_cfd, cli_wt_callback, EPOLLOUT, this);

    return 0;
}

//处理读业务
void tcp_client::do_read(){
    //1. 从_cfd中读数据
    int ret = _ibuf.read_data(_cfd);
    if(ret == -1){
        cerr << "Client read data error." << endl;
        this->do_disconnect();
        return;
    }
    else if(ret == 0){
        cout << "Server closed." << endl;
        this->do_disconnect();
        return;
    }
    //2. 判断读出来数据的合法性
    msg_head head;
    while(_ibuf.length() >= MESSAGE_HEAD_LEN){
        //2.1 先读头部，得到msgid、msglen,并且立刻转换为小端
        memcpy(&head, _ibuf.data(), MESSAGE_HEAD_LEN);
        head.msgid = ntohl(head.msgid);
        head.msglen = ntohl(head.msglen);
        //msglen如果已经非法，退出并断开连接，防御性工程
        if(head.msglen > MESSAGE_LENGTH_LIMIT || head.msglen < 0){
            cerr << "Invalid data. Too large or negative. Close cfd." << endl;
            this->do_disconnect();
            break;
        }

        //2.2 判断实际缓冲接受长度和头部记录是否一致
        if(_ibuf.length() < MESSAGE_HEAD_LEN + head.msglen){
            //缓冲实际长度比记录的小，说明不是一个完整的包，继续接收。此处不应断开连接。
            break;
        }

        //弹出消息头长度
        _ibuf.pop(MESSAGE_HEAD_LEN);

        //3，执行注册的回显业务
        this->_router.call(head.msgid, head.msglen, _ibuf.data(), this);

        //弹出消息体长度
        _ibuf.pop(head.msglen);
    }
    _ibuf.adjust();

    return;
}

//处理写业务
void tcp_client::do_write(){
    while(_obuf.length()){
        int ret = _obuf.write2fd(_cfd);
        if(ret == -1){
            cerr << "Client write2fd error." << endl;
            this->do_disconnect();
            return;
        }
        else if(ret == 0){
            cout << "Obuf full, EAGAIN." <<  endl;
            break;  //当前不可写，obuf满，即EAGAIN
        }

        //数据全部写完，_cfd事件的读掩码删掉
        if(_obuf.length() == 0)      
            _loop->del_io_event(_cfd, EPOLLOUT);

    }

    return;
}

//检测链接创建成功的函数，成功执行即为成功。
//这里是普通函数，可以作为回调，非静态成员函数不能作为回调
void connection_succ(event_loop* loop, int cfd, void* args){
    tcp_client* cli = (tcp_client*)args;
    //先前写检测即为了检查EINPROGRESS的临时事件，调起本函数，现删除。
    loop->del_io_event(cfd);

    //再对当前cfd进行一次错误码获取，如果没有任何错误，那么就意味着成功。如果有，即失败
    int result = 0;
    socklen_t result_len = sizeof(result);
    getsockopt(cfd, SOL_SOCKET, SO_ERROR, &result, &result_len);     //result传出参数。0表示成功，非0是错误码
        
    //用于后面输出一下服务器ip信息。仅查看用。
    char ip[20];
    inet_ntop(AF_INET, &cli->_saddr.sin_addr, ip, sizeof(ip));
    uint16_t port = ntohs(cli->_saddr.sin_port);

    if(result == 0){
        //创建成功
        //执行连接成功后的Hook业务。
        if(cli->_conn_start_cb != NULL)
            cli->_conn_start_cb(cli, cli->_conn_start_cb_args);
        
        cerr << "******************Client connection succ. Server IP: " << ip << ", port:" << port 
            <<"******************"<< endl;

        //添加cfd的读回调检测
        loop->add_io_event(cfd, cli_rd_callback, EPOLLIN, cli);

        if(cli->_obuf.length() != 0){
            loop->add_io_event(cfd, cli_wt_callback, EPOLLOUT, cli);
        }
    }
    else{
        //创建链接失败
        cerr << "Client connection failed. Server IP: " << ip << ", port:" << port << endl;
        return;
    }
}

//链接服务器
void tcp_client::do_connect(){
    //如果已经有一个有效套接字，关闭，否则后面赋值时造成资源泄露
    if(_cfd != -1){
        cerr << "Cfd already exited. Previous closed." << endl;
        close(_cfd);
    }

    //创建套接字，设置非阻塞模式
    _cfd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0);
    if(_cfd == -1){
        cerr << "Client cfd create error." << endl;
        exit(1);
    }

    int ret = connect(_cfd, (const struct sockaddr*)&_saddr, _saddrlen);
    if(ret == -1){
        if(errno == EINPROGRESS){
            //非阻塞模式下connect会产生的错误码，表示可能还在三次握手。
            cout << "Client connect in progress....." << endl;
            //需要检测cfd是否可写，可写就是成功了。
            //直接添加到事件堆，设置一个回调函数。
            _loop->add_io_event(_cfd, connection_succ, EPOLLOUT, this);
        }
        else{
            cerr << "Client connect error." << endl;
            exit(1);
        }
    }else{
        //创建链接成功
        char ip[20];
        inet_ntop(AF_INET, &_saddr.sin_addr, ip, sizeof(ip));
        uint16_t port = ntohs(_saddr.sin_port);
        cout << "Client connect succ. Server IP: " << ip << ", port: " << port << endl;

        connection_succ(_loop, _cfd, this);
    }
}


//释放链接
void tcp_client::do_disconnect(){
    if(_conn_close_cb != NULL)
        _conn_close_cb(this, _conn_close_cb_args);

    if(_cfd != -1){
        _loop->del_io_event(_cfd);
        close(_cfd);
    }
    else    
        cout << "Client already disconnected." << endl;
}





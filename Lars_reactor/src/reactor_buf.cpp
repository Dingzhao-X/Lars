#include "reactor_buf.h"
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <iostream>
using namespace std;


reactor_buf::reactor_buf(){
    _buf = nullptr;
}

reactor_buf::~reactor_buf(){
    this->clear();
}

//得到当前buf还有多少有效数据
int reactor_buf::length(){
    if(!_buf)   return 0;
    else    return _buf->length;
}

//将处理完的数据弹出
void reactor_buf::pop(int len){
    if(_buf && len > _buf->length){
        cerr << "Io_buf pop error!" << endl;
        return;
    }

    _buf->pop(len);

    if(_buf->length == 0)    this->clear();  //当前buf用完，归还，下次再取
}

//将当前buf清空，并归还到池子
void reactor_buf::clear(){
    if(_buf){
        buf_pool::get_instance()->revert(_buf);
        _buf = nullptr;
    }
}

//===========================================================================================
//从一个fd中读取数据到io_buf中（fd到io层。在业务层处理数据）
int input_buf::read_data(int fd){
    int need_read = 0;     //硬件中有多少数据是可读

    /* 一次将io中所有缓存读出来。传出参数：目前socket缓冲中一共有多少数据可读。
     * 注意：如果返回-1，通常是大问题，并且need_read不会修改。如果半关闭，只关闭写，那应该在read处返回0.
     * */
    if(ioctl(fd, FIONREAD, &need_read) == -1){        
        cerr << "Ioctl FIOREAD error." << endl;
        return -1;
    }

    //内存缓冲区准备
    if(!_buf){    //如果当前input_buf的_buf为空，从buf_pool取一个新的
        _buf = buf_pool::get_instance()->alloc_buf(need_read);
        if(!_buf){
            cerr <<  "No new buf to alloc!" << endl;
            return -1;
        }
    }
    else{              //如果当前有buf可用，判断是否够存
        //有效数据头指针必须在内存块头部，防止有内存空洞（已消费未回收）
        //后read是写到_buf->data + _buf->length位置, head有效数据必须在头部。
        if(_buf->head != 0){
            cerr << "Read failed. Used data not poped." << endl;
            return -1;
        }

        if(_buf->capacity - _buf->length < need_read){  //不够存，取一块新的把新旧一起放进去
            io_buf* new_buf = buf_pool::get_instance()->alloc_buf(need_read + _buf->length);
            if(!new_buf){
                cerr <<  "No new buf to alloc!" << endl;
                return -1;
            }
            new_buf->copy(_buf);
            buf_pool::get_instance()->revert(_buf);
            _buf = new_buf;
        }
    }

    //读取数据（替代read，fd到io层）
    //注意，此处不是循环读取, while只是处理EINTR情况
    int already_read = 0;
    do{
        if(need_read == 0){
            already_read = read(fd, _buf->data + _buf->length, m4K);    //选一个最小的读，非阻塞模式是不会阻塞的。
        }else{
            already_read = read(fd, _buf->data + _buf->length, need_read);  
        }
    }while(already_read == -1 && errno == EINTR);   //良性，继续读取


    /*
     * 虽然非阻塞判断EAGAIN是POSIX标准的标准规定，不应该是可选行为
     * 但，这里不需要，因为io_buf会确保有足够空间(message.h限制了报文大小)，且Reactor本身就是确保了有数据才会来读。
     * */

    if(already_read > 0){
        //防止异常。前面need_read获取大小，到读之前可能因为网络中断、信号打断，只能读到一部分。
        if(already_read != need_read){
            cerr << "Unexpected read error!" << endl;
            return -1;
        }
        //读取数据成功
        //io缓冲区是累计的，上面read就是从length开始读的，用+=
        _buf->length += already_read;
    }

    //和output_buf不一样的是，这里还在io层，尚未处理，在tcp_conn, tcp_client中拿到ibuf.data()再弹出+回收。

    return already_read;
}

//获取当前数据
const char* input_buf::data(){
    return _buf ? _buf->data + _buf->head : NULL;
}

//回收已消费数据
void input_buf::adjust(){
    if(_buf)
        _buf->adjust();
}

//===========================================================================================
//将一段数据写到io_buf中（业务层到io层交界处）。
int output_buf::write2buf(const char* data, int datalen){
    //内存缓冲区准备
    if(!_buf){         //当前output_buf的_buf为空，从buf_pool取一个新的
        _buf = buf_pool::get_instance()->alloc_buf(datalen);
        if(!_buf){
            cerr <<  "No new buf to alloc!" << endl;
        return -1;
        }
    }
    else{       //如果当前有buf可用，判断是否够存
        if(_buf->head != 0){
            cerr << "Read failed. Used data not poped." << endl;
            return -1;
        }
        if(_buf->capacity - _buf->length < datalen){  //不够存，取一块新的把新旧一起放进去
            io_buf* new_buf = buf_pool::get_instance()->alloc_buf(datalen + _buf->length);
            if(!new_buf){
                cerr <<  "No new buf to alloc!" << endl;
                return -1;
            }
            new_buf->copy(_buf);
            buf_pool::get_instance()->revert(_buf);
            _buf = new_buf;
        }
    }
    
    //将data数据写到io_buf中，拼接到后面
    memcpy(_buf->data + _buf->length, data ,datalen);
    _buf->length += datalen;

    return 0;
}

//将io_buf中数据写到fd中。取代write（io层到内核）。
int output_buf::write2fd(int fd){
    int already_write = 0;

    do{
        already_write = write(fd, _buf->data, _buf->length);    
    }while(already_write == -1 && errno == EINTR);

    if(already_write > 0){
        //写成功，弹出已消费数据，再回收已消费数据。
        //因为这个函数也是每次从_buf->data开始读到fd(和上一个函数无关，上一个函数可以在io层拼接，一起写入fd)
        _buf->pop(already_write);
        _buf->adjust();
    }

    //如果fd是非阻塞的，可能写内存满无法写入
    if(already_write == -1 && errno == EAGAIN){
        already_write = 0;      //不是一个错误，表示为写0字节。
    }
    
    return already_write;
}




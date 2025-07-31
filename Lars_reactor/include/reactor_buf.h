#pragma once 
#include "io_buf.h"
#include "buf_pool.h"

//双缓冲策略，封装io_buf类，实现无锁读写竞争。
//缓冲意义类似buffer_event缓冲区

//父类，有共同的基于io_buf的性质。
class reactor_buf{
public:
    reactor_buf();
    ~reactor_buf();

    //当前buf有多少有效数据
    int length();

    //将已消费数据弹出
    void pop(int len);

    //将当前buf清空，并归还到内存池
    void clear();
protected:
    io_buf* _buf;
};


class input_buf : public reactor_buf{
public:
    //从一个fd中读取数据到io_buf中，取代read（内核到io层）
    int read_data(int fd);

    //获取当前数据
    const char* data();

    //回收已消费数据
    void adjust();

};


class output_buf : public reactor_buf{
public:
    //将一段数据写到io_buf中（业务层到io层）。
    int write2buf(const char* data, int dalaten);

    //将io_buf中数据写到fd中。取代write（io层到内核）。
    int write2fd(int fd);
};



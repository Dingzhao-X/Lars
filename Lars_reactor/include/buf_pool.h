#pragma once
#include "io_buf.h"
#include <mutex>
#include <unordered_map>
using namespace std;

//内存池单例模式

//总内存大小上限，单位kb
#define MEM_LIMIT (5U*1024*1024)

using pool_t = unordered_map<int, io_buf*>;

//定义一些内存刻度
enum MEM_CAP{
    m4K = 4096,
    m16K = 16384,
    m64K = 65536,
    m256K = 262144,
    m1M = 1048576,
    m4M = 4194304,
    m8M = 8388608,
};

class buf_pool{
public:
    static buf_pool* get_instance(){
        //一个确保一段代码只能用一次的锁，第一位参数标志位。
        //代替pthread_once
        call_once(_once_flag, [](){
                  _instance = new buf_pool;
                  });
        return _instance;
    }

    //申请一块内存
    io_buf *alloc_buf(int N);

    //将一个io_buf放回pool中
    void revert(io_buf*buffer);

    //生成pool池复用
    void make_io_buf_list(int cap, int num);

private:
    //=======================1.单例模式==========================
    buf_pool();
    buf_pool(const buf_pool&) = delete;
    const buf_pool& operator=(const buf_pool&) = delete;
    buf_pool(const buf_pool&&) = delete;
    const buf_pool&& operator=(const buf_pool&&) = delete;
    static buf_pool* _instance;
    //这是一个类，call_once中的flag。
    static once_flag _once_flag;

    //===================2.pool内存池属性==========================
    //存放所有io_buf的map句柄，总内存池
    pool_t _pool;

    //当前内存池大小，单位kb
    uint64_t _mem_capacity;

    //确保多线程操作pool增删改时线程安全的锁
    static mutex _mutex;
};












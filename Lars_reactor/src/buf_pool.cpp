#include "buf_pool.h"
#include <iostream>
using namespace std;

buf_pool* buf_pool:: _instance = nullptr;
mutex buf_pool::_mutex;
once_flag buf_pool::_once_flag;

//初始化pool池复用
void buf_pool::make_io_buf_list(int cap, int num){
    //用于创建内存链表的指针
    io_buf* pcur;

    //创建头节点
    _pool[cap] = new io_buf(cap);
    if(!_pool[cap]){
        cerr << "New io_buf " << cap << " create error."<< endl;
        exit(1);
    }
    
    _mem_capacity += cap/1024;
    pcur = _pool[cap];

    //循环开辟后续节点
    for(int i = 1; i < num; ++i){
        pcur->next = new io_buf(cap);
        if(!_pool[cap]){
            cerr << "New io_buf " << cap << " create error."<< endl;
            exit(1);
        }
        pcur = pcur->next;
        _mem_capacity += cap/1024;
    }
}

//构造函数
buf_pool::buf_pool():_mem_capacity(0){
    make_io_buf_list(m4K, 5000);
    make_io_buf_list(m16K, 1000);
    make_io_buf_list(m64K, 500);
    make_io_buf_list(m256K,200);
    make_io_buf_list(m1M, 50);
    make_io_buf_list(m4M, 20);
    make_io_buf_list(m8M, 10);
}

//申请一块内存
io_buf*  buf_pool::alloc_buf(int N){
    int index = [&]()->int {        // lambda所有的返回类型必须一致,int不能隐式转换为MEM_CAP枚举类型
        if (N <= m4K)        return m4K;
        else if (N <= m16K)  return m16K;
        else if (N <= m64K)  return m64K;
        else if (N <= m256K) return m256K;
        else if (N <= m1M)   return m1M;
        else if (N <= m4M)   return m4M;
        else if (N <= m8M)   return m8M;
        else                 return 0; 
    }();  //注意最后(),因为lambda生成的是重载了operator()的匿名类，是仿函数还未执行。

    if (index == 0) return nullptr;  // 处理无效输入

    unique_lock<mutex> lock(_mutex);    
    
    //2.如果该刻度的内存链表已经用完，额外申请内存
    if(_pool[index] == NULL){
        if(_mem_capacity + index/1024 >= MEM_LIMIT){
            cerr << "Already too much memory used." << endl;
            exit(1);
        }

        io_buf* new_buf = new io_buf(index);
        if(!new_buf){
            cerr << "New io_buf " << index << " create error."<< endl;
            exit(1);
        }

        _mem_capacity += index/1024;
        //不用手动解锁
        return new_buf;
    }

    //3.如果该刻度有内存，从pool中取一块内存返回
    io_buf* target = _pool[index];
    _pool[index] = target->next;

    target->next = nullptr;
    return target;
}

//将一个io_buf放回pool中
void buf_pool::revert(io_buf* buffer){
    //属于哪个内存链表
    int index = buffer->capacity;

    buffer->head = buffer->length = 0;

    unique_lock<mutex> lock(_mutex);

    //未找到这个key
    if(_pool.find(index) == _pool.end()){
        cerr << "No such type io_buf" << endl;
        return;
    }

    buffer->next = _pool[index];
    _pool[index] = buffer;
    buffer = nullptr;
}












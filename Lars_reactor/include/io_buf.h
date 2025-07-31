#pragma once

//链式内存单元
class io_buf{
public:
    //构造函数，创建一个size大小的buf
    io_buf(int size);
    //清空数据
    void clear();
    //处理长度len的数据，并向后移动head
    void pop(int len);
    //将已处理数据清空，未处理数据前移(回收已消费数据，这些数据叫空洞)
    void adjust();
    //将其他数据拷贝到自己中
    void copy(const io_buf *other);

public:
    //当前buf总容量
    int capacity;
    //当前buf有效长度
    int length;
    //当前有效数据头部索引
    int head;
    //当前buf内存首地址
    char* data;
    //链表形式管理io_buf
    io_buf *next;
};


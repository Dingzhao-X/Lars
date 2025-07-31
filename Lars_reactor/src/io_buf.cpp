#include "io_buf.h"
#include <cassert>
#include <string.h>
#include <stdio.h>

//构造函数，创建一个size大小的buf
//char[]()是c++风格，直接初始化。c则是char[]跟memset 0
io_buf::io_buf(int size): capacity(size), length(0), head(0), data(new char[size]()), next(nullptr){
    assert(data);
}
//清空数据
void io_buf::clear()
{
    length = head = 0;
}
//处理长度len的数据，并向后移动head
void io_buf::pop(int len)
{
    head += len;
    length -= len;
}
//回收已消费数据。未处理数据前移(已消费数据叫空洞)
void io_buf::adjust()
{
    if(head!=0 && length != 0){
        memmove(data, data+head, length);
    }
    head = 0;
}
//将其他数据拷贝到自己
void io_buf::copy(const io_buf *other)
{
    memcpy(data, other->data + other->head, other->length);
    head = 0;
    length = other->length;
}


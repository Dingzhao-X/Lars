#pragma once

//链接类型的抽象类。
//Tips: c++所有父类都可以有构造函数（区别Java），但抽象类只能在子类创建对象。
class net_connection{
public:
    net_connection() = default;

    //纯虚函数，子类必须重写，父类变为抽象类。
    virtual int conn_write2fd(const char* data, int msglen, int msgid) = 0;

    //虚析构函数，必须，防止基类析构没能析构子类
    virtual ~net_connection() = default;

    //供开发者传递一些自定义参数
    void* param;    
};

//创建、销毁链接要触发的Hook回调函数的函数类型。
//注意，必须是函数指针/function，函数类型不是对象
using conn_callback = void (*)(net_connection* conn, void* args);

# WebServer
基于 Reactor 模型的高并发 C++ 服务框架，核心网络层实现 10K+ 并发连接处理能力。

## 功能
* **利用分层架构实现模块化框架设计**：整合内存管理、双缓冲封装、线程池、配置文件读写功能；
* **基于事件驱动模型开发核心框架**：扩展epoll复用实现事件触发循环，集成TCP连接管理（服务器/客户端）、链接属性设置与生命周期Hook机制；
* **实现高效通信协议体系**：封装TCP/UDP双协议支持，完成消息路由分发与Protobuf数据传输协议集成；
* **构建异步任务处理系统**：组合消息队列与线程池技术实现异步消息任务机制与业务逻辑分发；
* **开发性能验证体系**：设计QPS压力测试模块，验证高并发场景下的连接管理能力；
* **实现扩展机制**：支持通过Hook注入自定义逻辑，提供链接属性动态配置能力。

## 框架结构

![Reactor框架结构](C:\Users\Dingzhao Xia\OneDrive\Desktop\Reactor框架结构.png)

## 环境要求
* Linux
* C++14

## 目录树
```
.
├── build							目标文件
│   ├── buf_pool.o
│   ├── config_file.o
│   ├── event_loop.o
│   ├── io_buf.o
│   ├── message.o
│   ├── reactor_buf.o
│   ├── tcp_client.o
│   ├── tcp_conn.o
│   ├── tcp_server.o
│   ├── thread_pool.o
│   ├── udp_client.o
│   └── udp_server.o
├── conf							配置参数
│   └── server.ini
├── include							头文件
│   ├── buf_pool.h
│   ├── config_file.h
│   ├── event_handler.h
│   ├── event_loop.h
│   ├── io_buf.h
│   ├── message.h
│   ├── msg_task.h
│   ├── net_connection.h
│   ├── reactor_buf.h
│   ├── reactor.h
│   ├── tcp_client.h
│   ├── tcp_conn.h
│   ├── tcp_server.h
│   ├── thread_pool.h
│   ├── thread_queue.hpp
│   ├── udp_client.h
│   └── udp_server.h
├── lib								静态库
│   └── libreactor.a
├── makefile
├── src								源文件
│   ├── buf_pool.cpp
│   ├── config_file.cpp
│   ├── event_loop.cpp
│   ├── io_buf.cpp
│   ├── message.cpp
│   ├── reactor_buf.cpp
│   ├── tcp_client.cpp
│   ├── tcp_conn.cpp
│   ├── tcp_server.cpp
│   ├── thread_pool.cpp
│   ├── udp_client.cpp
│   └── udp_server.cpp
└── test							启动及测试
    ├── makefile
    ├── qps_test								qps测试
    │   ├── build.sh
    │   ├── makefile
    │   ├── msg.pb.cc
    │   ├── msg.pb.h
    │   ├── msg.proto
    │   ├── qps_client_tcp
    │   ├── qps_client_tcp.cpp
    │   ├── qps_server_tcp
    │   └── qps_server_tcp.cpp
    ├── reactor_client_tcp
    ├── reactor_client_tcp.cpp
    ├── reactor_client_udp
    ├── reactor_client_udp.cpp
    ├── reactor_server_tcp
    ├── reactor_server_tcp.cpp
    ├── reactor_server_udp
    └── reactor_server_udp.cpp

```


## 项目启动
需要先配置好对应的数据库
```bash
//编译目标文件，并打包静态库
make 

//设置参数
cd conf/
vim server.ini

//启动tcp客户端
cd ../test/
make
./reactor_server_tcp

//启动tcp服务端
./reactor_client_tcp
```

## QPS测试
| 客户端线程数 | QPS     |
| ------------ | ------- |
| 1            | 0.81w/s |
| 2            | 1.96w/s |
| 10           | 4.12w/s |
| 100          | 4.23w/s |
| 500          | 3.65w/s |

* 测试环境: Ubuntu:22.04 ; g++ 11.4.0; 
* CPU: i5-13500H - 4核; 4G内存 

## TODO
* DNS Service 开发
* Reporter Service 开发
* 负载均衡代理


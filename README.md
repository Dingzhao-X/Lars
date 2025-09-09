# LARS负载均衡系统

## 项目简介

这是一个基于现代C++重新实现的LARS（Load balance And Remote service schedule System）负载均衡系统。在保持原有功能的基础上，采用了C++11/17的新特性，提升了代码的安全性和可维护性。

## 系统架构

![面试_lars](D:\UK Study\Career\面试用\面试_lars.png)

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   lars_dns      │    │  lars_lb_agent   │    │ lars_reporter   │
│  (DNS路由服务)   │◄───┤  (负载均衡代理)   ├───►│  (状态上报服务)  │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
    ┌──────────────────────────────────────────────────────────┐
    │                lars_reactor                              │
    │             (网络I/O框架)                                │
    └──────────────────────────────────────────────────────────┘
```

### 核心模块

- **lars_dns**: DNS路由服务，管理服务节点路由信息
- **lars_lb_agent**: 负载均衡代理，提供服务发现和负载均衡
- **lars_reporter**: 状态上报服务，收集服务调用统计
- **lars_reactor**: 网络IO框架，基于epoll的事件驱动框架

## 技术特性

### 1. 现代C++特性应用
```cpp
// 智能指针管理资源
std::unique_ptr<tcp_server> server;
std::shared_ptr<route_manager> route_mgr;

// STL容器和算法
std::unordered_map<uint64_t, host_set> route_map;
std::vector<std::shared_ptr<host_info>> hosts;

// RAII资源管理
std::lock_guard<std::mutex> lock(mutex);
```

### 2. 事件驱动架构
- 基于epoll的IO多路复用
- 线程池处理业务逻辑
- 异步事件通知机制

### 3. 负载均衡策略
- 轮询算法
- 基于权重的动态负载均衡
- 过载保护机制

## 构建运行

### 环境要求
- Linux (Ubuntu 18.04+)
- g++ 7.4.0+ (支持C++17)
- MySQL 5.7+
- protobuf 3.6.1+

### 编译安装
```bash
# 编译所有模块
cd Reactor_XDZ
make all

# 分模块编译
make dns        # 编译DNS服务
make agent      # 编译负载均衡代理
make reporter   # 编译状态上报服务
```

### 运行服务
```bash
# 1. 启动DNS服务
cd lars_dns/bin
./lars_dns

# 2. 启动Reporter服务  
cd lars_reporter/bin
./lars_reporter

# 3. 启动LoadBalance Agent
cd lars_lb_agent/bin
./lars_lb_agent
```

## 主要改进

### 1. 代码质量
- 使用智能指针管理内存
- STL容器替代原始数组
- 异常安全的RAII设计

### 2. 并发控制
- 基于RAII的锁管理
- 线程安全的事件处理
- 优化的线程池设计

### 3. 系统架构
- 模块化的设计
- 完整的构建系统
- 配置驱动的部署

## 性能特点

1. **内存管理**: 智能指针减少内存泄漏
2. **并发处理**: 事件驱动提升并发能力
3. **负载均衡**: 动态权重计算和调整
4. **可扩展性**: 模块化设计易于扩展

## 配置说明

各服务都有独立的配置文件：
```ini
[mysql]
db_host = 127.0.0.1
db_port = 3306
db_user = root
db_name = lars_dns

[reactor]
ip = 127.0.0.1
port = 7778
threadNum = 5
```

## 参考资料

- LARS开源项目
- 《深入理解计算机网络》
- 《C++并发编程实战》
- Reactor模式相关论文

## 项目规划

- [x] 网络框架重构
- [x] DNS服务实现
- [x] 负载均衡实现
- [x] 状态上报服务
- [ ] 完整的监控系统
- [ ] 容器化部署支持

## 贡献

欢迎提交Issue和Pull Request来帮助改进项目。

#pragma once

#include "../../lars_reactor/include/reactor.h"
#include "../../common/include/lars.pb.h"
#include "route_manager.h"
#include <memory>
#include <vector>

/*
 * Agent服务器主类 - 现代C++版本
 * 负责启动UDP服务器、DNS客户端、Reporter客户端
 */
class agent_server {
public:
    // 构造函数
    agent_server();
    
    // 析构函数
    ~agent_server();

    // 初始化Agent服务
    bool initialize();

    // 启动所有服务
    void start_services();

    // 获取路由管理器
    std::shared_ptr<route_manager> get_route_manager(int modid, int cmdid);

private:
    // 配置结构
    struct lb_config {
        int probe_num = 10;
        int init_succ_cnt = 180;
        int init_err_cnt = 5;
        float err_rate = 0.1f;
        float succ_rate = 0.95f;
        int contin_succ_limit = 10;
        int contin_err_limit = 10;
        int idle_timeout = 15;
        int overload_timeout = 15;
        int update_timeout = 15;
    } _lb_config;

    // 3个路由管理器，对应3个UDP服务器
    std::vector<std::shared_ptr<route_manager>> _route_managers;

    // 消息队列指针
    std::unique_ptr<thread_queue<lars::ReportStatusRequest>> _report_queue;
    std::unique_ptr<thread_queue<lars::GetRouteRequest>> _dns_queue;

    // 启动UDP服务器
    void start_udp_servers();
    
    // 启动Reporter客户端
    void start_report_client();
    
    // 启动DNS客户端
    void start_dns_client();

    // 加载配置
    void load_config();

    // 计算模块应该使用哪个路由管理器
    int get_manager_index(int modid, int cmdid) const {
        uint64_t mod_key = (static_cast<uint64_t>(modid) << 32) + cmdid;
        return mod_key % 3;
    }
};

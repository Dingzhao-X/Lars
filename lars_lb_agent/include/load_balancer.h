#pragma once

#include "host_info.h"
#include "../../common/include/lars.pb.h"
#include <vector>
#include <memory>
#include <random>
#include <mutex>
#include <chrono>

/*
 * 负载均衡器 - 现代C++版本
 * 负责管理一组后端服务器并实现负载均衡算法
 */
class load_balancer {
public:
    // 负载均衡器状态
    enum lb_status {
        LB_NEW = 0,        // 新建状态，需要拉取路由
        LB_PULLING = 1,    // 正在拉取路由
        LB_RUNNING = 2     // 正常运行
    };

    // 构造函数
    load_balancer(int modid, int cmdid);
    
    // 析构函数
    ~load_balancer();

    // 选择一个可用的主机
    int choose_host(lars::GetHostResponse& response);

    // 获取所有主机信息
    std::vector<std::shared_ptr<host_info>> get_all_hosts();

    // 更新主机列表（从DNS获取的新路由）
    void update_hosts(const lars::GetRouteResponse& route_response);

    // 上报主机调用结果
    void report_host_status(const lars::ReportRequest& report);

    // 检查是否为空
    bool empty() const;

    // 拉取路由信息
    void pull_route();

    // 获取模块标识
    int get_modid() const { return _modid; }
    int get_cmdid() const { return _cmdid; }

    // 公共状态变量 - 用mutex保护替代atomic
    lb_status status;
    std::chrono::steady_clock::time_point last_update_time;
    mutable std::mutex _status_mutex;  // 保护状态变量

private:
    // 模块标识
    int _modid;
    int _cmdid;

    // 主机列表 - 使用智能指针管理
    std::vector<std::shared_ptr<host_info>> _hosts;
    
    // 保护主机列表的互斥锁
    std::mutex _hosts_mutex;

    // 随机数生成器 - 用于负载均衡算法
    std::mt19937 _random_gen;
    std::uniform_int_distribution<size_t> _uniform_dist;

    // 当前选择的主机索引（轮询算法用）- 用mutex保护
    size_t _current_index;
    std::mutex _index_mutex;

    // 负载均衡算法实现
    std::shared_ptr<host_info> random_choice();
    std::shared_ptr<host_info> round_robin_choice(); 
    std::shared_ptr<host_info> weighted_choice();

    // 检查主机是否需要探测
    bool need_probe(std::shared_ptr<host_info> host) const;

    // 从主机列表中移除不可用主机
    void remove_unavailable_hosts();
};

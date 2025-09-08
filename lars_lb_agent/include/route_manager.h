#pragma once

#include "load_balancer.h"
#include "../../common/include/lars.pb.h"
#include <unordered_map>
#include <memory>
#include <mutex>

/*
 * 路由管理器 - 现代C++版本  
 * 管理多个modid/cmdid对应的负载均衡器
 * 为业务层提供统一的服务发现和负载均衡接口
 */
class route_manager {
public:
    // 构造函数
    route_manager(int manager_id);
    
    // 析构函数
    ~route_manager();

    // 获取一个可用主机
    int get_host(int modid, int cmdid, lars::GetHostResponse& response);

    // 获取主机列表
    int get_route(int modid, int cmdid, lars::GetRouteResponse& response);

    // 更新路由信息
    int update_route(int modid, int cmdid, const lars::GetRouteResponse& route_response);

    // 上报主机调用结果
    void report_host_result(const lars::ReportRequest& report);

    // 重置所有负载均衡器状态
    void reset_all_lb_status();

    // 获取管理器ID
    int get_id() const { return _manager_id; }

private:
    // 管理器ID（对应UDP服务器编号）
    int _manager_id;

    // 路由映射：modid/cmdid组合 -> 负载均衡器
    std::unordered_map<uint64_t, std::shared_ptr<load_balancer>> _route_map;
    
    // 保护路由映射的互斥锁
    std::mutex _route_mutex;

    // 将modid/cmdid组合为唯一键
    uint64_t make_route_key(int modid, int cmdid) const {
        return (static_cast<uint64_t>(modid) << 32) + cmdid;
    }

    // 创建新的负载均衡器
    std::shared_ptr<load_balancer> create_load_balancer(int modid, int cmdid);
};

#include "route_manager.h"
#include <iostream>

route_manager::route_manager(int manager_id) 
    : _manager_id(manager_id) {
    std::cout << "Created route manager with ID: " << manager_id << std::endl;
}

route_manager::~route_manager() {
    std::cout << "Destroyed route manager ID: " << _manager_id << std::endl;
}

int route_manager::get_host(int modid, int cmdid, lars::GetHostResponse& response) {
    uint64_t route_key = make_route_key(modid, cmdid);
    
    std::lock_guard<std::mutex> lock(_route_mutex);
    
    auto it = _route_map.find(route_key);
    if (it != _route_map.end()) {
        // 找到对应的负载均衡器
        auto lb = it->second;
        
        if (lb->empty()) {
            // 负载均衡器为空，可能正在拉取数据
            response.set_retcode(lars::RET_NOEXIST);
            return lars::RET_NOEXIST;
        }
        
        // 选择一个主机
        int ret = lb->choose_host(response);
        response.set_retcode(ret);
        
        // 检查是否需要触发路由拉取
        auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(lb->_status_mutex);
            if (lb->status == load_balancer::LB_NEW) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - lb->last_update_time).count();
                if (duration > 15) { // 15秒超时
                    lb->pull_route();
                }
            }
        }
        
        return ret;
    } else {
        // 没有找到对应的负载均衡器，需要创建
        auto lb = create_load_balancer(modid, cmdid);
        _route_map[route_key] = lb;
        
        // 立即拉取路由
        lb->pull_route();
        
        response.set_retcode(lars::RET_NOEXIST);
        return lars::RET_NOEXIST;
    }
}

int route_manager::get_route(int modid, int cmdid, lars::GetRouteResponse& response) {
    uint64_t route_key = make_route_key(modid, cmdid);
    
    std::lock_guard<std::mutex> lock(_route_mutex);
    
    auto it = _route_map.find(route_key);
    if (it != _route_map.end()) {
        auto lb = it->second;
        auto hosts = lb->get_all_hosts();
        
        response.set_modid(modid);
        response.set_cmdid(cmdid);
        
        // 添加所有主机信息到响应中
        for (const auto& host : hosts) {
            lars::HostInfo* host_info = response.add_host();
            host_info->set_ip(host->ip);
            host_info->set_port(host->port);
        }
        
        std::cout << "Returned " << hosts.size() << " hosts for modid=" 
                 << modid << ", cmdid=" << cmdid << std::endl;
        
        // 检查超时拉取
        auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(lb->_status_mutex);
            if (lb->status == load_balancer::LB_NEW) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - lb->last_update_time).count();
                if (duration > 15) {
                    lb->pull_route();
                }
            }
        }
        
        return lars::RET_SUCC;
    } else {
        // 创建新的负载均衡器
        auto lb = create_load_balancer(modid, cmdid);
        _route_map[route_key] = lb;
        lb->pull_route();
        
        return lars::RET_NOEXIST;
    }
}

int route_manager::update_route(int modid, int cmdid, const lars::GetRouteResponse& route_response) {
    uint64_t route_key = make_route_key(modid, cmdid);
    
    std::lock_guard<std::mutex> lock(_route_mutex);
    
    auto it = _route_map.find(route_key);
    if (it != _route_map.end()) {
        // 更新现有负载均衡器
        it->second->update_hosts(route_response);
        std::cout << "Updated route for modid=" << modid << ", cmdid=" << cmdid << std::endl;
    } else {
        // 创建新的负载均衡器并更新
        auto lb = create_load_balancer(modid, cmdid);
        lb->update_hosts(route_response);
        _route_map[route_key] = lb;
        std::cout << "Created and updated new route for modid=" << modid << ", cmdid=" << cmdid << std::endl;
    }
    
    return lars::RET_SUCC;
}

void route_manager::report_host_result(const lars::ReportRequest& report) {
    uint64_t route_key = make_route_key(report.modid(), report.cmdid());
    
    std::lock_guard<std::mutex> lock(_route_mutex);
    
    auto it = _route_map.find(route_key);
    if (it != _route_map.end()) {
        it->second->report_host_status(report);
    } else {
        std::cout << "No load balancer found for modid=" << report.modid() 
                 << ", cmdid=" << report.cmdid() << " when reporting host result" << std::endl;
    }
}

void route_manager::reset_all_lb_status() {
    std::lock_guard<std::mutex> lock(_route_mutex);
    
    for (auto& pair : _route_map) {
        std::lock_guard<std::mutex> status_lock(pair.second->_status_mutex);
        pair.second->status = load_balancer::LB_NEW;
    }
    
    std::cout << "Reset all load balancer status for manager " << _manager_id << std::endl;
}

std::shared_ptr<load_balancer> route_manager::create_load_balancer(int modid, int cmdid) {
    return std::make_shared<load_balancer>(modid, cmdid);
}

#include "load_balancer.h"
#include <iostream>
#include <algorithm>
#include <chrono>

load_balancer::load_balancer(int modid, int cmdid)
    : status(LB_NEW)
    , last_update_time(std::chrono::steady_clock::now())
    , _modid(modid)
    , _cmdid(cmdid)
    , _random_gen(std::chrono::steady_clock::now().time_since_epoch().count())
    , _current_index(0) {
    
    std::cout << "Created load balancer for modid=" << modid << ", cmdid=" << cmdid << std::endl;
}

load_balancer::~load_balancer() {
    std::cout << "Destroyed load balancer for modid=" << _modid << ", cmdid=" << _cmdid << std::endl;
}

int load_balancer::choose_host(lars::GetHostResponse& response) {
    std::lock_guard<std::mutex> lock(_hosts_mutex);
    
    if (_hosts.empty()) {
        std::cout << "No available hosts for modid=" << _modid << ", cmdid=" << _cmdid << std::endl;
        return lars::RET_NOEXIST;
    }

    // 移除不可用的主机
    remove_unavailable_hosts();
    
    if (_hosts.empty()) {
        std::cout << "All hosts unavailable for modid=" << _modid << ", cmdid=" << _cmdid << std::endl;
        return lars::RET_OVERLOAD;
    }

    // 选择负载均衡算法 - 这里使用轮询算法
    auto chosen_host = round_robin_choice();
    if (!chosen_host) {
        return lars::RET_SYSTEM_ERROR;
    }

    // 设置响应
    lars::HostInfo* host_info = response.mutable_host();
    host_info->set_ip(chosen_host->ip);
    host_info->set_port(chosen_host->port);
    
    std::cout << "Chose host " << chosen_host->ip << ":" << chosen_host->port 
             << " for modid=" << _modid << ", cmdid=" << _cmdid << std::endl;

    return lars::RET_SUCC;
}

std::vector<std::shared_ptr<host_info>> load_balancer::get_all_hosts() {
    std::lock_guard<std::mutex> lock(_hosts_mutex);
    return _hosts;  // 返回副本
}

void load_balancer::update_hosts(const lars::GetRouteResponse& route_response) {
    std::lock_guard<std::mutex> lock(_hosts_mutex);
    
    // 清空现有主机列表
    _hosts.clear();

    // 添加新的主机
    for (const auto& host : route_response.host()) {
        auto host_ptr = std::make_shared<host_info>(host.ip(), host.port());
        _hosts.push_back(host_ptr);
    }

    // 重新初始化随机分布
    if (!_hosts.empty()) {
        _uniform_dist = std::uniform_int_distribution<size_t>(0, _hosts.size() - 1);
    }

    {
        std::lock_guard<std::mutex> lock(_status_mutex);
        status = LB_RUNNING;
        last_update_time = std::chrono::steady_clock::now();
    }
    
    std::cout << "Updated " << _hosts.size() << " hosts for modid=" << _modid 
             << ", cmdid=" << _cmdid << std::endl;
}

void load_balancer::report_host_status(const lars::ReportRequest& report) {
    std::lock_guard<std::mutex> lock(_hosts_mutex);
    
    uint32_t target_ip = report.host().ip();
    uint32_t target_port = report.host().port();
    
    // 查找对应的主机
    auto it = std::find_if(_hosts.begin(), _hosts.end(),
        [target_ip, target_port](const std::shared_ptr<host_info>& host) {
            return host->ip == target_ip && host->port == target_port;
        });
    
    if (it != _hosts.end()) {
        bool success = (report.retcode() == lars::RET_SUCC);
        bool overload = (report.retcode() == lars::RET_OVERLOAD);
        
        (*it)->update_call_result(success, overload);
        
        std::cout << "Updated host " << target_ip << ":" << target_port 
                 << " status: success=" << success << ", overload=" << overload << std::endl;
    }
}

bool load_balancer::empty() const {
    std::lock_guard<std::mutex> lock(_hosts_mutex);
    return _hosts.empty();
}

void load_balancer::pull_route() {
    {
        std::lock_guard<std::mutex> lock(_status_mutex);
        status = LB_PULLING;
    }
    // TODO: 实现从DNS服务拉取路由的逻辑
    // 这里应该向DNS服务发送GetRouteRequest
    std::cout << "Pulling route for modid=" << _modid << ", cmdid=" << _cmdid << std::endl;
}

std::shared_ptr<host_info> load_balancer::random_choice() {
    if (_hosts.empty()) {
        return nullptr;
    }
    
    size_t index = _uniform_dist(_random_gen);
    return _hosts[index];
}

std::shared_ptr<host_info> load_balancer::round_robin_choice() {
    if (_hosts.empty()) {
        return nullptr;
    }
    
    size_t index;
    {
        std::lock_guard<std::mutex> lock(_index_mutex);
        index = _current_index % _hosts.size();
        _current_index++;
    }
    return _hosts[index];
}

std::shared_ptr<host_info> load_balancer::weighted_choice() {
    // 基于成功率的加权选择
    if (_hosts.empty()) {
        return nullptr;
    }
    
    // 计算权重总和
    double total_weight = 0.0;
    for (const auto& host : _hosts) {
        if (host->is_available()) {
            total_weight += host->get_success_rate();
        }
    }
    
    if (total_weight <= 0.0) {
        // 如果没有可用主机或权重为0，回退到轮询
        return round_robin_choice();
    }
    
    // 生成随机数
    std::uniform_real_distribution<double> weight_dist(0.0, total_weight);
    double random_weight = weight_dist(_random_gen);
    
    // 选择主机
    double current_weight = 0.0;
    for (const auto& host : _hosts) {
        if (host->is_available()) {
            current_weight += host->get_success_rate();
            if (current_weight >= random_weight) {
                return host;
            }
        }
    }
    
    // 应该不会到这里，但以防万一
    return _hosts.front();
}

bool load_balancer::need_probe(std::shared_ptr<host_info> host) const {
    // 如果主机过载时间超过一定阈值，需要探测
    if (host->status == host_info::HOST_OVERLOAD) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - host->last_update_time).count();
        return duration > 30; // 30秒后可以探测
    }
    return false;
}

void load_balancer::remove_unavailable_hosts() {
    // 移除不可用的主机（但保留用于探测的过载主机）
    _hosts.erase(
        std::remove_if(_hosts.begin(), _hosts.end(),
            [this](const std::shared_ptr<host_info>& host) {
                return !host->is_available() && !need_probe(host);
            }),
        _hosts.end());
}

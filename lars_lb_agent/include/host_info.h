#pragma once

#include <cstdint>
#include <chrono>

/*
 * 主机信息类 - 现代C++版本
 * 表示一个后端服务节点的状态信息
 */
class host_info {
public:
    // 主机状态枚举
    enum host_status {
        HOST_IDLE = 0,      // 空闲状态
        HOST_OVERLOAD = 1   // 过载状态  
    };

    // 构造函数
    host_info(uint32_t ip, uint32_t port);

    // 析构函数
    ~host_info() = default;

    // 获取主机标识（ip:port组合）
    uint64_t get_host_key() const;

    // 重置统计信息
    void reset_stats();

    // 更新调用结果
    void update_call_result(bool success, bool overload = false);

    // 检查主机是否可用
    bool is_available() const;

    // 获取当前成功率
    double get_success_rate() const;

    // 获取连续成功/失败次数
    uint32_t get_continuous_success() const { return _continuous_succ; }
    uint32_t get_continuous_error() const { return _continuous_err; }

    // 主机基本信息
    uint32_t ip;
    uint32_t port;
    
    // 统计信息
    uint32_t succ_count;        // 成功次数
    uint32_t err_count;         // 失败次数
    uint32_t overload_count;    // 过载次数
    
    // 状态信息
    host_status status;
    std::chrono::steady_clock::time_point last_update_time;  // 最后更新时间

private:
    uint32_t _continuous_succ;  // 连续成功次数
    uint32_t _continuous_err;   // 连续失败次数
    uint64_t _host_key;         // 主机标识缓存
};

#include "host_info.h"

host_info::host_info(uint32_t ip, uint32_t port) 
    : ip(ip)
    , port(port)  
    , succ_count(0)
    , err_count(0)
    , overload_count(0)
    , status(HOST_IDLE)
    , last_update_time(std::chrono::steady_clock::now())
    , _continuous_succ(0)
    , _continuous_err(0)
    , _host_key((static_cast<uint64_t>(ip) << 32) + port) {
}

uint64_t host_info::get_host_key() const {
    return _host_key;
}

void host_info::reset_stats() {
    succ_count = 0;
    err_count = 0;
    overload_count = 0;
    _continuous_succ = 0;
    _continuous_err = 0;
    status = HOST_IDLE;
    last_update_time = std::chrono::steady_clock::now();
}

void host_info::update_call_result(bool success, bool overload) {
    last_update_time = std::chrono::steady_clock::now();
    
    if (overload) {
        // 主机过载
        overload_count++;
        status = HOST_OVERLOAD;
        _continuous_err++;
        _continuous_succ = 0;
    } else if (success) {
        // 调用成功
        succ_count++;
        _continuous_succ++;
        _continuous_err = 0;
        
        // 如果之前是过载状态，可能需要恢复
        if (status == HOST_OVERLOAD) {
            // 这里可以加入恢复逻辑，比如连续成功N次后恢复正常
            if (_continuous_succ >= 5) {  // 连续成功5次恢复正常
                status = HOST_IDLE;
            }
        }
    } else {
        // 调用失败
        err_count++;
        _continuous_err++;
        _continuous_succ = 0;
        
        // 连续失败过多可能标记为过载
        if (_continuous_err >= 3) {  // 连续失败3次标记为过载
            status = HOST_OVERLOAD;
        }
    }
}

bool host_info::is_available() const {
    // 过载状态的主机不可用
    if (status == HOST_OVERLOAD) {
        return false;
    }
    
    // 可以根据成功率判断是否可用
    double success_rate = get_success_rate();
    return success_rate >= 0.5; // 成功率低于50%认为不可用
}

double host_info::get_success_rate() const {
    uint32_t total_calls = succ_count + err_count;
    if (total_calls == 0) {
        return 1.0; // 没有调用记录时认为是可用的
    }
    
    return static_cast<double>(succ_count) / total_calls;
}

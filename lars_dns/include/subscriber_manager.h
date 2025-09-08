#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <vector>

/*
 * 订阅管理器 - 现代C++版本
 * 管理客户端对模块的订阅关系
 * 当路由信息发生变化时，通知相关的订阅客户端
 */
class subscriber_manager {
public:
    // 获取单例实例
    static std::shared_ptr<subscriber_manager> instance();

    // 构造函数
    subscriber_manager();

    // 析构函数  
    ~subscriber_manager();

    // 订阅模块 - 客户端订阅某个modid/cmdid
    void subscribe(uint64_t mod, int fd);

    // 取消订阅
    void unsubscribe(uint64_t mod, int fd);

    // 获取某个模块的所有订阅者
    std::vector<int> get_subscribers(uint64_t mod);

    // 发布变更通知 - 通知所有订阅者模块信息已变更
    void publish_changes(const std::vector<uint64_t>& changed_mods);

private:
    // 单例模式相关
    static std::shared_ptr<subscriber_manager> _instance;
    static std::mutex _instance_mutex;

    // 订阅关系映射
    // mod(modid+cmdid) -> set<fd> (订阅该模块的客户端fd集合)
    std::unordered_map<uint64_t, std::unordered_set<int>> _subscribers;
    
    // 反向映射: fd -> set<mod> (该客户端订阅的模块集合)
    std::unordered_map<int, std::unordered_set<uint64_t>> _client_subs;

    // 保护订阅关系的互斥锁
    std::mutex _sub_mutex;
};

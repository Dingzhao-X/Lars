#include "subscriber_manager.h"
#include <iostream>
#include <algorithm>

// 单例实现
std::shared_ptr<subscriber_manager> subscriber_manager::_instance = nullptr;
std::mutex subscriber_manager::_instance_mutex;

std::shared_ptr<subscriber_manager> subscriber_manager::instance() {
    // 双重检查锁定模式 
    if (_instance == nullptr) {
        std::lock_guard<std::mutex> lock(_instance_mutex);
        if (_instance == nullptr) {
            _instance = std::make_shared<subscriber_manager>();
        }
    }
    return _instance;
}

subscriber_manager::subscriber_manager() {
    // 构造函数为空，成员变量已通过初始化列表初始化
}

subscriber_manager::~subscriber_manager() {
    // 使用智能容器，自动清理资源
}

void subscriber_manager::subscribe(uint64_t mod, int fd) {
    std::lock_guard<std::mutex> lock(_sub_mutex);
    
    // 添加到正向映射: mod -> set<fd>
    _subscribers[mod].insert(fd);
    
    // 添加到反向映射: fd -> set<mod>
    _client_subs[fd].insert(mod);
    
    std::cout << "Client fd=" << fd << " subscribed to mod=" << mod << std::endl;
}

void subscriber_manager::unsubscribe(uint64_t mod, int fd) {
    std::lock_guard<std::mutex> lock(_sub_mutex);
    
    // 从正向映射中移除
    auto sub_it = _subscribers.find(mod);
    if (sub_it != _subscribers.end()) {
        sub_it->second.erase(fd);
        
        // 如果该模块没有订阅者了，删除整个条目
        if (sub_it->second.empty()) {
            _subscribers.erase(sub_it);
        }
    }
    
    // 从反向映射中移除
    auto client_it = _client_subs.find(fd);
    if (client_it != _client_subs.end()) {
        client_it->second.erase(mod);
        
        // 如果该客户端没有订阅任何模块了，删除整个条目
        if (client_it->second.empty()) {
            _client_subs.erase(client_it);
        }
    }
    
    std::cout << "Client fd=" << fd << " unsubscribed from mod=" << mod << std::endl;
}

std::vector<int> subscriber_manager::get_subscribers(uint64_t mod) {
    std::lock_guard<std::mutex> lock(_sub_mutex);
    
    std::vector<int> result;
    
    auto it = _subscribers.find(mod);
    if (it != _subscribers.end()) {
        // 将set转换为vector
        result.reserve(it->second.size());
        for (int fd : it->second) {
            result.push_back(fd);
        }
    }
    
    return result;
}

void subscriber_manager::publish_changes(const std::vector<uint64_t>& changed_mods) {
    std::lock_guard<std::mutex> lock(_sub_mutex);
    
    // 收集所有需要通知的客户端 - 使用set去重
    std::unordered_set<int> clients_to_notify;
    
    for (uint64_t mod : changed_mods) {
        auto it = _subscribers.find(mod);
        if (it != _subscribers.end()) {
            // 将该模块的所有订阅者加入通知列表
            for (int fd : it->second) {
                clients_to_notify.insert(fd);
            }
        }
    }
    
    // 这里可以添加实际的通知逻辑
    // 比如向客户端发送路由更新消息
    if (!clients_to_notify.empty()) {
        std::cout << "Publishing route changes to " << clients_to_notify.size() 
                 << " clients for " << changed_mods.size() << " modules" << std::endl;
        
        // TODO: 实现具体的通知发送逻辑
        // 可以通过事件循环向这些fd发送更新消息
    }
}

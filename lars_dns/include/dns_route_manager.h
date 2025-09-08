#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <mysql.h>

// 定义主机信息类型 - 使用现代C++容器
using host_set = std::unordered_set<uint64_t>;
using route_map = std::unordered_map<uint64_t, host_set>;

/*
 * DNS路由管理器 - 现代C++版本
 * 负责管理 modid/cmdid 到 host:port 的映射关系
 * 使用智能指针和现代C++特性
 */
class dns_route_manager {
public:
    // 获取单例实例
    static std::shared_ptr<dns_route_manager> instance();

    // 构造函数
    dns_route_manager();
    
    // 析构函数
    ~dns_route_manager();

    // 连接数据库
    bool connect_database();

    // 构建路由映射 - 从数据库加载数据到内存
    void build_route_map();

    // 加载路由数据到临时映射
    void load_route_data();

    // 获取指定模块的主机集合
    host_set get_hosts(int modid, int cmdid);

    // 检查并加载版本信息
    // 返回值: 0-版本无变化, 1-版本有变化, -1-失败  
    int load_version();

    // 加载变更信息
    void load_changes(std::vector<uint64_t>& change_list);

    // 交换数据指针 - 双缓冲机制
    void swap_data();

private:
    // 单例模式相关
    static std::shared_ptr<dns_route_manager> _instance;
    static std::mutex _instance_mutex;

    // MySQL数据库连接
    MYSQL _db_connection;
    std::string _sql_buffer;  // 使用string替代固定大小数组

    // 双缓冲机制 - 使用智能指针管理内存
    std::unique_ptr<route_map> _current_data;
    std::unique_ptr<route_map> _temp_data;
    
    // 读写锁保护数据访问
    std::shared_mutex _data_mutex;

    // 版本信息
    uint64_t _current_version;

    // 数据库配置信息
    struct db_config {
        std::string host = "127.0.0.1";
        int port = 3306;
        std::string user = "root"; 
        std::string password = "aceld";
        std::string database = "lars_dns";
    } _db_cfg;
};

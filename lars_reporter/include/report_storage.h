#pragma once

#include "../../common/include/lars.pb.h"
#include "../../lars_reactor/include/thread_queue.hpp"
#include "../../lars_reactor/include/event_loop.h"
#include <mysql.h>
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <thread>

/*
 * 报告存储管理器 - 现代C++版本
 * 负责将上报的统计数据存储到MySQL数据库
 */
class report_storage {
public:
    // 构造函数
    report_storage();
    
    // 析构函数
    ~report_storage();

    // 连接数据库
    bool connect_database();

    // 存储单个报告
    bool store_report(const lars::ReportStatusRequest& report);

    // 批量存储报告（提高性能）
    bool store_batch_reports(const std::vector<lars::ReportStatusRequest>& reports);

    // 检查数据库连接状态
    bool is_connected() const { return _is_connected; }

private:
    // MySQL连接
    MYSQL _db_connection;
    bool _is_connected;

    // 数据库配置
    struct db_config {
        std::string host = "127.0.0.1";
        int port = 3306;
        std::string user = "root";
        std::string password = "aceld";
        std::string database = "lars_dns";
    } _db_cfg;

    // SQL语句缓冲区
    std::string _sql_buffer;

    // 保护数据库操作的互斥锁
    std::mutex _db_mutex;

    // 加载数据库配置
    void load_db_config();

    // 构建插入SQL语句
    std::string build_insert_sql(const lars::ReportStatusRequest& report);

    // 执行SQL语句
    bool execute_sql(const std::string& sql);
};

/*
 * 报告队列处理器 - 事件驱动版本
 * 基于lars_reactor的thread_queue，符合事件驱动架构
 */
class report_queue_processor {
    // 声明友元函数用于事件回调
    friend void process_report_task(event_loop* loop, int fd, void* args);
    
public:
    // 构造函数
    report_queue_processor();
    
    // 析构函数
    ~report_queue_processor();

    // 启动处理线程
    void start();

    // 停止处理线程
    void stop();

    // 添加报告到队列
    void enqueue_report(const lars::ReportStatusRequest& report);

    // 获取队列大小
    size_t queue_size() const;

private:
    // 使用lars_reactor的线程队列，符合事件驱动模式
    std::unique_ptr<thread_queue<lars::ReportStatusRequest>> _thread_queue;
    
    // 工作线程的事件循环
    std::unique_ptr<event_loop> _worker_loop;
    
    // 线程控制 - 用简单的bool + mutex替代atomic
    bool _running;
    std::mutex _running_mutex;

    // 工作线程
    std::unique_ptr<std::thread> _worker_thread;

    // 存储管理器
    std::unique_ptr<report_storage> _storage;

    // 工作线程函数 - 基于事件循环
    void worker_thread_func();

    // 检查运行状态
    bool is_running();
    void set_running(bool running);
};

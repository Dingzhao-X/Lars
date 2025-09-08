#include "report_storage.h"
#include "../../lars_reactor/include/config_file.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

report_storage::report_storage() 
    : _is_connected(false) {
    
    // 初始化MySQL连接
    mysql_init(&_db_connection);
    
    // 预分配SQL缓冲区
    _sql_buffer.reserve(2048);
    
    // 加载数据库配置
    load_db_config();
}

report_storage::~report_storage() {
    if (_is_connected) {
        mysql_close(&_db_connection);
    }
}

bool report_storage::connect_database() {
    std::lock_guard<std::mutex> lock(_db_mutex);
    
    // 设置MySQL连接选项
    mysql_options(&_db_connection, MYSQL_OPT_CONNECT_TIMEOUT, "30");
    
    // 开启自动重连
    my_bool reconnect = 1;
    mysql_options(&_db_connection, MYSQL_OPT_RECONNECT, &reconnect);

    // 连接数据库
    if (!mysql_real_connect(&_db_connection,
                           _db_cfg.host.c_str(),
                           _db_cfg.user.c_str(),
                           _db_cfg.password.c_str(),
                           _db_cfg.database.c_str(),
                           _db_cfg.port, nullptr, 0)) {
        std::cerr << "Failed to connect to MySQL: " << mysql_error(&_db_connection) << std::endl;
        _is_connected = false;
        return false;
    }

    _is_connected = true;
    std::cout << "Successfully connected to database for reporting!" << std::endl;
    return true;
}

bool report_storage::store_report(const lars::ReportStatusRequest& report) {
    if (!_is_connected) {
        std::cerr << "Database not connected" << std::endl;
        return false;
    }

    std::string sql = build_insert_sql(report);
    return execute_sql(sql);
}

bool report_storage::store_batch_reports(const std::vector<lars::ReportStatusRequest>& reports) {
    if (!_is_connected || reports.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_db_mutex);

    // 开始事务
    if (mysql_query(&_db_connection, "START TRANSACTION")) {
        std::cerr << "Failed to start transaction: " << mysql_error(&_db_connection) << std::endl;
        return false;
    }

    bool success = true;
    
    // 批量插入
    for (const auto& report : reports) {
        std::string sql = build_insert_sql(report);
        if (mysql_query(&_db_connection, sql.c_str())) {
            std::cerr << "Failed to insert report: " << mysql_error(&_db_connection) << std::endl;
            success = false;
            break;
        }
    }

    // 提交或回滚事务
    if (success) {
        if (mysql_query(&_db_connection, "COMMIT")) {
            std::cerr << "Failed to commit transaction: " << mysql_error(&_db_connection) << std::endl;
            success = false;
        } else {
            std::cout << "Successfully stored " << reports.size() << " reports" << std::endl;
        }
    } else {
        mysql_query(&_db_connection, "ROLLBACK");
    }

    return success;
}

void report_storage::load_db_config() {
    auto config = config_file::instance();
    
    _db_cfg.host = config->GetString("mysql", "db_host", "127.0.0.1");
    _db_cfg.port = config->GetNumber("mysql", "db_port", 3306);
    _db_cfg.user = config->GetString("mysql", "db_user", "root");
    _db_cfg.password = config->GetString("mysql", "db_passwd", "aceld");
    _db_cfg.database = config->GetString("mysql", "db_name", "lars_dns");
}

std::string report_storage::build_insert_sql(const lars::ReportStatusRequest& report) {
    std::ostringstream sql;
    
    // 构建INSERT SQL语句
    sql << "INSERT INTO ServerCallStatus (modid, cmdid, ip, port, caller, succ_cnt, err_cnt, ts, overload) VALUES ";
    
    bool first = true;
    for (const auto& result : report.results()) {
        if (!first) {
            sql << ", ";
        }
        first = false;
        
        sql << "(" 
            << report.modid() << ", "
            << report.cmdid() << ", "
            << result.ip() << ", "
            << result.port() << ", "
            << report.caller() << ", "
            << result.succ() << ", "
            << result.err() << ", "
            << report.ts() << ", "
            << "'" << (result.overload() ? "Y" : "N") << "'"
            << ")";
    }
    
    // 使用ON DUPLICATE KEY UPDATE来处理重复键
    sql << " ON DUPLICATE KEY UPDATE "
        << "succ_cnt = succ_cnt + VALUES(succ_cnt), "
        << "err_cnt = err_cnt + VALUES(err_cnt), "
        << "ts = VALUES(ts), "
        << "overload = VALUES(overload)";
    
    return sql.str();
}

bool report_storage::execute_sql(const std::string& sql) {
    std::lock_guard<std::mutex> lock(_db_mutex);
    
    if (mysql_query(&_db_connection, sql.c_str())) {
        std::cerr << "SQL execution error: " << mysql_error(&_db_connection) << std::endl;
        std::cerr << "SQL: " << sql << std::endl;
        return false;
    }
    
    return true;
}

// ==================== report_queue_processor 实现 ====================

// 前置声明友元函数
void process_report_task(event_loop* loop, int fd, void* args);

// 事件驱动的报告处理回调函数
void process_report_task(event_loop* loop, int fd, void* args) {
    report_queue_processor* processor = static_cast<report_queue_processor*>(args);
    
    // 从thread_queue中取出报告并处理  
    std::queue<lars::ReportStatusRequest> tmp_queue;
    processor->_thread_queue->recv(tmp_queue);
    
    const size_t batch_size = 100;
    std::vector<lars::ReportStatusRequest> batch;
    batch.reserve(batch_size);
    
    // 批量处理报告
    while (!tmp_queue.empty()) {
        batch.push_back(tmp_queue.front());
        tmp_queue.pop();
        
        // 达到批处理大小时处理一次
        if (batch.size() >= batch_size) {
            processor->_storage->store_batch_reports(batch);
            batch.clear();
        }
    }
    
    // 处理剩余的报告
    if (!batch.empty()) {
        processor->_storage->store_batch_reports(batch);
    }
}

report_queue_processor::report_queue_processor()
    : _running(false) {
    
    _storage = std::make_unique<report_storage>();
    _thread_queue = std::make_unique<thread_queue<lars::ReportStatusRequest>>();
    _worker_loop = std::make_unique<event_loop>();
}

report_queue_processor::~report_queue_processor() {
    stop();
}

void report_queue_processor::start() {
    if (is_running()) {
        return; // 已经启动
    }

    // 连接数据库
    if (!_storage->connect_database()) {
        std::cerr << "Failed to connect database, cannot start queue processor" << std::endl;
        return;
    }

    set_running(true);
    
    // 设置事件驱动的消息处理
    _thread_queue->set_loop(_worker_loop.get());
    _thread_queue->set_callback(&process_report_task, this);
    
    // 启动工作线程
    _worker_thread = std::make_unique<std::thread>(&report_queue_processor::worker_thread_func, this);
    
    std::cout << "Report queue processor started (event-driven)" << std::endl;
}

void report_queue_processor::stop() {
    set_running(false);
    
    // 等待工作线程结束
    if (_worker_thread && _worker_thread->joinable()) {
        _worker_thread->join();
    }
    
    std::cout << "Report queue processor stopped" << std::endl;
}

void report_queue_processor::enqueue_report(const lars::ReportStatusRequest& report) {
    if (_thread_queue) {
        _thread_queue->send(report);
    }
}

size_t report_queue_processor::queue_size() const {
    // thread_queue内部有自己的大小管理，这里返回一个估计值
    return 0; // 或者可以在thread_queue中添加size()方法
}

void report_queue_processor::worker_thread_func() {
    std::cout << "Report worker thread started (event-driven)" << std::endl;
    
    // 事件驱动的循环，不再使用condition_variable
    while (is_running()) {
        _worker_loop->event_process(); // 这会处理thread_queue的事件
    }
    
    std::cout << "Report worker thread stopped" << std::endl;
}

bool report_queue_processor::is_running() {
    std::lock_guard<std::mutex> lock(_running_mutex);
    return _running;
}

void report_queue_processor::set_running(bool running) {
    std::lock_guard<std::mutex> lock(_running_mutex);
    _running = running;
}

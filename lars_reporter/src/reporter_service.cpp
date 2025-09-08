#include "../../lars_reactor/include/reactor.h"
#include "../../common/include/lars.pb.h"
#include "report_storage.h"
#include <iostream>
#include <memory>
#include <vector>

// 全局的TCP服务器指针
std::unique_ptr<tcp_server> g_reporter_server;

// 报告队列处理器集合
std::vector<std::unique_ptr<report_queue_processor>> g_queue_processors;
int g_processor_index = 0;
std::mutex g_processor_index_mutex;

/*
 * 处理客户端上报状态的请求
 * 这是Reporter服务的核心业务处理函数
 */
void handle_report_status_request(const char* data, uint32_t len, int msgid,
                                net_connection* conn, void* user_data) {
    
    // 1. 解析客户端请求
    lars::ReportStatusRequest request;
    if (!request.ParseFromArray(data, len)) {
        std::cerr << "Failed to parse ReportStatusRequest" << std::endl;
        return;
    }

    std::cout << "Received report status request: modid=" << request.modid() 
             << ", cmdid=" << request.cmdid() 
             << ", caller=" << request.caller()
             << ", results_count=" << request.results_size() << std::endl;

    // 2. 将请求分发给其中一个队列处理器（负载均衡）
    int processor_idx;
    {
        std::lock_guard<std::mutex> lock(g_processor_index_mutex);
        processor_idx = g_processor_index % g_queue_processors.size();
        g_processor_index++;
    }
    g_queue_processors[processor_idx]->enqueue_report(request);

    std::cout << "Report enqueued to processor " << processor_idx << std::endl;
}

/*
 * 客户端连接创建时的回调函数
 */
void on_client_connect(net_connection* conn, void* args) {
    std::cout << "Reporter client connected: fd=" << conn->get_fd() << std::endl;
}

/*
 * 客户端连接关闭时的回调函数
 */
void on_client_disconnect(net_connection* conn, void* args) {
    std::cout << "Reporter client disconnected: fd=" << conn->get_fd() << std::endl;
}

/*
 * 创建数据库存储线程池
 */
void create_storage_thread_pool() {
    auto config = config_file::instance();
    int thread_count = config->GetNumber("reporter", "db_thread_cnt", 3);
    
    std::cout << "Creating " << thread_count << " database storage threads" << std::endl;
    
    // 创建指定数量的队列处理器
    for (int i = 0; i < thread_count; ++i) {
        auto processor = std::make_unique<report_queue_processor>();
        processor->start();
        g_queue_processors.push_back(std::move(processor));
        
        std::cout << "Database storage thread " << (i + 1) << " created" << std::endl;
    }
}

/*
 * 停止所有存储线程
 */
void stop_storage_threads() {
    std::cout << "Stopping all storage threads..." << std::endl;
    
    for (auto& processor : g_queue_processors) {
        processor->stop();
    }
    
    g_queue_processors.clear();
    std::cout << "All storage threads stopped" << std::endl;
}

/*
 * 信号处理函数
 */
void signal_handler(int sig) {
    std::cout << "Received signal " << sig << ", shutting down..." << std::endl;
    stop_storage_threads();
    exit(0);
}

/*
 * Reporter服务主函数
 */
int main() {
    try {
        // 1. 设置信号处理
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // 2. 创建事件循环
        event_loop main_loop;
        
        // 3. 加载配置文件
        config_file::setPath("./conf/lars_reporter.ini");
        auto config = config_file::instance();
        
        std::string server_ip = config->GetString("reactor", "ip", "127.0.0.1");
        uint16_t server_port = config->GetNumber("reactor", "port", 7779);
        
        std::cout << "Starting Reporter service on " << server_ip << ":" << server_port << std::endl;
        
        // 4. 创建TCP服务器
        g_reporter_server = std::make_unique<tcp_server>(&main_loop, server_ip.c_str(), server_port);
        
        // 5. 注册消息处理器
        g_reporter_server->add_msg_router(lars::ID_ReportStatusRequest, handle_report_status_request);
        
        // 6. 注册连接回调
        g_reporter_server->set_conn_start(on_client_connect);
        g_reporter_server->set_conn_close(on_client_disconnect);
        
        // 7. 创建数据库存储线程池
        create_storage_thread_pool();
        
        std::cout << "Reporter service started successfully!" << std::endl;
        
        // 8. 启动事件循环
        main_loop.event_process();
        
    } catch (const std::exception& e) {
        std::cerr << "Reporter service error: " << e.what() << std::endl;
        stop_storage_threads();
        return -1;
    }
    
    return 0;
}

/*
 * 监控和统计功能（可选）
 */
void print_statistics() {
    static auto last_print = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count();
    
    if (duration >= 60) { // 每分钟打印一次统计信息
        std::cout << "=== Reporter Statistics ===" << std::endl;
        
        size_t total_queue_size = 0;
        for (size_t i = 0; i < g_queue_processors.size(); ++i) {
            size_t queue_size = g_queue_processors[i]->queue_size();
            total_queue_size += queue_size;
            std::cout << "Processor " << i << " queue size: " << queue_size << std::endl;
        }
        
        std::cout << "Total pending reports: " << total_queue_size << std::endl;
        std::cout << "===========================" << std::endl;
        
        last_print = now;
    }
}

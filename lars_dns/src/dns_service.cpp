#include "../../lars_reactor/include/reactor.h"
#include "../../common/include/lars.pb.h"
#include "dns_route_manager.h"
#include "subscriber_manager.h"
#include <iostream>
#include <memory>
#include <unordered_set>
#include <thread>
#include <chrono>

// 全局的TCP服务器指针
std::unique_ptr<tcp_server> g_dns_server;

// 客户端订阅的模块集合类型
using client_sub_set = std::unordered_set<uint64_t>;

/*
 * 处理客户端获取路由信息的请求
 * 这是DNS服务的核心业务处理函数
 */
void handle_get_route_request(const char* data, uint32_t len, int msgid, 
                             net_connection* conn, void* user_data) {
    
    // 1. 解析客户端请求
    lars::GetRouteRequest request;
    if (!request.ParseFromArray(data, len)) {
        std::cerr << "Failed to parse GetRouteRequest" << std::endl;
        return;
    }
    
    int modid = request.modid();
    int cmdid = request.cmdid(); 
    
    std::cout << "Received route request: modid=" << modid << ", cmdid=" << cmdid << std::endl;

    // 2. 检查客户端订阅状态并处理订阅
    uint64_t mod_key = (static_cast<uint64_t>(modid) << 32) + cmdid;
    
    client_sub_set* client_subs = static_cast<client_sub_set*>(conn->param);
    if (client_subs == nullptr) {
        // 第一次请求，为客户端创建订阅集合
        client_subs = new client_sub_set();
        conn->param = client_subs;
    }
    
    // 如果客户端还没订阅这个模块，进行订阅
    if (client_subs->find(mod_key) == client_subs->end()) {
        client_subs->insert(mod_key);
        subscriber_manager::instance()->subscribe(mod_key, conn->get_fd());
        std::cout << "Client fd=" << conn->get_fd() << " subscribed to modid=" 
                 << modid << ", cmdid=" << cmdid << std::endl;
    }

    // 3. 从路由管理器获取主机信息
    auto route_mgr = dns_route_manager::instance();
    host_set hosts = route_mgr->get_hosts(modid, cmdid);
    
    // 4. 构建响应消息
    lars::GetRouteResponse response;
    response.set_modid(modid);
    response.set_cmdid(cmdid);
    
    // 将主机信息添加到响应中
    for (uint64_t host_key : hosts) {
        uint32_t ip = static_cast<uint32_t>(host_key >> 32);
        uint32_t port = static_cast<uint32_t>(host_key);
        
        lars::HostInfo* host_info = response.add_host();
        host_info->set_ip(ip);
        host_info->set_port(port);
    }
    
    std::cout << "Returning " << hosts.size() << " hosts for modid=" 
             << modid << ", cmdid=" << cmdid << std::endl;

    // 5. 发送响应给客户端
    std::string response_data;
    response.SerializeToString(&response_data);
    conn->send_message(response_data.c_str(), response_data.size(), 
                      lars::ID_GetRouteResponse);
}

/*
 * 客户端连接创建时的回调函数
 */
void on_client_connect(net_connection* conn, void* args) {
    std::cout << "New client connected: fd=" << conn->get_fd() << std::endl;
    
    // 为客户端创建订阅集合
    conn->param = new client_sub_set();
}

/*
 * 客户端连接关闭时的回调函数
 * 清理客户端的订阅信息
 */
void on_client_disconnect(net_connection* conn, void* args) {
    std::cout << "Client disconnected: fd=" << conn->get_fd() << std::endl;
    
    client_sub_set* client_subs = static_cast<client_sub_set*>(conn->param);
    if (client_subs != nullptr) {
        // 取消客户端的所有订阅
        auto sub_mgr = subscriber_manager::instance();
        for (uint64_t mod_key : *client_subs) {
            sub_mgr->unsubscribe(mod_key, conn->get_fd());
        }
        
        // 清理内存
        delete client_subs;
        conn->param = nullptr;
    }
}

/*
 * 后台线程：定期检查数据库变更
 * 使用现代C++的线程和智能指针
 */
void route_change_monitor_thread() {
    auto route_mgr = dns_route_manager::instance();
    auto sub_mgr = subscriber_manager::instance();
    
    const int check_interval = 10; // 10秒检查一次
    
    while (true) {
        try {
            // 检查版本变更
            int version_status = route_mgr->load_version();
            
            if (version_status == 1) {
                std::cout << "Route version changed, reloading data..." << std::endl;
                
                // 重新加载路由数据
                route_mgr->load_route_data();
                route_mgr->swap_data();
                
                // 获取变更的模块列表
                std::vector<uint64_t> changed_mods;
                route_mgr->load_changes(changed_mods);
                
                // 通知订阅者
                if (!changed_mods.empty()) {
                    sub_mgr->publish_changes(changed_mods);
                }
                
                std::cout << "Route data reloaded, " << changed_mods.size() 
                         << " modules changed" << std::endl;
            }
            else if (version_status == -1) {
                std::cerr << "Failed to check route version" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error in route monitor thread: " << e.what() << std::endl;
        }
        
        // 等待下次检查
        std::this_thread::sleep_for(std::chrono::seconds(check_interval));
    }
}

/*
 * DNS服务主函数
 */
int main() {
    try {
        // 1. 创建事件循环
        event_loop main_loop;
        
        // 2. 加载配置文件
        config_file::setPath("./conf/lars_dns.ini");
        auto config = config_file::instance();
        
        std::string server_ip = config->GetString("reactor", "ip", "127.0.0.1");
        uint16_t server_port = config->GetNumber("reactor", "port", 7778);
        
        std::cout << "Starting DNS service on " << server_ip << ":" << server_port << std::endl;
        
        // 3. 初始化路由管理器
        auto route_mgr = dns_route_manager::instance();
        if (!route_mgr->connect_database()) {
            std::cerr << "Failed to connect to database" << std::endl;
            return -1;
        }
        
        // 加载初始路由数据
        route_mgr->build_route_map();
        
        // 4. 创建TCP服务器
        g_dns_server = std::make_unique<tcp_server>(&main_loop, server_ip.c_str(), server_port);
        
        // 5. 注册消息处理器
        g_dns_server->add_msg_router(lars::ID_GetRouteRequest, handle_get_route_request);
        
        // 6. 注册连接回调
        g_dns_server->set_conn_start(on_client_connect);
        g_dns_server->set_conn_close(on_client_disconnect);
        
        // 7. 启动后台监控线程
        std::thread monitor_thread(route_change_monitor_thread);
        monitor_thread.detach();
        
        std::cout << "DNS service started successfully!" << std::endl;
        
        // 8. 启动事件循环
        main_loop.event_process();
        
    } catch (const std::exception& e) {
        std::cerr << "DNS service error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

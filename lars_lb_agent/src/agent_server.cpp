#include "agent_server.h" 
#include "../../lars_reactor/include/config_file.h"
#include <iostream>
#include <thread>

// 全局变量
std::unique_ptr<agent_server> g_agent_server;

agent_server::agent_server() {
    // 创建3个路由管理器
    for (int i = 0; i < 3; ++i) {
        _route_managers.push_back(std::make_shared<route_manager>(i + 1));
    }
}

agent_server::~agent_server() {
    std::cout << "Agent server destroyed" << std::endl;
}

bool agent_server::initialize() {
    // 加载配置文件
    config_file::setPath("./conf/lb_agent.ini");
    load_config();

    // 创建消息队列
    _report_queue = std::make_unique<thread_queue<lars::ReportStatusRequest>>();
    _dns_queue = std::make_unique<thread_queue<lars::GetRouteRequest>>();

    std::cout << "Agent server initialized successfully" << std::endl;
    return true;
}

void agent_server::start_services() {
    std::cout << "Starting agent services..." << std::endl;
    
    // 启动UDP服务器
    start_udp_servers();
    
    // 启动客户端线程
    start_report_client(); 
    start_dns_client();
    
    std::cout << "All agent services started" << std::endl;
}

std::shared_ptr<route_manager> agent_server::get_route_manager(int modid, int cmdid) {
    int index = get_manager_index(modid, cmdid);
    return _route_managers[index];
}

void agent_server::load_config() {
    auto config = config_file::instance();
    
    _lb_config.probe_num = config->GetNumber("loadbalance", "probe_num", 10);
    _lb_config.init_succ_cnt = config->GetNumber("loadbalance", "init_succ_cnt", 180);
    _lb_config.init_err_cnt = config->GetNumber("loadbalance", "init_err_cnt", 5);
    _lb_config.err_rate = config->GetFloat("loadbalance", "err_rate", 0.1f);
    _lb_config.succ_rate = config->GetFloat("loadbalance", "succ_rate", 0.95f);
    _lb_config.contin_succ_limit = config->GetNumber("loadbalance", "contin_succ_limit", 10);
    _lb_config.contin_err_limit = config->GetNumber("loadbalance", "contin_err_limit", 10);
    _lb_config.idle_timeout = config->GetNumber("loadbalance", "idle_timeout", 15);
    _lb_config.overload_timeout = config->GetNumber("loadbalance", "overload_timeout", 15);
    _lb_config.update_timeout = config->GetNumber("loadbalance", "update_timeout", 15);
    
    std::cout << "Load balance config loaded" << std::endl;
}

void agent_server::start_udp_servers() {
    std::cout << "Starting 3 UDP servers..." << std::endl;
    
    // 为每个UDP服务器创建独立的事件循环
    for (int i = 0; i < 3; ++i) {
        std::thread udp_thread([this, i]() {
            // 创建独立的事件循环
            event_loop loop;
            
            // 获取端口配置
            auto config = config_file::instance();
            std::string port_key = "server" + std::to_string(i + 1) + "_port";
            uint16_t port = config->GetNumber("udp_servers", port_key.c_str(), 7775 + i);
            
            // TODO: 创建UDP服务器
            // udp_server server(&loop, "127.0.0.1", port);
            // server.add_msg_router(lars::ID_GetHostRequest, handle_get_host_request);
            // server.add_msg_router(lars::ID_ReportRequest, handle_report_request);
            
            std::cout << "UDP server " << (i + 1) << " started on port " << port << std::endl;
            
            // 启动事件循环
            // loop.event_process();
        });
        
        udp_thread.detach();
    }
}

void agent_server::start_report_client() {
    std::cout << "Starting report client..." << std::endl;
    
    std::thread report_thread([this]() {
        // TODO: 实现Reporter客户端逻辑
        // 从_report_queue中取出报告请求，发送给Reporter服务
        
        std::cout << "Report client thread started" << std::endl;
        
        // 这里应该：
        // 1. 连接到Reporter服务器
        // 2. 循环处理_report_queue中的消息
        // 3. 将消息发送给Reporter服务器
        
        while (true) {
            // 处理报告队列
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    
    report_thread.detach();
}

void agent_server::start_dns_client() {
    std::cout << "Starting DNS client..." << std::endl;
    
    std::thread dns_thread([this]() {
        // TODO: 实现DNS客户端逻辑  
        // 从_dns_queue中取出路由请求，发送给DNS服务
        
        std::cout << "DNS client thread started" << std::endl;
        
        // 这里应该：
        // 1. 连接到DNS服务器
        // 2. 循环处理_dns_queue中的消息
        // 3. 将消息发送给DNS服务器
        // 4. 接收响应并更新对应的route_manager
        
        while (true) {
            // 处理DNS队列
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    
    dns_thread.detach();
}

// UDP服务器业务处理函数

/*
 * 处理获取主机请求
 */
void handle_get_host_request(const char* data, uint32_t len, int msgid,
                           net_connection* conn, void* user_data) {
    lars::GetHostRequest request;
    if (!request.ParseFromArray(data, len)) {
        std::cerr << "Failed to parse GetHostRequest" << std::endl;
        return;
    }
    
    int modid = request.modid();
    int cmdid = request.cmdid();
    uint32_t seq = request.seq();
    
    // 获取对应的路由管理器
    auto route_mgr = g_agent_server->get_route_manager(modid, cmdid);
    
    // 获取主机
    lars::GetHostResponse response;
    response.set_seq(seq);
    response.set_modid(modid); 
    response.set_cmdid(cmdid);
    
    int ret = route_mgr->get_host(modid, cmdid, response);
    response.set_retcode(ret);
    
    // 发送响应
    std::string response_data;
    response.SerializeToString(&response_data);
    conn->send_message(response_data.c_str(), response_data.size(), 
                      lars::ID_GetHostResponse);
}

/*
 * 处理上报请求
 */
void handle_report_request(const char* data, uint32_t len, int msgid,
                         net_connection* conn, void* user_data) {
    lars::ReportRequest request;
    if (!request.ParseFromArray(data, len)) {
        std::cerr << "Failed to parse ReportRequest" << std::endl;
        return;
    }
    
    int modid = request.modid();
    int cmdid = request.cmdid();
    
    // 获取对应的路由管理器并上报结果
    auto route_mgr = g_agent_server->get_route_manager(modid, cmdid);
    route_mgr->report_host_result(request);
}

/*
 * 主函数
 */
int main() {
    try {
        // 创建Agent服务器
        g_agent_server = std::make_unique<agent_server>();
        
        // 初始化
        if (!g_agent_server->initialize()) {
            std::cerr << "Failed to initialize agent server" << std::endl;
            return -1;
        }
        
        // 启动服务
        g_agent_server->start_services();
        
        std::cout << "Load Balance Agent is running..." << std::endl;
        
        // 主线程保持运行
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Agent server error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

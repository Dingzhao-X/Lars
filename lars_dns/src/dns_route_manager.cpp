#include "dns_route_manager.h"
#include "../../lars_reactor/include/config_file.h"
#include <iostream>
#include <sstream>
#include <ctime>

// 单例实现
std::shared_ptr<dns_route_manager> dns_route_manager::_instance = nullptr;
std::mutex dns_route_manager::_instance_mutex;

std::shared_ptr<dns_route_manager> dns_route_manager::instance() {
    // 双重检查锁定模式
    if (_instance == nullptr) {
        std::lock_guard<std::mutex> lock(_instance_mutex);
        if (_instance == nullptr) {
            _instance = std::make_shared<dns_route_manager>();
        }
    }
    return _instance;
}

dns_route_manager::dns_route_manager() 
    : _current_data(std::make_unique<route_map>())
    , _temp_data(std::make_unique<route_map>())
    , _current_version(0) {
    
    // 初始化MySQL连接
    mysql_init(&_db_connection);
    
    // 预分配SQL缓冲区
    _sql_buffer.reserve(1024);
}

dns_route_manager::~dns_route_manager() {
    mysql_close(&_db_connection);
}

bool dns_route_manager::connect_database() {
    // 从配置文件加载数据库配置
    auto config = config_file::instance();
    _db_cfg.host = config->GetString("mysql", "db_host", "127.0.0.1");
    _db_cfg.port = config->GetNumber("mysql", "db_port", 3306);
    _db_cfg.user = config->GetString("mysql", "db_user", "root");
    _db_cfg.password = config->GetString("mysql", "db_passwd", "aceld");
    _db_cfg.database = config->GetString("mysql", "db_name", "lars_dns");

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
        return false;
    }

    std::cout << "Successfully connected to database!" << std::endl;
    return true;
}

void dns_route_manager::build_route_map() {
    load_route_data();
    swap_data();
}

void dns_route_manager::load_route_data() {
    // 清空临时数据
    _temp_data->clear();

    // 查询路由数据 
    _sql_buffer = "SELECT modid, cmdid, serverip, serverport FROM RouteData";
    
    if (mysql_query(&_db_connection, _sql_buffer.c_str())) {
        std::cerr << "MySQL query error: " << mysql_error(&_db_connection) << std::endl;
        return;
    }

    MYSQL_RES* result = mysql_store_result(&_db_connection);
    if (!result) {
        std::cerr << "MySQL store result error: " << mysql_error(&_db_connection) << std::endl;
        return;
    }

    // 处理查询结果
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        int modid = std::stoi(row[0]);
        int cmdid = std::stoi(row[1]); 
        uint32_t ip = std::stoul(row[2]);
        uint32_t port = std::stoul(row[3]);

        // 组合modid和cmdid为键
        uint64_t mod_key = (static_cast<uint64_t>(modid) << 32) + cmdid;
        
        // 组合ip和port为值
        uint64_t host_key = (static_cast<uint64_t>(ip) << 32) + port;

        // 添加到临时数据映射中
        (*_temp_data)[mod_key].insert(host_key);
    }

    mysql_free_result(result);

    std::cout << "Loaded " << _temp_data->size() << " route entries from database" << std::endl;
}

host_set dns_route_manager::get_hosts(int modid, int cmdid) {
    uint64_t mod_key = (static_cast<uint64_t>(modid) << 32) + cmdid;
    
    // 使用共享锁进行读操作
    std::shared_lock<std::shared_mutex> lock(_data_mutex);
    
    auto it = _current_data->find(mod_key);
    if (it != _current_data->end()) {
        return it->second;
    }
    
    // 返回空集合
    return host_set{};
}

int dns_route_manager::load_version() {
    _sql_buffer = "SELECT version FROM RouteVersion WHERE id = 1";
    
    if (mysql_query(&_db_connection, _sql_buffer.c_str())) {
        std::cerr << "Load version query error: " << mysql_error(&_db_connection) << std::endl;
        return -1;
    }

    MYSQL_RES* result = mysql_store_result(&_db_connection);
    if (!result) {
        std::cerr << "Load version store result error: " << mysql_error(&_db_connection) << std::endl;
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return -1;
    }

    uint64_t new_version = std::stoull(row[0]);
    mysql_free_result(result);

    if (new_version == _current_version) {
        return 0; // 版本无变化
    }

    _current_version = new_version;
    return 1; // 版本有变化
}

void dns_route_manager::load_changes(std::vector<uint64_t>& change_list) {
    change_list.clear();

    _sql_buffer = "SELECT modid, cmdid FROM RouteChange WHERE version > " + std::to_string(_current_version);
    
    if (mysql_query(&_db_connection, _sql_buffer.c_str())) {
        std::cerr << "Load changes query error: " << mysql_error(&_db_connection) << std::endl;
        return;
    }

    MYSQL_RES* result = mysql_store_result(&_db_connection);
    if (!result) {
        std::cerr << "Load changes store result error: " << mysql_error(&_db_connection) << std::endl;
        return;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        int modid = std::stoi(row[0]);
        int cmdid = std::stoi(row[1]);
        uint64_t mod_key = (static_cast<uint64_t>(modid) << 32) + cmdid;
        change_list.push_back(mod_key);
    }

    mysql_free_result(result);
}

void dns_route_manager::swap_data() {
    // 使用独占锁进行写操作
    std::unique_lock<std::shared_mutex> lock(_data_mutex);
    
    // 交换当前数据和临时数据
    _current_data.swap(_temp_data);
    
    std::cout << "Route data swapped, current routes: " << _current_data->size() << std::endl;
}

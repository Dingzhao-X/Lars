#include "tcp_client.h"
#include "msg.pb.h"
#include <vector>
#include <cstring>
#include <string>
#include <iostream>
using namespace std;

struct Qps{
    Qps(){
        last_time = time(NULL);
        succ_cnt = 0;
    }

    long last_time; //最后一次发包的时间 ms单位
    int succ_cnt; //成功收到服务器回显业务的次数
};

void callback_busi(const char* data, uint32_t len, int msgid, net_connection* conn, void* usr_data){
    //测试qps性能
    Qps* qps = (Qps*)usr_data;

    qps_test::echo_message request, response;

    //解析server返回的数据包
    if (response.ParseFromArray(data, len) == false) {
        cerr << "server call back data error" << endl;
        return;
    }

    //判断消息的内容是否一致
    if (response.content() == "QPS test.") {
        //服务器回显业务成功，认为请求一次成功 qps++
        ++qps->succ_cnt;
    }

    //得到当前时间
    long current_time = time(NULL);
    if (current_time - qps->last_time >= 1) {
        //如果当前时间比最后记录的时间大于1秒，就需要统计qps成功的次数
        cout << "QPS = " << qps->succ_cnt << endl;

        qps->last_time = current_time;
        qps->succ_cnt = 0;
    }

    //发送request给客户端
    request.set_id(response.id() + 1);
    request.set_content(response.content());

    string request_string;
    request.SerializeToString(&request_string);

    conn->conn_write2fd(request_string.c_str(), request_string.size(), msgid);
}

void on_client_build(net_connection* conn, void* args){
    //建立链接成功后，主动发一个包
    qps_test::echo_message request;

    request.set_id(1);
    request.set_content("QPS test.");

    string request_string;
    request.SerializeToString(&request_string);

    int msgid = 1;
    conn->conn_write2fd(request_string.c_str(), request_string.size(), msgid);
}

void* thread_main(void* args){
    event_loop loop;

    tcp_client client(&loop, "127.0.0.1", 7777);

    //创建qps句柄
    Qps qps;

    //注册回显业务函数
    client.add_msg_router(1, callback_busi, (void*)&qps);

    //注册链接Hook函数
    client.set_conn_start(on_client_build);

    loop.event_process();

    return nullptr;
}

int main(int argc, char** argv){
    if(argc == 1){
        cout << "Usage: ./qps_client_tcp [thread_num]" << endl;
        exit(1);
    }

    int thread_num = atoi(argv[1]);
    vector<pthread_t> tids(thread_num);

    for(int i = 0; i < thread_num; ++i){
        pthread_create(&tids[i], NULL, thread_main, NULL);
    }

    for(int i = 0; i < thread_num; ++i)
        pthread_join(tids[i], NULL);

    return 0;
}

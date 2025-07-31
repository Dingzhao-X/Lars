#include "message.h"
#include <iostream>

//构造函数，初始化两个map
msg_router::msg_router(): _msgid2router(), _msgid2args(){
    printf("Router init succ.\n");
//    cout << "Router init succ." << endl;
}

//注册一个msgid和对应回调函数的映射
int msg_router::register_msg_router(int msgid, msg_callback msg_cb, void* usr_data){
    if(_msgid2router.find(msgid) != _msgid2router.end())
        cout << "Callback for msgID: " << msgid << "has already existed. Updated now." << endl;
    _msgid2router[msgid] = msg_cb;
    _msgid2args[msgid] = usr_data;
    return 0;
}

//调用对应回调函数的函数
void msg_router::call(int msgid, uint32_t msglen, const char* data, net_connection* conn){
    if(_msgid2router.find(msgid) == _msgid2router.end()){
        cout << "Callback for msgID " << msgid << "is not registered." << endl;
        return;
    }

    auto callback = _msgid2router[msgid];  //注意，auto接map第二个值如果value不存在，会插入默认值。
    auto usr_data = _msgid2args[msgid];     //所以前面的find是必要的
    callback(data, msglen, msgid, conn, usr_data);
    //cout << "========================================================" << endl;
}

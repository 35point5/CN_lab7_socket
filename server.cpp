#include <stdio.h>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <set>
#include <mutex>
#include <queue>
#include <map>
#include "defs.h"

using namespace std;

mutex client_mutex;
map<int, SOCKET> clients;
int client_id;
enum UserOp {
    Exit = 66
};
struct Msg {
    UserOp type;
    string data;
};
mutex msg_mutex;
queue<Msg> msg_q;

void Send(OpType t, string s, SOCKET server_s) {
    char buf[buf_size];
    buf[0] = BeginPackage; //数据包头部
    buf[1] = t; //数据包类型
    memcpy(buf + 2, s.c_str(), s.size()); //数据包内容
    buf[2+s.size()] = EndPackage; //数据包结束
    send(server_s, buf, 3 + s.size(), 0);
}

void SendTime(SOCKET client_s, int cid) {
    cout << "Send time to " << cid << endl;
    time_t t = time(nullptr);
    Send(RespTime, TO_STRING(t), client_s);
}

void SendName(SOCKET client_s, int cid) {
    cout << "Send name to " << cid << endl;
    Send(RespName, host_name, client_s);
}

void SendList(SOCKET client_s, int cid) {
    cout << "Send list to " << cid << endl;
    client_mutex.lock(); //锁定客户端列表
    string s;
    for (auto o: clients) { //枚举所有客户端
        sockaddr_in addr; //客户端地址
        int len = sizeof(addr);
        getpeername(o.second, (sockaddr *) &addr, &len);
        s += TO_STRING(o.first) + TO_STRING(addr);
    }
    client_mutex.unlock();
    Send(RespList, s, client_s);
}

void SendAndResp(SOCKET s, const string &data, int cid) {
    int id;
    string msg;
    id = *(int *) data.c_str();
    msg = data.substr(sizeof(int));
    int err;
    if (clients.count(id)) { //如果客户端存在
        cout<<"Send message to "<<id<<endl;
        Send(RcvMsg, TO_STRING(cid) + msg, clients[id]); //发送消息给目标
        err = 0;
        cout<<"Response send message success to "<<cid<<endl;
        Send(RespMsg, TO_STRING(err), s); //回复发送成功
    } else { //如果客户端不存在
        err = 1;
        cout<<"Response send message failed to "<<cid<<endl;
        Send(RespMsg, TO_STRING(err), s); //回复发送失败
    }
}

void Receiver(SOCKET client_s) {
    client_mutex.lock();
    int cur_id = client_id;
    clients[client_id++] = client_s;
    client_mutex.unlock();
    sockaddr_in addr;
    int len = sizeof(addr);
    getpeername(client_s, (sockaddr *) &addr, &len);
    cout<<"Client "<<cur_id<<": "<<inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)<<" connected"<<endl;
    char buf[buf_size];
    string data;
    char *buf_begin, *buf_end;
    buf_begin = buf_end = buf;
    Stage cur = EndStage;
    bool is_trans = false;
    OpType type;
    while (1) {
        int sz = recv(client_s, buf, buf_size, 0); //接收数据
        if (sz <= 0) {
            if (!sz || WSAGetLastError() == WSAECONNRESET) { //连接关闭
                cout<<"Client "<<cur_id<<" disconnected"<<endl;
            }
            else{
                cout << "client "<<cur_id<<" error:" << WSAGetLastError() << endl;
            }
            break;
        } //错误处理
        if (sz > 0) {
            buf_begin = buf;
            buf_end = buf + sz; //设置缓冲区指针
            while (buf_begin != buf_end) {
                if (cur == EndStage && *buf_begin == BeginPackage) { //识别数据包头部
                    cur = BeginStage; //当前状态
                } else if (cur == BeginStage) { //识别数据包类型
                    cur = TypeStage;
                    switch (*buf_begin) {
                        case GetTime: {
                            type = GetTime; //设置类型
                            break;
                        }
                        case GetName: {
                            type = GetName;
                            break;
                        }
                        case GetList: {
                            type = GetList;
                            break;
                        }
                        case SendMsg: {
                            type = SendMsg;
                            break;
                        }
                    }
                } else if (cur == TypeStage && *buf_begin != EndPackage) { //识别数据包内容
                    cur = DataStage;
                } else if ((cur == DataStage || cur == TypeStage) && *buf_begin == EndPackage && !is_trans) { //识别数据包尾部，非转义
                    cur = EndStage;
                    switch (type) { //根据类型进行处理
                        case GetTime: {
                            SendTime(client_s, cur_id);
                            break;
                        }
                        case GetName: {
                            SendName(client_s, cur_id);
                            break;
                        }
                        case GetList: {
                            SendList(client_s, cur_id);
                            break;
                        }
                        case SendMsg: {
                            SendAndResp(client_s, data, cur_id);
                            break;
                        }
                    }
                    data = "";
                }
                if (cur == DataStage) { //处理数据
                    if (is_trans) { //需要转义
                        is_trans = false;
                        data.push_back(*buf_begin);
                    } else if (*buf_begin == (char) Trans) {
                        is_trans = true;
                    } else {
                        data.push_back(*buf_begin);
                    }
                }
                ++buf_begin;
            }
        }
    }
    client_mutex.lock();
    closesocket(client_s);
    clients.erase(cur_id);
    client_mutex.unlock();
}

void UserHandler() {
    string op;
    Msg temp;
    while (1) {
        cin >> op;
        if (op == "exit") {
            temp.type = Exit;
            msg_mutex.lock();
            msg_q.push(temp);
            msg_mutex.unlock();
        }
    }
}

int main(int argc, char *argv[]) {
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return 0;
    }

    SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (slisten == INVALID_SOCKET) {
        printf("socket error !");
        return 0;
    }
    cout << "socket created" << endl;
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(4495);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(slisten, (LPSOCKADDR) &sin, sizeof(sin)) == SOCKET_ERROR) {
        printf("bind error !");
    }
    cout << "bind success" << endl;
    if (listen(slisten, 5) == SOCKET_ERROR) {
        printf("listen error !");
        return 0;
    }
    cout << "listen success" << endl;
    u_long a = 1;
    ioctlsocket(slisten, FIONBIO, &a);

    SOCKET sClient;
    sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);
    fflush(stdout);
    bool run = true;
    thread user_handler(UserHandler);
    user_handler.detach();
    while (run) {
        if (!msg_q.empty()) { //查看消息队列是否为空
            msg_mutex.lock(); //加锁
            Msg temp = msg_q.front(); //取出队首元素
            msg_q.pop(); //删除队首元素
            msg_mutex.unlock(); //解锁
            switch (temp.type) { //判断消息类型
                case Exit: { //如果是退出消息
                    client_mutex.lock();
                    for (auto o: clients) {
                        closesocket(o.second);
                    }
                    client_mutex.unlock();
                    run = false;
                    break;
                }
            }
        }
        sClient = accept(slisten, (SOCKADDR *) &remoteAddr, &nAddrlen); //接受客户端连接
        if (sClient == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) { //如果原因是非阻塞，继续循环
                continue;
            }
            printf("accept error !");
            continue;
        }
        a = 0;
        ioctlsocket(sClient, FIONBIO, &a); //设置为阻塞模式
        thread t(Receiver, sClient); //创建线程
        t.detach(); //分离线程
    }

    closesocket(slisten);
    WSACleanup();
    return 0;
}

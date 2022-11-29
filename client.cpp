#include<WINSOCK2.H>
#include<stdio.h>
#include<iostream>
#include<cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <sstream>
#include <iomanip>
#include "defs.h"

using namespace std;

enum UserOp {
    UserConnect = 99,
    UserClose,
    UserExit,
    CloseConnection
};
union MsgType {
    OpType op;
    UserOp user;
};
struct Msg {
    MsgType type;
    string data;
};
mutex msg_mutex;
queue<Msg> msg_q;

void PushQ(Msg m) {
    msg_mutex.lock();
    msg_q.push(m);
    msg_mutex.unlock();
}

void Send(OpType t, string s, SOCKET server_s) {
    char buf[buf_size];
    buf[0] = BeginPackage;
    buf[1] = t;
    memcpy(buf + 2, s.c_str(), s.size());
    buf[2+s.size()] = EndPackage;
    send(server_s, buf, 3 + s.size(), 0);
}

void Receiver(SOCKET peer_s) {
    char buf[buf_size];
    string data;
    char *buf_begin, *buf_end;
    buf_begin = buf_end = buf;
    Stage cur = EndStage;
    bool is_trans = false;
    OpType type;
    Msg temp;
    while (1) {
        int sz = recv(peer_s, buf, buf_size, 0); //接收数据
        if (sz <= 0) {
            temp.type.user = CloseConnection;
            PushQ(temp);
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
                        case RespTime: {
                            type = RespTime;
                            break;
                        }
                        case RespName: {
                            type = RespName;
                            break;
                        }
                        case RespList: {
                            type = RespList;
                            break;
                        }
                        case RespMsg: {
                            type = RespMsg;
                            break;
                        }
                        case RcvMsg: {
                            type = RcvMsg;
                            break;
                        }
                    }
                } else if (cur == TypeStage && *buf_begin != EndPackage) { //识别数据包内容
                    cur = DataStage;
                } else if ((cur == DataStage || cur == TypeStage) && *buf_begin == EndPackage && !is_trans) { //识别数据包结束符，非转义
                    cur = EndStage;
                    switch (type) { //根据数据包类型进行处理
                        case RespTime: {
                            temp.type.op = RespTime;
                            temp.data = data;
                            PushQ(temp); //将数据包压入消息队列
                            break;
                        }
                        case RespName: {
                            temp.type.op = RespName;
                            temp.data = data;
                            PushQ(temp);
                            break;
                        }
                        case RespList: {
                            temp.type.op = RespList;
                            temp.data = data;
                            PushQ(temp);
                            break;
                        }
                        case RespMsg: {
                            temp.type.op = RespMsg;
                            temp.data = data;
                            PushQ(temp);
                            break;
                        }
                        case RcvMsg: {
                            temp.type.op = RcvMsg;
                            temp.data = data;
                            PushQ(temp);
                            break;
                        }
                    }
                    data = "";
                }
                if (cur == DataStage) { //处理内容
                    if (is_trans) { //当前字节需要转义
                        is_trans = false;
                        data.push_back(*buf_begin);
                    } else if (*buf_begin == (char) Trans) { //转义标志符
                        is_trans = true;
                    } else { //直接添加至数据
                        data.push_back(*buf_begin);
                    }
                }
                ++buf_begin; //指针后移
            }
        }
    }
}

void UserHandler() {
    string op;
    Msg temp;
    while (1) {
        cin >> op;
        if (op == "connect") {
            temp.type.user = UserConnect;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "close") {
            temp.type.user = UserClose;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "exit") {
            temp.type.user = UserExit;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "getname") {
            temp.type.op = GetName;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "getlist") {
            temp.type.op = GetList;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "gettime") {
            temp.type.op = GetTime;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "send") {
            temp.type.op = SendMsg;
            getline(cin, temp.data);
            PushQ(temp);
        } else {
            cout << "Unknown command!" << endl;
        }
    }
}

void ReqTime(SOCKET s) {
    Send(GetTime, "", s);
}

void PrintTime(time_t t) {
    cout << std::put_time(localtime(&t), "%x %X") << endl;
    flush(cout);
}

void ReqName(SOCKET s) {
    Send(GetName, "", s);
}

void PrintName(const string &s) {
    cout << "Server's name: " << s << endl;
    flush(cout);
}

void ReqList(SOCKET s) {
    Send(GetList, "", s);
}

void PrintList(const string &s) {
    cout << "Client list:" << endl;
    for (int i = 0; i < s.length(); i += sizeof(int) + sizeof(sockaddr_in)) {
        int id = *((int *) (s.c_str() + i));
        sockaddr_in addr = *((sockaddr_in *) (s.c_str() + i + sizeof(int)));
        cout << id << ": " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << endl;
    }
    flush(cout);
}

void PostMsg(const string &s, SOCKET server_s) {
    stringstream ss(s);
    int id;
    string msg;
    ss >> id;
    getline(ss, msg);
    Send(SendMsg, TO_STRING(id) + msg, server_s);
}

void PrintRespMsg(const string &s) {
    if (!s[0]) {
        cout << "Send success!" << endl;
    } else {
        cout << "Send failed!" << endl;
    }
    flush(cout);
}

void PrintRcvMsg(const string &s) {
    int id = *((int *) s.c_str());
    string msg = s.substr(sizeof(int));
    cout << "From " << id << ":" << msg << endl;
    flush(cout);
}

void PrintMenu() {
    cout << "Menu:" << endl;
    cout << "connect <ip> <port> - connect to server" << endl;
    cout << "close - close connection" << endl;
    cout << "gettime - get server time" << endl;
    cout << "getname - get server name" << endl;
    cout << "getlist - get client list" << endl;
    cout << "send <id> <msg> - send message to client" << endl;
    cout << "exit - exit program" << endl;
    flush(cout);
}

int main() {
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA data;
    if (WSAStartup(sockVersion, &data) != 0) {
        cout << WSAGetLastError() << "!!!" << endl;
        return 0;
    }
    PrintMenu();
    thread user_handler(UserHandler);
    user_handler.detach();
    string op;
    bool connected = false;
    SOCKET sclient;
    while (1) {
        if (!msg_q.empty()) { //判断消息队列是否有消息
            msg_mutex.lock(); //加锁
            Msg temp = msg_q.front(); //取出消息
            msg_q.pop(); //删除消息
            msg_mutex.unlock(); //解锁
            if (temp.type.user == UserConnect) { //用户连接请求
                sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //创建套接字
                if (sclient == INVALID_SOCKET) {
                    printf("invalid socket!");
                    continue;
                }
                stringstream ss(temp.data);
                string ip;
                int port;
                ss >> ip >> port; //获取ip和端口
                sockaddr_in serAddr; //设置服务器地址
                serAddr.sin_family = AF_INET;
                serAddr.sin_port = htons(port);
                serAddr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());
                if (connect(sclient, (sockaddr *) &serAddr, sizeof(serAddr)) == SOCKET_ERROR) {  //连接失败
                    printf("connect error !");
                    closesocket(sclient);
                    continue;
                }
                cout << "Connected!" << endl;
                connected = true; //连接成功
                thread rcv_t(Receiver, sclient); //创建接收线程
                rcv_t.detach(); //分离线程
            } else if (temp.type.user == CloseConnection || temp.type.user == UserClose) { //关闭连接
                if (connected) { //如果已经连接
                    closesocket(sclient);
                    connected = false;
                    cout << "Connection closed." << endl;
                }
            } else if (temp.type.user == UserExit) { //退出程序
                if (connected) {
                    closesocket(sclient);
                    connected = false;
                }
                break;
            } else if (temp.type.op == RespTime) { //处理服务器时间响应
                PrintTime(*(time_t *) temp.data.c_str());
            } else if (temp.type.op == GetName) { //发送服务器名称请求
                ReqName(sclient);
            } else if (temp.type.op == RespName) { //处理服务器名称响应
                PrintName(temp.data);
            } else if (temp.type.op == GetList) { //发送客户端列表请求
                ReqList(sclient);
            } else if (temp.type.op == RespList) { //处理客户端列表响应
                PrintList(temp.data);
            } else if (temp.type.op == GetTime) { //发送服务器时间请求
                ReqTime(sclient);
            } else if (temp.type.op == SendMsg) { //向指定客户端发送消息
                PostMsg(temp.data, sclient);
            } else if (temp.type.op == RespMsg) { //处理消息发送响应
                PrintRespMsg(temp.data);
            } else if (temp.type.op == RcvMsg) { //处理接收到的消息
                PrintRcvMsg(temp.data);
            }
        }
    }
    WSACleanup();
    return 0;
}

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
    buf[0] = BeginPackage;
    buf[1] = t;
    send(server_s, buf, 2, 0);
    int i = 0;
    for (; i < s.length(); i += buf_size) {
        memcpy(buf, s.c_str() + i, min(buf_size, (int) s.length() - i));
        send(server_s, buf, min(buf_size, (int) s.length() - i), 0);
    }
    buf[0] = EndPackage;
    send(server_s, buf, 1, 0);
}

void SendTime(SOCKET client_s) {
    time_t t = time(nullptr);
    cout << "time_t" << t << endl;
    Send(RespTime, TO_STRING(t), client_s);
}

void SendName(SOCKET client_s) {
    cout << "name:" << host_name << endl;
    Send(RespName, host_name, client_s);
}

void SendList(SOCKET client_s) {
    client_mutex.lock();
    string s;
    string s1,s2;
    for (auto o: clients) {
        sockaddr_in addr;
        int len = sizeof(addr);
        getpeername(o.second, (sockaddr *) &addr, &len);
        s += TO_STRING(o.first) + TO_STRING(addr);
    }
    cout<<"list:";
    for (auto o: s) printf("%2x ",o);
    puts("");
    fflush(stdout);
    flush(cout);
    client_mutex.unlock();
    Send(RespList,s,client_s);
}

void Receiver(SOCKET client_s) {
    client_mutex.lock();
    int cur_id = client_id;
    clients[client_id++] = client_s;
    client_mutex.unlock();
    char buf[buf_size];
    string data;
    char *buf_begin, *buf_end;
    buf_begin = buf_end = buf;
    Stage cur = EndStage;
    bool is_trans = false;
    OpType type;
    while (1) {
        int sz = recv(client_s, buf, buf_size, 0);
//        cout<<"sz:"<<sz<<endl;
        if (sz <= 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
            cout << "err:" << WSAGetLastError() << endl;
            break;
        }
//        buf[sz]=0;


        if (sz > 0) {
            for (int jj = 0; jj < sz; ++jj) {
                printf("%2x ", buf[jj]);
            }
            puts("");
            buf_begin = buf;
            buf_end = buf + sz;
            while (buf_begin != buf_end) {
                if (cur == EndStage && *buf_begin == BeginPackage) {
                    cout << "Begin" << endl;
                    cur = BeginStage;
                } else if (cur == BeginStage) {
                    cout << "OpType" << endl;
                    cur = TypeStage;
                    switch (*buf_begin) {
                        case GetTime: {
                            type = GetTime;
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
                    }
                } else if (cur == TypeStage && *buf_begin != EndPackage) {
                    cout << "Data" << endl;
                    cur = DataStage;
                } else if ((cur == DataStage || cur == TypeStage) && *buf_begin == EndPackage && !is_trans) {
                    cout << "End" << endl;
                    cur = EndStage;
                    switch (type) {
                        case GetTime: {
                            cout << "sendtime!!" << endl;
                            SendTime(client_s);
                            cout << data << endl;
                            break;
                        }
                        case GetName: {
                            SendName(client_s);
                            break;
                        }
                        case GetList: {
                            SendList(client_s);
                            break;
                        }
                    }
                    data = "";
                }
                if (cur == DataStage) {
                    if (is_trans) {
                        is_trans = false;
                        cerr << *buf_begin << endl;
                        flush(cerr);
                        data.push_back(*buf_begin);
                    } else if (*buf_begin == (char) Trans) {
                        is_trans = true;
                    } else {
                        cerr << *buf_begin << endl;
                        flush(cerr);
                        data.push_back(*buf_begin);
                    }
                }
                ++buf_begin;
            }
            cout << "okk" << endl;
        }
    }
    cout << "rcv exit!!!" << endl;
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
    //初始化WSA  
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return 0;
    }
    //创建套接字
    SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (slisten == INVALID_SOCKET) {
        printf("socket error !");
        return 0;
    }
    //绑定IP和端口
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(4495);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(slisten, (LPSOCKADDR) &sin, sizeof(sin)) == SOCKET_ERROR) {
        printf("bind error !");
    }
    //开始监听
    if (listen(slisten, 5) == SOCKET_ERROR) {
        printf("listen error !");
        return 0;
    }
    u_long a = 1;
    cout << "ioerr:" << ioctlsocket(slisten, FIONBIO, &a) << endl;

    //循环接收数据
    SOCKET sClient;
    sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);
    fflush(stdout);
    bool run = true;
    thread user_handler(UserHandler);
    user_handler.detach();
    while (run) {
//        cout<<"running"<<endl;
        if (!msg_q.empty()) {
            msg_mutex.lock();
            Msg temp = msg_q.front();
            msg_q.pop();
            msg_mutex.unlock();
            switch (temp.type) {
                case Exit: {
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
        sClient = accept(slisten, (SOCKADDR *) &remoteAddr, &nAddrlen);
        if (sClient == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                continue;
            }
            printf("accept error !");
            continue;
        }
        thread t(Receiver, sClient);
        t.detach();

//        printf("接受到一个连接：%s \r\n", inet_ntoa(remoteAddr.sin_addr));
//
//        //接收数据
//        int ret = recv(sClient, revData, 255, 0);
//        if(ret > 0)
//        {
//            revData[ret] = 0x00;
//            printf(revData);
//        }
//
//        //发送数据
//        const char * sendData = "你好，TCP客户端！\n";
//        send(sClient, sendData, strlen(sendData), 0);
//        closesocket(sClient);
    }

    closesocket(slisten);
    WSACleanup();
    return 0;
}

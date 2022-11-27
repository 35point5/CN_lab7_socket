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
    UserConnect = 88,
    UserClose
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
    send(server_s, buf, 2, 0);
    int i = 0;
    for (; i < s.length(); i += buf_size) {
        memcpy(buf, s.c_str() + i, min(buf_size, (int) s.length() - i));
        send(server_s, buf, min(buf_size, (int) s.length() - i), 0);
    }
    buf[0] = EndPackage;
    send(server_s, buf, 1, 0);
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
        int sz = recv(peer_s, buf, buf_size, 0);
        if (sz <= 0) {
            if (sz != -1) cout << sz << endl;
            if (WSAGetLastError() != WSAEWOULDBLOCK || !sz) {
                cout << "exit" << endl;
                temp.type.op = CloseConnection;
                PushQ(temp);
                break;
            }
        }
//        buf[sz]=0;
//        cout<<"sz:"<<sz<<endl;
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
                    cout << "type:";
                    switch (*buf_begin) {
                        case RespTime: {
                            cout << RespTime << endl;
                            type = RespTime;
                            break;
                        }
                        case RespName: {
                            type = RespName;
                            break;
                        }
                        case RespList: {
                            cout<<"resplist"<<endl;
                            type = RespList;
                            break;
                        }
                    }
                } else if (cur == TypeStage && *buf_begin != EndPackage) {
                    cout << "Data" << endl;
                    cur = DataStage;
                } else if ((cur == DataStage || cur == TypeStage) && *buf_begin == EndPackage && !is_trans) {
                    cout << "End" << endl;
                    cur = EndStage;
                    cout << data << endl;
                    switch (type) {
                        case RespTime: {
                            cout << "gettime !!" << endl;
                            temp.type.op = RespTime;
                            temp.data = data;
                            cout << "time_t:" << *(time_t *) data.c_str() << endl;
                            PushQ(temp);
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

        }
    }
    cout << "Receiver end" << endl;
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
            cout << "input exit" << endl;
            temp.type.user = UserClose;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "getname") {
            temp.type.op = GetName;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op == "getlist"){
            temp.type.op=GetList;
            getline(cin, temp.data);
            PushQ(temp);
        } else if (op =="gettime"){
            temp.type.op=GetTime;
            getline(cin, temp.data);
            PushQ(temp);
        }
    }
}

void ReqTime(SOCKET s) {
    cout << "req time" << endl;
    Send(GetTime, "", s);
}

void PrintTime(time_t t) {
    cout << put_time(localtime(&t), "%x %X") << endl;
    flush(cout);
}

void ReqName(SOCKET s) {
    cout << "req name" << endl;
    Send(GetName, "", s);
}

void PrintName(const string &s) {
    cout << "Server's name: " << s << endl;
    flush(cout);
}

void ReqList(SOCKET s){
    Send(GetList,"",s);
}

void PrintList(const string &s){
    cout<<"Client list:"<<endl;
    for (int i=0;i<s.length();i+=sizeof(int)+sizeof(sockaddr_in)){
        int id=*((int*)(s.c_str()+i));
        sockaddr_in addr=*((sockaddr_in*)(s.c_str()+i+sizeof(int)));
        cout<<id<<": "<<inet_ntoa(addr.sin_addr)<<":"<<ntohs(addr.sin_port)<<endl;
    }
}

int main() {
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA data;
    if (WSAStartup(sockVersion, &data) != 0) {
        return 0;
    }

    thread user_handler(UserHandler);
    user_handler.detach();
    string op;
    bool connected = false;
    SOCKET sclient;
    while (1) {
        if (!msg_q.empty()) {
            msg_mutex.lock();
            Msg temp = msg_q.front();
            msg_q.pop();
            msg_mutex.unlock();
            cout << "msg!!!" << endl;
            if (temp.type.user == UserConnect) {
                sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sclient == INVALID_SOCKET) {
                    printf("invalid socket!");
                    return 0;
                }
                stringstream ss(temp.data);
                string ip;
                int port;
                ss >> ip >> port;
                sockaddr_in serAddr;
                serAddr.sin_family = AF_INET;
                serAddr.sin_port = htons(port);
                serAddr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());
                if (connect(sclient, (sockaddr *) &serAddr, sizeof(serAddr)) == SOCKET_ERROR) {  //连接失败
                    printf("connect error !");
                    closesocket(sclient);
                    return 0;
                }
                connected = true;
                u_long a = 1;
                cout << "ioerr:" << ioctlsocket(sclient, FIONBIO, &a) << endl;
                thread rcv_t(Receiver, sclient);
                rcv_t.detach();
            } else if (temp.type.op == CloseConnection || temp.type.user == UserClose) {
                cout << "closed" << endl;
                closesocket(sclient);
                connected = false;
            } else if (temp.type.op == RespTime) {
                PrintTime(*(time_t *) temp.data.c_str());
            } else if (temp.type.op == GetName) {
                ReqName(sclient);
            } else if (temp.type.op == RespName) {
                PrintName(temp.data);
            } else if (temp.type.op == GetList) {
                ReqList(sclient);
            } else if (temp.type.op == RespList) {
                PrintList(temp.data);
            } else if (temp.type.op == GetTime){
                ReqTime(sclient);
            }
        }
    }






//    char recData[255];
//    int ret = recv(sclient, recData, 255, 0);
//    if (ret > 0) {
//        recData[ret] = 0x00;
//        printf(recData);
//    }
    closesocket(sclient);


    WSACleanup();
    return 0;

}

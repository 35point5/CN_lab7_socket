#include<WINSOCK2.H>
#include<stdio.h>
#include<iostream>
#include<cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <sstream>
#include "defs.h"

using namespace std;

enum UserOp{
    Connect=88
};
union MsgType{
    OpType op;
    UserOp user;
};
struct Msg{
    MsgType type;
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
    buf[0]=EndPackage;
    send(server_s, buf, 1, 0);
}

void Receiver(SOCKET peer_s){
    char buf[buf_size];
    string data;
    char *buf_begin,*buf_end;
    buf_begin=buf_end=buf;
    Stage cur=EndStage;
    bool is_trans=false;
    OpType type;
    Msg temp;
    while (1){
        int sz = recv(peer_s, buf, buf_size, 0);
        if (sz <= 0) {
            if (GetLastError()!=WSAEWOULDBLOCK){
                temp.type.op=CloseConnection;
                msg_mutex.lock();
                msg_q.push(temp);
                msg_mutex.unlock();
                break;
            }
        }
        buf[sz]=0;
        cout<<"sz:"<<sz<<endl;
        for (auto o:buf){
            printf("%2x ",o);
        }
        puts("");
        if (sz>0){
            buf_begin=buf;
            buf_end=buf+sz;
            while (buf_begin!=buf_end){
                if (cur == EndStage && *buf_begin == BeginPackage){
                    cout<<"Begin"<<endl;
                    cur=BeginStage;
                }
                else if (cur == BeginStage){
                    cout<<"OpType"<<endl;
                    cur=TypeStage;
                    switch (*buf_begin) {
                        case GetTime:{
                            type=GetTime;
                            break;
                        }
                    }
                }
                else if (cur == TypeStage){
                    cout<<"Data"<<endl;
                    cur=DataStage;
                }
                else if (cur == DataStage && *buf_begin == EndPackage && !is_trans){
                    cout<<"End"<<endl;
                    cur=EndStage;
                    cout<<data<<endl;
                    switch (type) {
                        case GetTime:{

                            break;
                        }
                    }
                }
                if (cur==DataStage){
                    if (is_trans){
                        is_trans=false;
                        cerr<<*buf_begin<<endl;
                        flush(cerr);
                        data.push_back(*buf_begin);
                    }
                    else if (*buf_begin == (char)Trans){
                        is_trans=true;
                    } else{
                        cerr<<*buf_begin<<endl;
                        flush(cerr);
                        data.push_back(*buf_begin);
                    }
                }
                ++buf_begin;
            }
        }
    }
    cout<<"Receiver end"<<endl;
}

void UserHandler(){
    string op;
    Msg temp;
    while (1){
        cin>>op;
        if (op=="connect"){
            temp.type.user=Connect;
            getline(cin,temp.data);
            msg_mutex.lock();
            msg_q.push(temp);
            msg_mutex.unlock();
        }
    }
}

int main() {
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA data;
    if (WSAStartup(sockVersion, &data) != 0) {
        return 0;
    }
    SOCKET sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sclient == INVALID_SOCKET) {
        printf("invalid socket!");
        return 0;
    }

    thread user_handler(UserHandler);
    user_handler.detach();
    string op;
    bool connected=false;
    while (1){
        if (!msg_q.empty()){
            msg_mutex.lock();
            Msg temp=msg_q.front();
            msg_q.pop();
            msg_mutex.unlock();
            if (temp.type.user==Connect){
                stringstream ss(temp.data);
                string ip;
                int port;
                ss>>ip>>port;
                sockaddr_in serAddr;
                serAddr.sin_family = AF_INET;
                serAddr.sin_port = htons(port);
                serAddr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());
                if (connect(sclient, (sockaddr *) &serAddr, sizeof(serAddr)) == SOCKET_ERROR) {  //连接失败
                    printf("connect error !");
                    closesocket(sclient);
                    return 0;
                }
                connected=true;
                u_long a=1;
                cout<<"ioerr:"<<ioctlsocket(sclient, FIONBIO, &a)<<endl;
                thread rcv_t(Receiver,sclient);
                rcv_t.detach();
                Send(GetTime, "hello", sclient);
            }
            else if (temp.type.op==CloseConnection){
                cout<<"closed"<<endl;
                closesocket(sclient);
                connected=false;
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

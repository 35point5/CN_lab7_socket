#include <stdio.h>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <set>
#include <mutex>
#include <queue>
#include "defs.h"
using namespace std;

mutex client_mutex;
set<SOCKET> clients;
enum UserOp{
    Exit=66
};
struct Msg{
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
    buf[0]=EndPackage;
    send(server_s, buf, 1, 0);
}
void SendTime(SOCKET client_s){
    Send(GetTime,"time",client_s);
}
void Receiver(SOCKET client_s){
    client_mutex.lock();
    clients.insert(client_s);
    client_mutex.unlock();
    char buf[buf_size];
    string data;
    char *buf_begin,*buf_end;
    buf_begin=buf_end=buf;
    Stage cur=EndStage;
    bool is_trans=false;
    OpType type;
    while (1){
        int sz = recv(client_s, buf, buf_size, 0);
        if (sz <= 0) {
            break;
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
                    switch (type) {
                        case GetTime:{
                            SendTime(client_s);
                            cout<<data<<endl;
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
    client_mutex.lock();
    clients.erase(client_s);
    client_mutex.unlock();
}

void UserHandler(){
    string op;
    Msg temp;
    while (1){
        cin>>op;
        if (op=="exit"){
            temp.type=Exit;
            msg_mutex.lock();
            msg_q.push(temp);
            msg_mutex.unlock();
        }
    }
}

int main(int argc, char* argv[])
{
    //初始化WSA  
    WORD sockVersion = MAKEWORD(2,2);
    WSADATA wsaData;
    if(WSAStartup(sockVersion, &wsaData)!=0)
    {
        return 0;
    }
    //创建套接字
    SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(slisten == INVALID_SOCKET)
    {
        printf("socket error !");
        return 0;
    }
    //绑定IP和端口
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(4495);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    if(bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
    {
        printf("bind error !");
    }
    //开始监听
    if(listen(slisten, 5) == SOCKET_ERROR)
    {
        printf("listen error !");
        return 0;
    }
    u_long a=1;
    cout<<"ioerr:"<<ioctlsocket(slisten, FIONBIO, &a)<<endl;

    //循环接收数据
    SOCKET sClient;
    sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);
    char revData[255];
    fflush(stdout);
    bool run=true;
    thread user_handler(UserHandler);
    user_handler.detach();
    while (run)
    {
//        cout<<"running"<<endl;
        if (!msg_q.empty()){
            msg_mutex.lock();
            Msg temp=msg_q.front();
            msg_q.pop();
            msg_mutex.unlock();
            switch (temp.type) {
                case Exit:{
                    client_mutex.lock();
                    for (auto o:clients){
                        closesocket(o);
                    }
                    client_mutex.unlock();
                    run=false;
                    break;
                }
            }
        }
        sClient = accept(slisten, (SOCKADDR *)&remoteAddr, &nAddrlen);
        if(sClient == INVALID_SOCKET)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK){
                this_thread::sleep_for(chrono::milliseconds(100));
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

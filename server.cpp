#include <stdio.h>
#include <winsock2.h>
#include <iostream>
#include "defs.h"
using namespace std;

enum Stage{
    BeginStage,
    TypeStage,
    DataStage,
    EndStage
};
const int buf_size=4096;
void SendTime(SOCKET client_s){
    cout<<"666"<<endl;
}
void ClientHandler(SOCKET client_s){
    char buf[buf_size];
    char data[buf_size];
    char *buf_begin,*buf_end,*data_p;
    buf_begin=buf_end=buf;
    data_p=data;
    Stage cur=EndStage;
    bool is_trans=false;
    Type type;
    while (1){
        int sz = recv(client_s, buf, buf_size, 0);
        if (sz>0){
            buf_begin=buf;
            buf_end=buf+sz;
            while (buf_begin!=buf_end){
                if (cur == EndStage && *buf_begin == BeginPackage){
                    cur=BeginStage;
                }
                if (cur == BeginStage){
                    cur=TypeStage;
                    switch (*buf_begin) {
                        case GetTime:{
                            type=GetTime;
                            break;
                        }
                    }
                }
                if (cur == TypeStage){
                    cur=DataStage;
                }
                if (cur == DataStage && *buf_begin == EndPackage && !is_trans){
                    cur=EndStage;
                    switch (type) {
                        case GetTime:{
                            SendTime(client_s);
                            break;
                        }
                    }
                }
                if (cur==DataStage){
                    if (is_trans){
                        is_trans=false;
                        *data_p=*buf_begin;
                    }
                    else if (*buf_begin == (char)Trans){
                        is_trans=true;
                    } else{
                        *data_p=*buf_begin;
                    }
                }
                ++buf_begin;
            }
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

    //循环接收数据
    SOCKET sClient;
    sockaddr_in remoteAddr;
    int nAddrlen = sizeof(remoteAddr);
    char revData[255];
    fflush(stdout);
    while (true)
    {
        printf("等待连接...\n");
        sClient = accept(slisten, (SOCKADDR *)&remoteAddr, &nAddrlen);
        if(sClient == INVALID_SOCKET)
        {
            printf("accept error !");
            continue;
        }
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

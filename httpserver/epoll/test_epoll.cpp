#include <sys/epoll.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
using namespace std;

int main()
{
    
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock == -1)
    {
        cout << "sock error.\n";
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(998);
    cout << bind(sock,(sockaddr*)&addr,sizeof(addr));
    cout << listen(sock,100);
    
    int epfd = epoll_create(200);cout << epfd;
    struct epoll_event ev;
    ev.data.fd = sock;
    ev.events = EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,sock,&ev);
    while(1)
    {
        //printf("wating\n");
        struct epoll_event total_ev[200];
        int active_num = epoll_wait(epfd,total_ev,200,-1);
        if(active_num < 0)  printf("%s\n",strerror(errno));
        for(int i  = 0; i < active_num;++i)
        {
            printf("here\n");
            if(total_ev[i].data.fd == sock)
            {
                sockaddr tmp_addr;
                memset(&tmp_addr,0,sizeof(tmp_addr));
                socklen_t len = sizeof(tmp_addr);
                int ret = accept(sock,&tmp_addr,&len);
                printf("有新连接：%d\n",ret);
                epoll_ctl(epfd,EPOLL_CTL_MOD,sock,&ev);
            }
        }
    }
}
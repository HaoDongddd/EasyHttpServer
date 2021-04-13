#include <sys/epoll.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
using namespace std;

int sock;
void do_sth(int)
{
    char buf[1024*90]={1};
    write(sock,buf,sizeof(buf));
    alarm(1);
}
int main()
{
    int fd[2];
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd) == 0);
    sock = fd[0];
    int epfd = epoll_create(200);
    struct epoll_event ev;
    ev.data.fd = fd[1];
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd,EPOLL_CTL_ADD,fd[1],&ev);
    signal(SIGALRM,do_sth);
    alarm(1);
    while(1)
    {
        //printf("wating\n");
        struct epoll_event total_ev[200];
        int active_num = epoll_wait(epfd,total_ev,200,-1);
        printf("active_num:%d\n",active_num);
        for(int i  = 0; i < active_num;++i)
        {
            if(total_ev[i].data.fd == fd[1])
            {
                char buf[2*1024*1024];
                read(fd[1],buf,sizeof(buf));
                printf("recv\n");
            }
        }
    }
}
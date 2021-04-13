#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <thread>
using namespace std;

void t(int i)
{
    int tfd = timerfd_create(CLOCK_MONOTONIC,0);
    int epfd = epoll_create(2);
    epoll_event ev;
    ev.events = EPOLLIN|EPOLLET;
    epoll_ctl(epfd,EPOLL_CTL_ADD,tfd,&ev);
    timespec ts;
    ts.tv_sec = i;
    ts.tv_nsec = 0;
    itimerspec its1,its2;
    its1.it_value = ts;
    its1.it_interval = ts;
    timerfd_settime(tfd,0,&its1,NULL);
    while(1)
    {
        int ret = epoll_wait(epfd,&ev,2,-1);
        if(ret > 0)
        {
            char buf[10];
            read(tfd,buf,8);
            printf("%d:%d\n",i,*(int*)buf);
        }
    }
}
int main()
{
    thread t1(t,1);

    thread t2(t,2);
    t1.join();t2.join();
}
#include "heap.h"
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
Heap<Timer> time_heap;

void print(int i)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    Timer tmp(1,&tv);
    printf("now: %d s.\n",tv.tv_sec);
    while(time_heap.empty() == false)
    {
        if(time_heap.get_front() < tmp || time_heap.get_front() == tmp)
        {
            printf("move: heap: %d s.\n",time_heap.get_front().get_time());
            time_heap.pop();
        }
        else
        {
            break;
        }
    }
    if(time_heap.empty() == false)
    {
        int sec = time_heap.get_front().get_time() - tv.tv_sec;
        alarm(sec>0?sec:1);
    }else
    {
        exit(0);
    }
    
}

int main()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    tv.tv_sec += 2;
    time_heap.add({1,&tv});
    tv.tv_sec += 4;
    time_heap.add({2,&tv});
    tv.tv_sec += 2;
    time_heap.add({3,&tv});
    tv.tv_sec += 8;
    time_heap.add({4,&tv});
    tv.tv_sec += 3;
    time_heap.add({5,&tv});
    tv.tv_sec += 12;
    time_heap.add({6,&tv});
    tv.tv_sec += 9;
    time_heap.add({7,&tv});
    tv.tv_sec += 15;
    // for(int i = 0;i < 7;i++)
    // {
    //     printf("%d\n",time_heap.get_front());
    //     time_heap.pop();
    // }
    signal(SIGALRM,print); 
    alarm(1);
    while(1);
}
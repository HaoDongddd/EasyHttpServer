#include "http.h"
#include "heap.h"
#include "work.h"
#include "../log_system/log.h"
#include <signal.h>
#include <assert.h>
extern queue<int> req_que[WORK_THRD_NUM];//请求队列
extern vector<int*> pair_socks;
extern mutex que_mtx[WORK_THRD_NUM];
extern Heap<Timer,Swap> time_heap;
extern mutex m_visit_time_heap;

Log log;
int main()
{
    int sock;
    if(init_socket(sock) == false)
    {
        return -1;
    }
    for(int i = 0;i < WORK_THRD_NUM;++i)
    {
        int *tmp = new int[2];
        assert(socketpair(AF_UNIX,SOCK_STREAM,0,tmp) == 0);
        pair_socks.push_back(tmp);
    }
    create_work_threads();
    int epfd = epoll_create(200);//使用ET模式。
    struct epoll_event ev;
    ev.data.fd = sock;
    ev.events = EPOLLIN|EPOLLET;
    epoll_ctl(epfd,EPOLL_CTL_ADD,sock,&ev);
    queue<int> req_cache;
    int j = 0;
    while(1)
    {
        struct epoll_event total_ev[2000];
        int active_num = epoll_wait(epfd,total_ev,2000,-1);//在epollwait有返回的情况下，力求不阻塞主线程，从而达到高效
        //assert(active_num >= 0);  //printf("%s\n",strerror(errno));
        
        for(int i  = 0; i < active_num;++i,++j)
        {
            
            if(total_ev[i].data.fd == sock)
            {
                sockaddr tmp_addr;
                memset(&tmp_addr,0,sizeof(tmp_addr));
                socklen_t len = sizeof(tmp_addr);
                while(1)
                {
                    int ret = accept4(sock,&tmp_addr,&len,SOCK_NONBLOCK);
                    if(ret > 0)
                    {
                        req_cache.push(ret);
                        //printf("有新连接：%d\n",ret);
						
                    }
                    else if(ret == -1 && errno == EAGAIN)
                    {
                        //printf("连接接收结束\n");
                        break;
                    }
                }
                //平均分
                //if(que_mtx[j%WORK_THRD_NUM].try_lock() == true)
					que_mtx[j%WORK_THRD_NUM].lock();
                {
                    while(req_cache.empty() == false)
                    {
                        req_que[j % WORK_THRD_NUM].push(req_cache.front());
                        req_cache.pop();
                    }
                    //printf("向线程%d分发完毕\n",j%WORK_THRD_NUM);
                    que_mtx[j % WORK_THRD_NUM].unlock();
                    write(pair_socks[j % WORK_THRD_NUM][0],&i,sizeof(int));
                }
            }
        }
        
    }

}
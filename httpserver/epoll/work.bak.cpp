#include "work.h"
#include "time_heap.h"
#include "../log_system/log.h"
#include <sys/timerfd.h>
queue<int> req_que[WORK_THRD_NUM];//请求队列
vector<int*> pair_socks;
mutex que_mtx[WORK_THRD_NUM];
extern Log log;
bool init_socket(int& sock,bool non_block)
{
    if(non_block == true)
        sock = socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK,0);
    else 
        sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock == -1)
    {
        printf("%s\n",strerror(errno));
        return false;
    }
    //设置socket复用
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("192.168.179.1");
    addr.sin_port = htons(10000);
    if(bind(sock,(sockaddr*)&addr,(socklen_t)sizeof(addr)) < 0 )
    {
        printf("%s\n",strerror(errno));
        return false;
    }
    if(listen(sock,10) < 0 )
    {
        printf("%s\n",strerror(errno));
        return false;
    }
    return true;
}
/*
对于时间堆中的每一个节点：
    有四个属性用于标记     
        del_flag---->该节点是否删除  true-->删除  false-->不删除
        delay---->这个连接延长多少秒，为0则不延长
        passive_close---->由于对端关闭而导致的被动关闭。
        active_close---->由于对端超时而导致本端主动关闭。

        被动关闭由task自己erase，主动关闭由timer_heap来erase task的对应元素。
        task只负责Myhttp的delete，其他资源由timer_heap来delete。


    逻辑是：访问堆顶，若del_flag标记存在，则删除相关资源，并且从task中删除对应节点。
    若del_flag和delay同时存在，那也是删除节点。

void check_time_heap()
{
    struct timeval current_time;
    gettimeofday(&current_time,NULL);
    unique_lock<mutex> mtx(m_visit_time_heap);
    while(time_heap.empty() == false)
    {
        Timer front(move(time_heap.get_front()));
        unordered_map<int,Myhttp*> task = *front.task;
        if(*front.del_flag == true )//被动删除，对端关闭连接，heap只要pop然后释放自己的资源即可。
        {
            time_heap.pop();
        }
        else if(current_time.tv_sec > front.time.tv_sec)//主动删除，该连接超时
        {
            //如果有delay还没有计算，那么就不算超时
            if(*front.delay != 0)
            {
                front.time.tv_sec += (*front.delay);
                *front.delay = 0;

                Timer t(move(front));
                time_heap.pop();
                time_heap.add(move(t));
            }
            else
            {
                printf("move node:%d\n",front.socket_id);
                auto it = task.find(front.socket_id);
                assert(it != task.end());
                delete (it->second);
                task.erase(it);

                struct epoll_event ev;
                ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
                assert(epoll_ctl(front.epfd,EPOLL_CTL_DEL,front.socket_id,&ev) == 0);

                time_heap.pop();
            }
        }
        else//该节点不需要删除
        {
            front.time.tv_sec += (*front.delay);
            *front.delay = 0;
            Timer t(move(front));
            time_heap.pop();
            time_heap.add(move(t));
            return;
        }
    }
}
*/

//check的时候检查table，看当前堆顶是否有delay，以及是否有效，循环向下check其他节点，循环终止条件是  ！（节点时间  《  当前时间）。
void check_time_heap(priority_queue<Heap_node*,vector<Heap_node*>,compare>& time_heap,unordered_map<int,Heap_node*> flag_table,unordered_map<int,Myhttp*>& task,int epfd,timeval cur_time)
{
    while(time_heap.empty() == false && time_heap.top()->time.tv_sec < cur_time.tv_sec)
    {
        Heap_node* top = time_heap.top();
        if(top->client_fd != -1)//有效
        {
            if(top->time.tv_sec <= cur_time.tv_sec)//该节点超时了
            {
                time_heap.pop();

                auto it = task.find(top->client_fd);
                assert(it != task.end());
                delete it->second;
                task.erase(it);

                epoll_event ev;
                ev.events = EPOLLIN|EPOLLOUT;
                epoll_ctl(epfd,EPOLL_CTL_DEL,top->client_fd,&ev);

                auto it2 = flag_table.find(top->client_fd);
                assert(it2 != flag_table.end());
                delete it2->second;
                flag_table.erase(it2);


                close(top->client_fd);
                char log_buf[50];
                sprintf(log_buf,"node timeout.socket of node:%d",top->client_fd);
                log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
            }
            else//该节点没有超时，直接返回
            {
                return;
            }
        }
        else//该节点无效
        {
            time_heap.pop();
            char log_buf[50];
            sprintf(log_buf,"invalid node:%d",top->client_fd);
            log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
        }
    }
}
void work_thread(int id)
{
    unordered_map<int,Myhttp*> task;
    unordered_map<int,Heap_node*> flag_table;//int--->socket
    priority_queue<Heap_node*,vector<Heap_node*>,compare> time_heap;
    int epfd = epoll_create(20000);
    epoll_event* evt = (epoll_event*)malloc(sizeof(epoll_event)*CONN_PER_THRD);
    memset(evt,0,sizeof(epoll_event)*CONN_PER_THRD);
    //监听socketpair
    struct epoll_event ev_for_pair;
    ev_for_pair.data.fd = pair_socks[id][1];
    ev_for_pair.events = EPOLLIN|EPOLLET;
    epoll_ctl(epfd,EPOLLIN,pair_socks[id][1],&ev_for_pair);
    //监听timerfd
    struct epoll_event tev;
    int tfd = timerfd_create(CLOCK_MONOTONIC,0);
    tev.data.fd = tfd;
    tev.events = EPOLLIN|EPOLLET;
    assert(epoll_ctl(epfd,EPOLL_CTL_ADD,tfd,&tev) == 0);
    //启动timerfd
    itimerspec itime;
    timespec ti;
    ti.tv_sec = 3;
    ti.tv_nsec = 0;
    itime.it_interval = ti;
    itime.it_value = ti;
    timerfd_settime(tfd,0,&itime,NULL);

    while(1)
    {
		memset(evt,0,sizeof(epoll_event)*CONN_PER_THRD);
        int ret = epoll_wait(epfd,evt,CONN_PER_THRD,-1);
        if(ret > 0)
        {
            for(int i = 0;i < ret;++i)
            {
                if(evt[i].data.fd == pair_socks[id][1])//recv notification from main thread
                {
                    int tmp_read;
                    read(pair_socks[id][1],&tmp_read,sizeof(int));
                    unique_lock<mutex> que_m(que_mtx[id]);
                    //que_mtx[id].lock();
                    while(req_que[id].empty() == false)
                    {
                        //log.append(Log::WARN,WARN_S,"new request.",12,__FILE__,__LINE__);
                        int cur_fd = req_que[id].front();

                        {
                            char log_buf[50];
                            sprintf(log_buf,"有新连接：%d",cur_fd);
                            log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                        }

                        req_que[id].pop();


                        struct timeval current_time;
                        gettimeofday(&current_time,NULL);
                        //加入定时器
                        current_time.tv_sec += 5;
                        Heap_node *tmp_node = new Heap_node(cur_fd,current_time);
                        time_heap.push(tmp_node);
                        

                        //epoll_add
                        epoll_event tmp_ev;
                        tmp_ev.data.fd = cur_fd;
                        tmp_ev.events = EPOLLIN|EPOLLET;
                        assert(epoll_ctl(epfd,EPOLL_CTL_ADD,cur_fd,&tmp_ev) == 0);

                        //加入task
                        Myhttp* http_conn = new Myhttp();
                        task.insert({cur_fd,http_conn});

                        //加入table
                        flag f;
                        assert(flag_table.insert({cur_fd,tmp_node}).second) ;
                    }
                    //printf("线程%d从队列读取完毕\n",id);
                    //que_mtx[id].unlock();
                }
                else if(evt[i].data.fd == tfd)
                {
                    char tmp_buf[8];
                    read(evt[i].data.fd,tmp_buf,8);
                    struct timeval current_time;
                    gettimeofday(&current_time,NULL);
                    check_time_heap(time_heap,flag_table,task,epfd,current_time);
                }
                else 
                {
                    auto flag_it = flag_table.find(evt[i].data.fd);
                    assert(flag_it != flag_table.end());

                    if(evt[i].events == EPOLLIN)
                    {
                        int cur_fd = evt[i].data.fd;
                        Myhttp* tmp_http;
                        auto it = task.find(cur_fd);
                        assert(it != task.end());
                        tmp_http = it->second;
                        tmp_http->parse_packet(cur_fd,id);
                        //printf("线程%d:%d正在解析中\n",id,cur_fd);
                        if(tmp_http->is_keep_alive() == false)
                        {
                            if(tmp_http->is_parse_ended())
                            {   
                                //printf("线程%d:%d准备发送\n",id,cur_fd);
                                epoll_event tmpev;
                                tmpev.data.fd = cur_fd;
                                tmpev.events = EPOLLOUT|EPOLLET;
                                assert(epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmpev) == 0);
                            }
                        }
                        else//打开了keep-alive
                        {
                            auto it = task.find(cur_fd);
                            assert(it != task.end());
                            if(tmp_http->is_closed())
                            {
                                //printf();
								char log_buf[50];
								sprintf(log_buf,"线程%d:%d已关闭",id,cur_fd);
								log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                                epoll_event tmpev;
                                tmpev.events = EPOLLIN|EPOLLOUT|EPOLLET;
                                assert(epoll_ctl(epfd,EPOLL_CTL_DEL,cur_fd,&tmpev) == 0);
                                
                                task.erase(it);
                                //将该节点标记为无效
                                flag_it->second->client_fd = -1;
                                delete flag_it->second;
                                flag_table.erase(flag_it);

                                shutdown(cur_fd,SHUT_RDWR);
                                close(cur_fd);

                                delete tmp_http;

                            }
                            else if(tmp_http->is_parse_ended())
                            {   
                                
                                //printf("线程%d:%d准备发送\n",id,cur_fd);
                                tmp_http->reset_parse_flag();
                                epoll_event tmpev;
                                tmpev.data.fd = cur_fd;
                                tmpev.events = EPOLLIN | EPOLLOUT|EPOLLET;
                                assert(epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmpev) == 0);
                            }
                            //延长时间
                            flag_it->second->time.tv_sec += 5;
                        }
                    }
                    if(evt[i].events == EPOLLOUT)
                    {
                        int cur_fd = evt[i].data.fd;
                        Myhttp* tmp_http = task.find(cur_fd)->second;
                        if(tmp_http->is_keep_alive() == false)//没有开启keep-alive
                        {
                            if(tmp_http->is_respon_ended())
                            {
                                epoll_event tmpev;
                                tmpev.data.fd = cur_fd;
                                tmpev.events = EPOLLIN|EPOLLOUT;
                                assert(epoll_ctl(epfd,EPOLL_CTL_DEL,cur_fd,&tmpev) == 0);
                                tmp_http->reset_responese_flag();//为了释放空间
                                shutdown(cur_fd,SHUT_RDWR);
                                close(cur_fd);
                                delete tmp_http;
                                assert(task.erase(cur_fd) != 0);

                                char log_buf[50];
                                sprintf(log_buf,"线程%d:%d发送完成\n",id,cur_fd);
                                log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);

                                continue;
                            }
                            else
                            {
                                tmp_http->send_responese(cur_fd);
                                epoll_event tmpev;
                                tmpev.data.fd = cur_fd;
                                tmpev.events = EPOLLOUT|EPOLLET;
                                epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmpev);
                            }
                        }
                        else //开启了keep-alive
                        {
                            if(tmp_http->has_send_tasks())
                            {
                                tmp_http->send_responese_for_keepalive(cur_fd);
                                //tmp->reset_responese_flag();
                                epoll_event tmpev;
                                tmpev.data.fd = cur_fd;
                                tmpev.events = EPOLLIN|EPOLLOUT|EPOLLET;
                                epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmpev);
                                //延时
                                flag_it->second->time.tv_sec += 5;
                            }
                            else
                            {
                                epoll_event tmpev;
                                tmpev.data.fd = cur_fd;
                                tmpev.events = EPOLLIN|EPOLLET;
                                epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmpev);
                            }
                        }
                    }
                }
            }
        }
        
    }
}


void create_work_threads()
{
    for(int i = 0;i < WORK_THRD_NUM;++i)
    {
        thread t(work_thread,i);
        t.detach();
    }
}
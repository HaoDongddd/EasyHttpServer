#include "work2.h"
#include "time_heap.h"
#include "../log_system/log.h"
#include <sys/timerfd.h>
queue<int> req_que[WORK_THRD_NUM];//请求队列
vector<int*> pair_socks;
mutex que_mtx[WORK_THRD_NUM];
extern Log log;
void BlockSigno(int signo) { /* 阻塞掉某个信号 */
    sigset_t signal_mask;
    sigemptyset(&signal_mask); /* 初始化信号集,并清除signal_mask中的所有信号 */
    sigaddset(&signal_mask, signo); /* 将signo添加到信号集中 */
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL); /* 这个进程屏蔽掉signo信号 */
}

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
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("192.168.179.1");
    addr.sin_port = htons(10000);
    if(bind(sock,(sockaddr*)&addr,(socklen_t)sizeof(addr)) < 0 )
    {
        printf("%s\n",strerror(errno));
        return false;
    }
    if(listen(sock,64) < 0 )
    {
        printf("%s\n",strerror(errno));
        return false;
    }
    return true;
}


//check的时候检查table，看当前堆顶是否有delay，以及是否有效，循环向下check其他节点，循环终止条件是  ！（节点时间  《  当前时间）。
void check_time_heap(priority_queue<Heap_node*,vector<Heap_node*>,compare>& time_heap,unordered_map<int,Heap_node*>& flag_table,unordered_map<int,Myhttp*>& task,int epfd,timeval cur_time)
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
                char log_buf[50];
                sprintf(log_buf,"task erase:%d",top->client_fd);
                log.append(Log::ERROR,ERROR_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                delete it->second;
                task.erase(it);

                epoll_event ev;
                ev.events = EPOLLIN|EPOLLOUT;
                int r = epoll_ctl(epfd,EPOLL_CTL_DEL,top->client_fd,&ev);
                assert(r == 0);
                assert(top->client_fd != 0);
                close(top->client_fd);
                
                sprintf(log_buf,"close:%d",top->client_fd);
                log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);

                auto it2 = flag_table.find(top->client_fd);
                if(it2 != flag_table.end())
                {
                    sprintf(log_buf,"flag_table find :%d",top->client_fd);
                    //log.append(Log::DEBUG,DEBUG_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                    delete it2->second;
                }
                else  
                {
                    sprintf(log_buf,"flag_table not find. %d not exist",top->client_fd);
                    //log.append(Log::ERROR,ERROR_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                }
                flag_table.erase(it2);
                
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
            //log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
        }
    }
}
void work_thread(int id)
{
    BlockSigno(SIGPIPE);
    unordered_map<int,Myhttp*> task;
    unordered_map<int,Heap_node*> flag_table;//int--->socket
    priority_queue<Heap_node*,vector<Heap_node*>,compare> time_heap;
    int epfd = epoll_create(2000);
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

    int listenfd;
    if(init_socket(listenfd) == false)
    {
        exit(-1);
    }
    tev.data.fd = listenfd;
    tev.events = EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&tev);
    while(1)
    {
        epoll_event evts[2000];
        int ret = epoll_wait(epfd,evts,2000,-1);
        if(ret > 0)
        {
            for(int i = 0;i < ret;++i)
            {
                int cur_fd = evts[i].data.fd;
                if(cur_fd == listenfd)
                {
                    while(1)
                    {
                        struct sockaddr addr;
                        socklen_t addr_len = sizeof(addr);
                        int client_fd = accept(listenfd,&addr,&addr_len);
						
                        if(client_fd < 0)
                        {
                            if(errno == EAGAIN)
                                ;//log<< error
                            break;
                        }
                        else if(client_fd == 0)
                        {
                            //log<< error
                            break;
                        }
                        else 
                        {
							set_nonblock(client_fd);
                            char log_buf[50];
                            //sprintf(log_buf,"thread %d accept new client：%d",id,client_fd);
                            //log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
						
                            struct timeval current_time;
                            gettimeofday(&current_time,NULL);
                            //加入定时器
                            current_time.tv_sec += KEEP_ALIVE_TIMEOUT;
                            Heap_node *tmp_node = new Heap_node(client_fd,current_time);
                            time_heap.push(tmp_node);


                            //epoll_add
                            epoll_event tmp_ev;
                            tmp_ev.data.fd = client_fd;
                            tmp_ev.events = EPOLLIN|EPOLLET;
                            int r = epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&tmp_ev);
                            if(r != 0)
                            {
                                sprintf(log_buf,"error：%s",strerror(errno));
                                log.append(Log::ERROR,ERROR_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                            }
							//assert(r == 0);
                            //加入task
                            Myhttp* http_conn = new Myhttp();
                            assert(task.insert({client_fd,http_conn}).second == true);
                            sprintf(log_buf,"task insert success :%d",client_fd);
                            //log.append(Log::DEBUG,DEBUG_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                            //加入table
                            flag f;
							//assert(flag_table.insert({client_fd,tmp_node}).second);
                            if(flag_table.insert({client_fd,tmp_node}).second == false)
							{
                                sprintf(log_buf,"flag_table insert error：%d",client_fd);
                                //log.append(Log::ERROR,ERROR_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                            }
                            else
                            {
                                sprintf(log_buf,"flag_table insert success：%d",client_fd);
                                //log.append(Log::DEBUG,DEBUG_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                            }
                        }
                    }
                }
                else if(cur_fd == tfd)
                {
                    char tmp_buf[8];
                    read(tfd,tmp_buf,8);
                    struct timeval current_time;
                    gettimeofday(&current_time,NULL);
                    check_time_heap(time_heap,flag_table,task,epfd,current_time);
                }
                else //处理事件
                {
                    epoll_event tmp_event;
                    if(evts[i].events == EPOLLIN)
                    {
						auto flag_it = flag_table.find(evts[i].data.fd);
						assert(flag_it != flag_table.end());
						if(flag_it == flag_table.end())
							sleep(1);
						auto task_it = task.find(cur_fd);
						assert(task_it != task.end());
						if(task_it == task.end())
							sleep(1);
						Myhttp* http = task_it->second;
					
					
                        http->parse_packet(cur_fd);
                        if(http->is_closed())
                        {
                            flag_it->second->client_fd = -1;
                                                    
                            flag_table.erase(flag_it);
                            
                            tmp_event.events = EPOLLIN|EPOLLOUT;
                            int r = epoll_ctl(epfd,EPOLL_CTL_DEL,cur_fd,&tmp_event);
							assert(r == 0);
							delete task_it->second;
							assert(cur_fd != 0);
                            close(task_it->first);
                            char log_buf[50];
                            sprintf(log_buf,"thread %d task erase : %d",id,task_it->first);
                            //log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);    
                            task.erase(task_it);
                            continue;
                        }
                        else if(http->is_parse_over())
                        {
                            if(http->is_keep_alive())//是长连接
                            {
                                tmp_event.data.fd = cur_fd;
                                tmp_event.events = EPOLLOUT;//LT
                                int r = epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmp_event);
                                assert(r == 0);
                                if(r != 0)
                                {
                                    printf("%s\n",strerror(errno));
                                }
                                http->reset_parse_flag();
                            }
                            else//是短连接
                            {
                                tmp_event.events = EPOLLOUT;//LT
                                epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmp_event);
                            }

                            flag_it->second->time.tv_sec += 5;
                        }
                        else //包还未分析完
                        {
                            flag_it->second->time.tv_sec += 5;
                        }
                    }
                    if(evts[i].events == EPOLLOUT)
                    {//防止迭代器失效
						auto flag_it = flag_table.find(evts[i].data.fd);
						//assert(flag_it != flag_table.end());
						if(flag_it == flag_table.end())
							continue;
						auto task_it = task.find(cur_fd);
						//assert(task_it != task.end());
						if(task_it == task.end())
							continue;
						Myhttp* http = task_it->second;
						
                        if(http->is_send_over())
                        {
                            if(http->is_keep_alive())
                            {
                                http->reset_send_flag();
                                tmp_event.data.fd = cur_fd;
                                tmp_event.events = EPOLLIN|EPOLLET;
                                int r = epoll_ctl(epfd,EPOLL_CTL_MOD,cur_fd,&tmp_event);
                                assert(r == 0);
                            }
                            else
                            {
                                delete task_it->second;
                                task.erase(task_it);
								char log_buf[50];
                                sprintf(log_buf,"thread %d task erase : %d",id,task_it->first);
                                log.append(Log::ERROR,ERROR_S,log_buf,strlen(log_buf),__FILE__,__LINE__); 
                                tmp_event.events = EPOLLOUT|EPOLLIN;
                                epoll_ctl(epfd,EPOLL_CTL_DEL,cur_fd,&tmp_event);

                                flag_it->second->client_fd = -1;
								assert(cur_fd != 0);
                                close(cur_fd);
                                flag_table.erase(flag_it);
                            }
                        }
                        else
                        {
                            http->send_all_data(cur_fd);
                        }
                    }
                }
            }
        }
    }
}

bool set_nonblock(int fd)
{
	int flags;
if(flags = fcntl(fd, F_GETFL, 0) < 0)
{
	perror("fcntl");
    return false;
}
flags |= O_NONBLOCK;
if(fcntl(fd, F_SETFL, flags) < 0)
{
    perror("fcntl");
    return false;
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
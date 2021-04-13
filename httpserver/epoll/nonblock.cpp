#include "http.h"
#include <fcntl.h>

bool init_socket(int& sock)
{
    sock = socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);
    if(sock == -1)
    {
        printf("%s\n",strerror(errno));
        return false;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("172.16.47.150");
    addr.sin_port = htons(998);
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
}
int main()
{
    int sock;
    cout << init_socket(sock);

    char buf[2];
    sockaddr tmp_addr;
    memset(&tmp_addr,0,sizeof(tmp_addr));
    socklen_t len = sizeof(tmp_addr);

    int epfd = epoll_create(200);
    struct epoll_event ev;
    ev.data.fd = sock;
    ev.events = EPOLLIN;
    struct epoll_event e[20];
    epoll_ctl(epfd,EPOLL_CTL_ADD,sock,&ev);
    while(1)
    {
        int i = epoll_wait(epfd,e,20,0);
        if(i > 0)break;
    }
    int ret = accept4(sock,&tmp_addr,&len,SOCK_NONBLOCK);//经测试，无论之前监听在阻塞的还是非阻塞的socket上，accept正常返回的socket，在后续read中体现的都是阻塞的，因此使用accept4.
    cout << "s\n";
    for(;;)
    {
        int r = read(ret,buf,sizeof(buf));
        if(r < 0)
        {
            if(errno == EINTR || errno == EAGAIN)
            {
                ;
            }
            else 
            {
                printf("%s\n",strerror(errno));
                break;
            }
        }
        else if(r == 0)//连接关闭
        {
            printf("lianjieguanbi\n");
            break;
        }
        else 
        {
            printf("ret: %d\n",r);
        }
    }
}
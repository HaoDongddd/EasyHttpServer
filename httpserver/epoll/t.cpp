#include "http.h"
#include <assert.h>
#include <queue>
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
    return true;
}

int main()
{
    
    string a;
    a=("\r\n");
    cout << a.length();
}
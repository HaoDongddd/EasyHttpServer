#ifndef TIME_HEAP_H
#define TIME_HEAP_H
#include <queue>
#include <sys/time.h>

class Heap_node
{
public:
    int client_fd;
    timeval time;
    Heap_node(int fd,timeval t):client_fd(fd),time(t){}
    bool operator>(const Heap_node& a)
    {
        if(this->time.tv_sec > a.time.tv_sec)
            return true;
        else 
            return false;
    }
};
class compare
{
public:
    bool operator()(Heap_node* a,Heap_node* b)
    {
        if((*a) > (*b))
            return true;
        else 
            return false;
    }

};



#endif
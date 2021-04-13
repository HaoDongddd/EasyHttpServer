#ifndef HEAP_H
#define HEAP_H
#include <map>
#include <vector>
#include <unordered_map>
#include <assert.h>
#include "http.h"
using namespace std;

template<class T,class Swap>
class Heap
{//对于节点 i ，其父节点为(i-1)/2   左儿子为2*i+1   右儿子为2*i+2
    int count;
    int index_of_last_leaf;
    vector<T> heap;
public:
    Heap():count(0),index_of_last_leaf(0) {}
    void add(T t)
    {
        heap.push_back(move(t));
        ++count;
        index_of_last_leaf = count - 1;
        int i = index_of_last_leaf;
        while(i != 0)
        {
            if(heap[i] < heap[(i-1)/2])
            {
                //Swap()
                Swap()(heap[i],heap[(i-1)/2]);
                // T tmp = heap[i];
                // heap[i] = heap[(i-1)/2];
                // heap[(i-1)/2] = tmp;
                i = (i-1)/2;
            }
            else 
            {
                break;
            }
        }
    }
    void pop()
    {
        if(count == 1)
        {
            heap.erase(heap.begin());
            count = 0;
        }
        else if(count > 1)
        {
            heap[0] = heap[index_of_last_leaf];
            assert(heap.erase(heap.begin() + index_of_last_leaf) != heap.end());
            --count;
            --index_of_last_leaf;
            int i = 0;
            while(i <= index_of_last_leaf)
            {
                int lson = 2*i+1;
                int rson = 2*i+2;
                if(lson <= index_of_last_leaf && rson <= index_of_last_leaf)
                {
                    if(heap[lson] < heap[rson] || heap[lson] == heap[rson])
                    {
                        if(heap[lson] < heap[i])
                        {
                            //Swap()
                            Swap()(heap[lson],heap[i]);
                            // T tmp = heap[lson];
                            // heap[lson] = heap[i];
                            // heap[i] = tmp;
                            i = lson;
                        }
                    }
                    else
                    {
                        if(heap[rson] < heap[i])
                        {
                            //Swap()
                            Swap()(heap[rson],heap[i]);
                            // T tmp = heap[rson];
                            // heap[rson] = heap[i];
                            // heap[i] = tmp;
                            i = rson;
                        }
                    }
                }
                else if(lson <= index_of_last_leaf)
                {
                    if(heap[lson] < heap[i])
                    {
                        //Swap()
                        Swap()(heap[lson],heap[i]);
                        // T tmp = heap[lson];
                        // heap[lson] = heap[i];
                        // heap[i] = tmp;
                    }
                    break;
                }
                else
                {
                    break;
                }
                
            }
        }
    }
    T& get_front()
    {
        return heap[0];
    }
    int length()
    {
        return count;
    }
    bool empty()
    {
        return heap.empty();
    }
};


class Timer
{
public:
    int socket_id;
    struct timeval time;//到期时间
    bool *del_flag;//若为true，则该节点无效
    int *delay;//延时
    int epfd;
    unordered_map<int,Myhttp*> *task;
    Timer(int sock,struct timeval time):socket_id(sock),time(time)
    {
        del_flag = new bool;
        *del_flag = false;
        delay = new int;
        *delay = 0;
        task = nullptr;
    }
    ~Timer()
    {
        if(del_flag != nullptr)
        delete del_flag;
        if(delay != nullptr)
        delete delay;
    }

    /*
    // 这里使用了移动构造 ， 目的是让  (修改time-->生成临时timer--->pop--->add临时time)    （这一操作调整那些未超时但是超时时间改变了的节点的位置）  移动拷贝可以在生成临时timer之后
    //使这个临时timer拥有和原来的timer一样得变量地址和值
     //地址很重要   这是工作线程和time_heap的唯一联系。
    */
    Timer(Timer&& t)
    {                      
        del_flag = t.del_flag;    
        t.del_flag = nullptr;  
        delay = t.delay;
        t.delay = nullptr;
        task = t.task;
    }
    Timer& operator=(Timer& t)
    {
        if(this != &t)
        {
            del_flag = t.del_flag;                                                                                                                               
            *del_flag = *t.del_flag;  
            t.del_flag = nullptr; 

            delay = t.delay;                                                                                                                                       
            *delay = *t.delay;
            t.delay = nullptr;
        }
        return *this;
    }
    Timer& operator=(Timer&& t)
    {
        if(this != &t)
        {
            del_flag = t.del_flag;                                                                                                                               
            *del_flag = *t.del_flag;  
            t.del_flag = nullptr; 

            delay = t.delay;                                                                                                                                       
            *delay = *t.delay;
            t.delay = nullptr;
        }
        return *this;
    }
    Timer(const Timer& t) = delete;
    bool operator>(Timer& tmp)
    {
        if(this->time.tv_sec > tmp.time.tv_sec)
            return true;
        else 
            return false;
    }
    bool operator<(Timer& tmp)
    {
        if(this->time.tv_sec < tmp.time.tv_sec)
            return true;
        else 
            return false;
    }
    bool operator==(Timer& tmp)
    {
        if(this->time.tv_sec == tmp.time.tv_sec && this->socket_id == tmp.socket_id)
            return true;
        else 
            return false;
    }
    
};

class Swap
{
public:
    void operator()(Timer& a,Timer& b)
    {
        Timer tmp(move(a));
        a = move(b);
        b = move(tmp);
    }
};
#endif
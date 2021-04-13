#ifndef WORK_H
#define WORK_H

#include "http.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <map>
#include <cstring>
#include <utility>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include<sys/time.h>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <time.h>
#include <sys/eventfd.h>
#define WORK_THRD_NUM 5
#define TIME_OUT 500
#define CONN_PER_THRD 10000
#define EPOLL_TIME_OUT 20//ms
#define KEEP_ALIVE_TIMEOUT 20//s
using namespace std;

struct flag
{
    int delay;
    bool valid;
    flag():delay(0),valid(true){}
};
bool init_socket(int& sock,bool non_block = true);

void check_time_heap();

void work_thread(int id);

void create_work_threads();

#endif
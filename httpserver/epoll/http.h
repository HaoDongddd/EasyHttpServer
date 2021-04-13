#ifndef HTTP_H
#define HTTP_H

#include <iostream>
#include <string>
#include <map>
#include <queue>
#include <cstring>
#include <utility>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <string>
#define WWWROOT "./index"
using namespace std;
class Myhttp
{
    typedef int LINE_STATUS;
    int http_status;
    int line_status;
    int request_line_status;
    int header_status;
    int send_status;
    int send_task_count;//发送任务计数
    bool f_responese;
    bool request_err;
    bool closed;
    bool keep_alive;//标记
    bool init_respon_once;//非keep-alive只init一次responese的标志
    volatile int parse_index;
    map<string,string> headers;
    queue<string> url_que;//长连接下的url队列
    char* buf;
    char* file;
    char* send_buf;
    long file_size;
    long buf_len;
    long already_send;
    volatile int recv_len;
    int send_len;
    enum{
        rline_start,
        rline_method,
        rline_method_sp,
        rline_url,
        rline_url_sp,
        rline_version,
        rline_almost_done,
        header_start,
        header_key,
        header_colon,
        header_colon_sp,
        header_value,
        header_almost_done,
        header_blank,
        CHECK_REQUEST_LINE,
        CHECK_HEADER,
        CHECK_BODY,
        LINE_OPEN,
        LINE_DONE,
        REQUEST_ERROR,
        LINE_NEXT,
        RLINE_DONE,
        HEADER_DONE,
        REQUEST_DONE,
        BAD_REQUEST,
        CONN_CLOSED,
    };
    enum{
        start_send,//开始发送
        sending,//发送中
        send_done//发送完成
    };//某个responese的send状态
    string method,url,version;
    string key,value;
    string url_for_send;
    LINE_STATUS parse_request_line();
    LINE_STATUS parse_header();
    LINE_STATUS parse_body();
    LINE_STATUS get_line(int);
    void init_responese();
    bool get_file();
    
public:
    
    Myhttp():http_status(CHECK_REQUEST_LINE),line_status(LINE_OPEN),f_responese(false),parse_index(0),recv_len(0),buf_len(4096),request_line_status(rline_start),header_status(header_start)
    {
        send_task_count = 0;
        keep_alive = true;
        init_respon_once = false;
        file_size = 0;
        send_len = 0;
        already_send = 0;
        closed = false;
        //printf("im new\n");
        send_status = start_send;
        buf = new char[buf_len];
        send_buf = NULL;
    }
    ~Myhttp()
    {
        if(send_buf != NULL)
            delete []send_buf;
        //printf("i go\n");
        delete []buf;
    }
    bool is_parse_ended();
    bool is_respon_ended();
    bool is_closed();
    bool is_keep_alive();
    bool has_send_tasks();
    void parse_packet(int sock ,int id);
    void send_responese(int);
    void send_responese_for_keepalive(int);
    void reset_parse_flag();//keep-alive打开时才需要清除标志位
    void reset_responese_flag();
    
};

#endif

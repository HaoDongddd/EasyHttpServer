#ifndef HTTP2_H
#define HTTP2_H

#include <iostream>
#include <string>
#include <unordered_map>
#include <queue>
#include <cstring>
#include <utility>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h> 
#include <string>
#define WWWROOT "./index"
#define BUFLEN 4096
class Myhttp
{
public:
    typedef int PARSE_RET_STATUS;//大写的用作返回值
    void send_all_data(int clientfd);
    void parse_packet(int clientfd);
    void reset_parse_flag();//针对keep alive需要复用，重置标记
    void reset_send_flag();
    bool is_closed();
    bool is_keep_alive();
    bool is_send_over();
    bool is_parse_over();
    bool need_to_send();
    Myhttp()
    {
        recv_buf = new char[BUFLEN];
        send_buf = new char[BUFLEN];
    }
private:
    int get_buf(int clientfd);
    void init_response_header();
    
    PARSE_RET_STATUS parse_request_status_line();
    PARSE_RET_STATUS parse_request_headers();

    off_t* send_file_offset = nullptr;

    bool keep_alive = true;//是否开启keepalive
    bool close_ = false;//对端是否关闭
    bool already_send_response_header = false;//header 和 body是分开发的
    bool file_already_open = false;
    bool responese_already_init = false;
    
    int file_fd;
    int parse_index = 0;
    int already_recv_len = 0;
    int recv_buf_len = 4096;
    int responese_header_len = 0;//响应头的长度，不包括body
    int this_package_status = CHECK_REQUEST_STATUS_LINE;//check status_line?  check header?
    int parse_status = parsing;//parsing?  parse end?
    int status_line_parse_status = rline_start;
    int headers_parse_status = header_start;
    int responese_status = normal_responese;//200？ 404？
    int send_task_count = 0;//要发几个包

    char* recv_buf = nullptr;
    char* send_buf = nullptr;

    long need_send_len = 0;
    long already_send_len = 0;
    long file_size;

    std::string method;
    std::string http_version;
    std::string url;
    std::string file_name;
    std::string key,value;
    std::string responese_header;
    std::unordered_map<std::string,std::string> headers;
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

        CHECK_REQUEST_STATUS_LINE,
        CHECK_REQUEST_HEADERS,
        CHECK_REQUEST_BODY,

        OK,
        MORE,
        ERROR,

        parsing,
        parse_end,

        bad_respone,
        normal_responese
    };
};


#endif
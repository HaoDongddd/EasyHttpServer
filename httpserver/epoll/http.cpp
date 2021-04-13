#include "http.h"
#include "/home/ubuntu/aliyun_backup/my_programs/c_c++/log_system/log.h"
#include <assert.h>
extern Log log;
Myhttp::LINE_STATUS Myhttp::get_line(int sock)
{
    while(1)
    {
    int ret = read(sock,buf+recv_len,buf_len-recv_len);
    if(ret < 0)
    {
        if(errno == EINTR || errno == EAGAIN)
            return LINE_NEXT;
        else 
        {
            char log_buf[50];
            sprintf(log_buf,"%s",strerror(errno));
            log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
            return REQUEST_ERROR;
        }
    }
    else if(ret == 0)//连接关闭
    {
        return CONN_CLOSED;
    }
    else 
    {
        recv_len += ret;
    }
    }
}

void Myhttp::parse_packet(int sock,int id)//一次读完，一次分析完已收到的数据
{
    int ret;
    line_status=get_line(sock);
    if(line_status == REQUEST_ERROR)
    {

        return ;
    }
    if(line_status == CONN_CLOSED )
    {
        closed = true;
        return ;
    }
    //assert(parse_index < recv_len);
    while(parse_index < recv_len)
    {    
        
        switch (http_status)
        {
        case CHECK_REQUEST_LINE:
            ret = parse_request_line();
            if(ret == RLINE_DONE)
            {
                http_status = CHECK_HEADER;
                //cout << sock << method << url << version << endl;
            }
            else if(ret == REQUEST_ERROR)
            {
                request_err = true;
                return ;
            }
            break;
        case CHECK_HEADER:
            ret = parse_header();
            if(ret == HEADER_DONE)
            {
                http_status = REQUEST_DONE;
                char log_buf[50];
                sprintf(log_buf,"thread:%d %d 解析完成 结果:%s",id,sock,url.c_str());
                log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
                //向客户端返回数据
                // for(auto i = headers.begin();i != headers.end();++i)
                // {
                //     cout << i->first <<":" << " " << i->second <<endl;
                // }
                
                return ; 
            }
            else if(ret == REQUEST_ERROR)
            {
                request_err = true;
                return ;
            }
            break;
        case CHECK_BODY:;

            break;
        }
    }
}

Myhttp::LINE_STATUS Myhttp::parse_request_line()
{
    for(;parse_index < recv_len;++parse_index)
    {
        char tmp = buf[parse_index];
        switch(request_line_status)
        {
            case rline_start:
                if( !isalpha(tmp) )
                    return REQUEST_ERROR;
                else
                    {
                        request_line_status = rline_method;
                        method.push_back(tmp);
                    }
            break;
            case rline_method:
                if(tmp == ' ' || tmp == '\t')
                {
                    if(method.length() < 2)
                        return REQUEST_ERROR;
                    request_line_status = rline_method_sp;
                }
                else if(  isalpha(tmp) )
                    method.push_back(tmp);
                else
                    return REQUEST_ERROR;
            break;
            case rline_method_sp:
                if(tmp != ' ' && tmp != '\t')    
                {
                    if(tmp == '/')
                    {
                        url.push_back(tmp);
                        request_line_status = rline_url;
                    }
                    else
                        return REQUEST_ERROR;
                }
            break;
            case rline_url:
                if(tmp == '/' || isalpha(tmp) || tmp == '.' || isdigit(tmp) || tmp == '_' || tmp == '-')
                    url.push_back(tmp);
                else if(tmp == '\t' || tmp == ' ')
                {
                    request_line_status = rline_url_sp;
                    //若keep-alive选项开启，把url入队，并且clear当前url
                    // if(keep_alive == true)
                    // {
                    //     url_que.push(url);
                    //     url_for_send = url;
                    //     url.clear();
                    // }
                }
                else 
                    return REQUEST_ERROR;
            break;
            case rline_url_sp:
                if( tmp != ' ' || tmp != '\t')
                {
                    if(isalpha(tmp))
                    {
                        request_line_status = rline_version;
                        version.push_back(tmp);
                    }    
                    else 
                        return REQUEST_ERROR;
                }
            break;
            case rline_version:
                if(tmp == '.' || tmp == '/' || isdigit(tmp) || isalpha(tmp))
                    version.push_back(tmp);
                else if(tmp == '\r')
                    request_line_status = rline_almost_done;
                else 
                    return REQUEST_ERROR;
            break;
            case rline_almost_done:
                if(tmp == '\n')
                {
                    ++parse_index;
                    return RLINE_DONE;
                }
                else 
                    return REQUEST_ERROR;
            break;
        }
    }
    //return LINE_OPEN;
}

Myhttp::LINE_STATUS Myhttp::parse_header()
{
    //要检测空行
    for(;parse_index < recv_len;++parse_index)
    {
        char tmp = buf[parse_index];
        switch (header_status)
        {
        case header_start:
            if(tmp == '\r')
                header_status = header_blank;
            else if(isalpha(tmp) || tmp == '-' || tmp == '_' || tmp == '~' || tmp == '.')
                key.push_back(tmp);
            else if(tmp == ':')
                header_status = header_colon;
            else 
                return REQUEST_ERROR;
        break;
        case header_colon:
            if(tmp == ' ')
                header_status = header_colon_sp;
            else 
                return REQUEST_ERROR;
        break;
        case header_colon_sp:
            if(tmp != '\t' && tmp != ' ')
            {
                if(tmp != '\r' && tmp != '\n')
                {
                    header_status = header_value;
                    value.push_back(tmp);
                }
                else 
                    return REQUEST_ERROR;
            }
        break;
        case header_value:
            if(tmp == '\r')
                header_status = header_almost_done;
            else if(tmp == '\n')
                return REQUEST_ERROR;
            else 
                value.push_back(tmp);
        break;
        case header_almost_done:
            if(tmp == '\n')
            {
                if(value.find(string("Connection: close")) != string::npos )//客户端不支持keep-alive
                {
                    keep_alive = false;
                }
                
                headers.insert({key,value});
                key.clear();
                value.clear();
                header_status = header_start;
                //return LINE_DONE;
            }    
            else 
                return REQUEST_ERROR;
        break;
        case header_blank:
            if(tmp == '\n')
            {
                url_que.push(url);
                url_for_send = url;
                ++send_task_count;
                ++parse_index;
                return HEADER_DONE;   
            }
            else 
                return REQUEST_ERROR;
        break;
        }
    }
    return LINE_OPEN;
}

void Myhttp::init_responese()
{
    //printf("reading file.\n");
    string base = WWWROOT;
    if(url_for_send.compare("/") == 0)
        url_for_send += "index.html";
    base += url_for_send;
    FILE* fd = fopen(base.c_str(),"r");
    string status_line;
    if(fd == NULL)//no this file
    {
        status_line += "HTTP/1.1 404 NOT FOUND\r\n\r\n";
        send_len = status_line.length();
        send_buf = new char[send_len];
        memcpy(send_buf,status_line.c_str(),send_len);
        return;
    }
    else
    {
        status_line += "HTTP/1.1 200 ok\r\n";
        if(keep_alive == true)
        {
            status_line += "Connection: keep-alive\r\n";
            status_line += "Keep-Alive: timeout=20, max=100\r\n";
        }
        else
        {
            status_line += "Connection: close\r\n";
            
        }
    }
    if(url_for_send.find(string(".html")) != string::npos)
    {
        status_line += "Content-Type: text/html\r\n";
    }
    else if(url_for_send.find(string(".css")) != string::npos)
    {
        status_line += "Content-Type: text/css\r\n";
    }
    else if(url_for_send.find(string(".png")) != string::npos)
    {
        status_line += "Content-Type: image/png\r\n";
    }
    else if(url_for_send.find(string(".jpg")) != string::npos)
    {
        status_line += "Content-Type: image/jpg\r\n";
    }
    struct stat statbuf;
    stat(base.c_str(),&statbuf);
    file_size=statbuf.st_size;
    char *file = new char[file_size];
    fread(file,file_size,file_size,fd);
    fclose(fd);

    // if(url_for_send.find(string(".png"))!=string::npos || url_for_send.find(string(".jpg"))!=string::npos  || url_for_send.find(string(".gif"))!=string::npos || url_for_send.find(string("jpeg"))!=string::npos || url_for_send.find(string(".bmp"))!=string::npos )
    // {
    //     status_line += "Content-Type: image/png\r\n";
    // }
    status_line += "Content-Length: ";
    status_line += to_string(file_size);
    status_line += "\r\n\r\n";
    send_len = file_size + status_line.length();
    send_buf = new char[send_len];
    memcpy(send_buf,status_line.c_str(),status_line.length());
    memcpy(send_buf+status_line.length(),file,file_size);

    delete file;
    file = NULL;
}

void Myhttp::send_responese(int sock)
{
    if(!init_respon_once)
        init_responese();
    while(1)
    {
        int ret = write(sock,send_buf+already_send,send_len-already_send);
        if(ret > 0)
        {
            already_send += ret;
            if(already_send == send_len)
            {
                f_responese = true;
                return;
            }
        }
        else if(ret == -1 && errno == EAGAIN)
        {
            return;
        }
    }
}

void Myhttp::send_responese_for_keepalive(int sock)
{//先从que中取url，这个url的responese发完之后再pop
    switch(send_status)
    {
        case start_send:
            url_for_send = url_que.front();
            init_responese();
            while(1)
            {
                int ret = write(sock,send_buf+already_send,send_len-already_send);
                if(ret > 0)
                {
                    //printf("实发:%d 应发:%d\n",ret,send_len-already_send);
                    already_send += ret;
                    if(already_send == send_len)
                    {
                        f_responese = true;
                        send_status = send_done;
                        return;
                    }
                }
                else if(ret == -1)
                {
                    send_status = sending;
                    return;
                }
            }
        break;
        
        case sending:
            while(1)
            {
                int ret = write(sock,send_buf+already_send,send_len-already_send);
                if(ret > 0)
                {
                    //printf("实发:%d 应发:%d\n",ret,send_len-already_send);
                    already_send += ret;
                    if(already_send == send_len)
                    {
                        f_responese = true;
                        send_status = send_done;
                        return;
                    }
                }
                else if(ret == -1)
                {
                    return;
                }
            }
        break;

        case send_done:
            url_for_send.clear();
            reset_responese_flag();
            url_que.pop();
            --send_task_count;
        break;
    }
    
}

void Myhttp::reset_parse_flag()
{
    parse_index = 0;
    recv_len = 0;
    http_status = CHECK_REQUEST_LINE;
    line_status = LINE_OPEN;
    memset(buf,0,buf_len);
    request_line_status = rline_start;
    header_status = header_start;
    closed = false;
    url.clear();
}

void Myhttp::reset_responese_flag()
{
    send_len = 0;
    file_size = 0;
    already_send = 0;
    f_responese = false;
    delete send_buf;
    send_buf = NULL;
    send_status = start_send;
}

bool Myhttp::is_parse_ended()
{
    return http_status==REQUEST_DONE;
}

bool Myhttp::is_respon_ended()
{
    return f_responese;
}

bool Myhttp::is_closed()
{
    return closed;
}

bool Myhttp::is_keep_alive()
{
    return keep_alive;
}

bool Myhttp::has_send_tasks()
{
    return send_task_count>0;
}
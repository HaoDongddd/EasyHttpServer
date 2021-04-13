#include "http2.h"
#include "../log_system/log.h"
#include <assert.h>
extern Log log;
//正常接收返回0，对端关闭返回-1
int Myhttp::get_buf(int clientfd)
{
    while(1)
    {
        int ret = read(clientfd,recv_buf+already_recv_len,recv_buf_len);
        
        if(ret == -1)
        {
			if(errno == EAGAIN)
            return 0;
			else 
			return -1;
        }
        else if(ret == 0)
        {
            close_ = true;
            return -1;
        }
		already_recv_len += ret;
    }
}

void Myhttp::parse_packet(int clientfd)
{
    int ret;
    if(get_buf(clientfd) == -1)
    {
        parse_status = parse_end;
        return;
    }
    while(parse_index < already_recv_len)
    {
        switch(this_package_status)
        {
            case CHECK_REQUEST_STATUS_LINE:
                ret = parse_request_status_line();
                if(ret == OK)
                {
                    //parsing
                    if(http_version.find("HTTP/1.0") != std::string::npos || http_version.find("HTTP/0.9") != std::string::npos)
                    {
                        keep_alive = false;
                    }
                    else 
                    {
                        keep_alive = true;
                    }
					parse_status = parsing;
					this_package_status = CHECK_REQUEST_HEADERS;
                }
                else if(ret == MORE)
                {
                    //parsing
                    parse_status = parsing;
                    return;
                }
                else 
                {
                    //parse end
                    parse_status = parse_end;
                    responese_status = bad_respone;
                    return;
                }
                break;


            case CHECK_REQUEST_HEADERS:
                ret = parse_request_headers();
                if(ret == OK)
                {
                    //parse end
                    if(headers.find("Connection") != headers.end())
                    {
                        keep_alive = true;
                    }
                    // if(keep_alive)
                    // {
                    //     url_que.push(url);
                    //     url.clear();
                    // }
                    ++send_task_count;
					parse_status = parse_end;
					return;
                }
                else if(ret == MORE)
                {
                    //parsing
                    parse_status = parsing;
                    return;
                }
                else 
                {
                    //parse end
                    parse_status = parse_end;
                    responese_status = bad_respone;
                    return;
                }
                break;
        }
    }
}

int Myhttp::parse_request_status_line()
{
    for(;parse_index < already_recv_len;++parse_index)
    {
        char tmp_char = recv_buf[parse_index];
        switch(status_line_parse_status)
        {
            case rline_start:
                if( !isalpha(tmp_char) )
                    return ERROR;
                else
                    {
                        status_line_parse_status = rline_method;
                        method.push_back(tmp_char);
                    }
            break;
            case rline_method:
                if(tmp_char == ' ' || tmp_char == '\t')
                {
                    if(method.length() < 2)
                        return ERROR;
                    status_line_parse_status = rline_method_sp;
                }
                else if(  isalpha(tmp_char) )
                    method.push_back(tmp_char);
                else
                    return ERROR;
            break;
            case rline_method_sp:
                if(tmp_char != ' ' && tmp_char != '\t')    
                {
                    if(tmp_char == '/')
                    {
                        url.push_back(tmp_char);
                        status_line_parse_status = rline_url;
                    }
                    else
                        return ERROR;
                }
            break;
            case rline_url:
                if(tmp_char == '/' || isalpha(tmp_char) || tmp_char == '.' || isdigit(tmp_char) || tmp_char == '_' || tmp_char == '-')
                    url.push_back(tmp_char);
                else if(tmp_char == '\t' || tmp_char == ' ')
                {
                    status_line_parse_status = rline_url_sp;
                }
                else 
                    return ERROR;
            break;
            case rline_url_sp:
                if( tmp_char != ' ' || tmp_char != '\t')
                {
                    if(isalpha(tmp_char))
                    {
                        status_line_parse_status = rline_version;
                        http_version.push_back(tmp_char);
                    }    
                    else 
                        return ERROR;
                }
            break;
            case rline_version:
                if(tmp_char == '.' || tmp_char == '/' || isdigit(tmp_char) || isalpha(tmp_char))
                    http_version.push_back(tmp_char);
                else if(tmp_char == '\r')
                    status_line_parse_status = rline_almost_done;
                else 
                    return ERROR;
            break;
            case rline_almost_done:
                if(tmp_char == '\n')
                {
                    ++parse_index;
                    return OK;
                }
                else 
                    return ERROR;
            break;
        }
    }
    return MORE;
}

int Myhttp::parse_request_headers()
{
    //要检测空行
    for(;parse_index < already_recv_len;++parse_index)
    {
        char tmp = recv_buf[parse_index];
        switch (headers_parse_status)
        {
        case header_start:
            if(tmp == '\r')
                headers_parse_status = header_blank;
            else if(isalpha(tmp) || tmp == '-' || tmp == '_' || tmp == '~' || tmp == '.')
                key.push_back(tmp);
            else if(tmp == ':')
                headers_parse_status = header_colon;
            else 
                return ERROR;
        break;
        case header_colon:
            if(tmp == ' ')
                headers_parse_status = header_colon_sp;
            else 
                return ERROR;
        break;
        case header_colon_sp:
            if(tmp != '\t' && tmp != ' ')
            {
                if(tmp != '\r' && tmp != '\n')
                {
                    headers_parse_status = header_value;
                    value.push_back(tmp);
                }
                else 
                    return ERROR;
            }
        break;
        case header_value:
            if(tmp == '\r')
                headers_parse_status = header_almost_done;
            else if(tmp == '\n')
                return ERROR;
            else 
                value.push_back(tmp);
        break;
        case header_almost_done:
            if(tmp == '\n')
            {
                headers.insert({key,value});
                key.clear();
                value.clear();
                headers_parse_status = header_start;
            }    
            else 
                return ERROR;
        break;
        case header_blank:
            if(tmp == '\n')
            {
                ++parse_index;
                return OK;   
            }
            else 
                return ERROR;
        break;
        }
    }
    return MORE;
}

void Myhttp::init_response_header()
{
    std::string base = WWWROOT;
    if(url.compare("/") == 0)
        url = "/index.html";
    base += url;
	file_name = base;
    FILE* fd = fopen(base.c_str(),"r");
    if(fd != NULL)
    {
        struct stat statbuf;
        stat(base.c_str(),&statbuf);
        need_send_len +=statbuf.st_size;
	    file_size = statbuf.st_size;
	    fclose(fd);
    }
    else
    {
        responese_status = bad_respone;
    }
	
    if(responese_status == bad_respone)
    {
        responese_header += "HTTP/1.1 404 NOT FOUND\r\nContent-Length: 13\r\n\r\n404 not found";
    }
    else
    {
        if(keep_alive)
        {
            responese_header += "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nKeep-Alive: timeout=20, max=100\r\nContent-Length: ";
			responese_header += std::to_string(file_size);
            responese_header += "\r\n";
            responese_header += "file-name: ";
            responese_header += url;
            responese_header += "\r\n";
            if(file_name.find(".html") != std::string::npos || file_name.find(".htm") != std::string::npos)
            {
                responese_header += "Content-Type: text/html\r\n";
            }
            else if(file_name.find(".css") != std::string::npos)
            {
                responese_header += "Content-Type: text/css\r\n";
            }
            else if(file_name.find(".js") != std::string::npos)
            {
                responese_header += "Content-Type: text/javascript\r\n";
            }
            else if(file_name.find(".png") != std::string::npos)
            {
                responese_header += "Content-Type: image/png\r\n";
            }
            else if(file_name.find(".jpg") != std::string::npos || file_name.find(".jpeg") != std::string::npos || file_name.find(".jfif") != std::string::npos || file_name.find(".pjpg") != std::string::npos)
            {
                responese_header += "Content-Type: image/jpeg\r\n";
            }
            else if(file_name.find(".gif") != std::string::npos)
            {
                responese_header += "Content-Type: image/gif\r\n";
            }
			responese_header += "\r\n";
        }
        else 
        {
            responese_header += "HTTP/1.1 200 OK\r\n\r\n";
        }
    }
    
	responese_header_len = responese_header.length();
    need_send_len += responese_header_len;
	responese_already_init = true;
	memmove(send_buf,responese_header.c_str(),responese_header_len);
}

void Myhttp::send_all_data(int clientfd)
{
    char log_buf[50];
    sprintf(log_buf,"%d for : %s",clientfd,url.c_str());
    //log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
    if(responese_already_init == false)
    {
        init_response_header();
    }
    if(responese_status == normal_responese)
    {
        while(already_send_len < need_send_len)
        {
            if(already_send_len >= responese_header_len)// 发完了头，开始发body，用sendfile发
            {
                if(!file_already_open)
                {
                    file_already_open = true;
                    file_fd = open(file_name.c_str(),O_RDONLY);
	    			assert(file_fd!=0);
                }
                int ret = sendfile(clientfd,file_fd,send_file_offset,file_size);
                if(ret == -1)
                {
                    return;
                }
                else if(ret == 0)
                {
                    return;
                }
                else 
                    already_send_len += ret;
            }
            else//还没发完头
            {
                while(1)
                {
                    int ret = write(clientfd,send_buf+already_send_len,responese_header_len-already_send_len);
                    if(ret == -1 && errno == EAGAIN)
                    {
                        return;
                    }
                    else if(ret == 0)
                    {
                        return;
                    }
                    already_send_len += ret;
                    if(already_send_len == responese_header_len)
                    {
                        break;
                    }
                }
            }
        }
        if(already_send_len == need_send_len && file_already_open)
        {
	    	assert(file_fd != 0);
	    	//if(file_fd == 0)printf("already_send_len:%d  need_send_len:%d  \n",already_send_len,need_send_len);
            close(file_fd);
            char log_buf[50];
            sprintf(log_buf,"close file_fd: %d",file_fd);
            //log.append(Log::INFO,INFO_S,log_buf,strlen(log_buf),__FILE__,__LINE__);
	    	file_already_open = false;
            --send_task_count;
        }
    }
    else 
    {//bad_responese  就发一个头就够了
        while(1)
        {
            int ret = write(clientfd,send_buf+already_send_len,responese_header_len-already_send_len);
            if(ret == -1 && errno == EAGAIN)
            {
                return;
            }
            else if(ret == 0)
            {
                return;
            }
            already_send_len += ret;
            if(already_send_len == responese_header_len)
            {
                break;
            }
        }
        --send_task_count;
    }
    
}

void Myhttp::reset_parse_flag()
{
    // keep_alive = true;
    close_ = false;
    parse_index = 0;
    this_package_status = CHECK_REQUEST_STATUS_LINE;
    parse_status = parsing;
    status_line_parse_status = rline_start;
    headers_parse_status = header_start;
    responese_status = normal_responese;
    method.clear();
    http_version.clear();
    already_recv_len = 0;

    key.clear();
    value.clear();
    headers.clear();
    memset(recv_buf,0,BUFLEN);
}

void Myhttp::reset_send_flag()
{
    url.clear();
    already_send_response_header = false;
    file_already_open = false;
    responese_already_init = false;
    responese_header_len = 0;
    send_task_count = 0;
    send_file_offset = nullptr;
    need_send_len = 0;
    already_send_len = 0;
    responese_header.clear();
	file_name.clear();
    file_size = 0;
    memset(send_buf,0,BUFLEN);
}

bool Myhttp::is_keep_alive()
{
    return keep_alive;
}

bool Myhttp::is_closed()
{
    return close_;
}

bool Myhttp::is_parse_over()
{
    return parse_status == parse_end;
}

bool Myhttp::is_send_over()
{
    return send_task_count==0;
}

bool Myhttp::need_to_send()
{
    return send_task_count>0;
}
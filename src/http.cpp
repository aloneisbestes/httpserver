#include "http.h"
#include "locker.h"
#include "log.h"
#include "debug.h"

#include <fstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>


using std::string;
using std::map;

// 定义 http 响应的状态
const char *ok_200 = "Ok";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 初始化类中静态变量
int HttpConn::m_user_count = 0;
int HttpConn::m_epollfd = -1;

locker m_lock;  // 互斥锁
map<string, string> users;

HttpConn::HttpConn() {

}

HttpConn::~HttpConn() {
    if (m_url)  delete m_url;

    if (m_version) delete m_version;
}

// 初始化 mysql 中存储的用户名和密码到程序
void HttpConn::initMysqlResult(MysqlPool *conn_pool) {
    // 从连接池中取出一个连接
    MYSQL *sql = nullptr;
    MysqlConnRAII sql_conn(&sql, conn_pool);

    // 在 user 表中检索 username, passwd 数据
    if (mysql_query(sql, "select username,passwd from user")) {
        LogError("mysql select error: %s\n", mysql_error(sql));
    }

    // 从表中检索完整的结果集合
    MYSQL_RES *result = mysql_store_result(sql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_field(result);

    // 从结果集中获取下一行，将对应的用户名和密码存入 map 中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd) {
    // 记录旧文件描述符说明
    int old_option = fcntl(fd, F_GETFL);
    // 创建新的文件描述符说明
    int new_option = old_option | O_NONBLOCK;
    // 重新设置新的文件描述符为非阻塞
    fcntl(fd, F_SETFL, new_option);

    return old_option;  // 返回旧的文件描述符描述
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT，one_shot表示ET模式下值出发一次
// 如果在多线程开发时，如果使用 ET 模式，会出现如下情况：
// 当epoll_wait已经检测到socket描述符fd1，并通知应用程序处理fd1的数据，
// 那么处理过程中该fd1又有新的数据可读，会唤醒其他线程对fd1进行操作，那么就出现了两个工作线程同时处理fd1的情况，
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event ev;
    ev.data.fd = fd;

    // 添加触发机制    
    if (TRIGMode == 1) 
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else 
        ev.events = EPOLLIN | EPOLLRDHUP;

    // 是否选择 ET 模式下只触发一次
    if (one_shot)
        ev.events |= EPOLLONESHOT;
    
    // 添加事件到 epoll 注册表中
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    // 将 fd 描述符设置为 非阻塞模式
    setnonblocking(fd);
}

// 从内核事件中删除监听的 fd
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int events, int TRIGMode) {
    epoll_event ev;
    ev.data.fd = fd;

    // 重置事件 
    if (TRIGMode == 1) 
        ev.events = events | EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    else 
        ev.events = events | EPOLLONESHOT | EPOLLRDHUP;
    
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

// 关闭连接，关闭一个连接，用户技术减一
void HttpConn::closeConn(bool real_close) {
    if (real_close && (m_sockfd != -1)) {
        DebugPrint("close fd: %d\n", m_sockfd);
        // 从内核事件中移除文件描述符
        removefd(m_epollfd, m_sockfd);
        // 用户数量减一
        m_sockfd = -1;
        m_user_count--;       
    }
}


// 初始化函数
void HttpConn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode, \
                    int close_log, const string &user, const string &passwd, const string &sqlname) {
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, m_sockfd, true, TRIGMode);
    ++m_user_count;

    // 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    m_close_log = close_log;
    m_doc_root = root;

    strcpy(m_sql_user, user.c_str());
    strcpy(m_sql_passwd, passwd.c_str());
    strcpy(m_sql_name, sqlname.c_str());

    // 调用内部初始化函数
    this->init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void HttpConn::init() {
    m_sql = nullptr;
    m_bytes_have_send = 0;
    m_bytes_to_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false; 
    m_method = GET;
    m_content_length = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_cgi = 0;
    m_state = 0;
    m_timer_falg = 0;
    m_improv = 0;
    m_bytes_read = 0;

    m_url = new char[URL_SER_HOST_MAX];
    memset(m_url, 0, URL_SER_HOST_MAX);
    m_version = new char[URL_SER_HOST_MAX];
    memset(m_version, 0, URL_SER_HOST_MAX);
    m_host = new char[URL_SER_HOST_MAX];
    memset(m_host, 0, URL_SER_HOST_MAX);

    memset(m_read_buf, 0, READ_BUFFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
    memset(m_real_file, 0, FILENAME_LEN);
}

// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
HttpConn::LINE_STATUS HttpConn::parseLine() {
    char tmp;
    for (;m_checked_idx < m_read_idx; ++m_checked_idx) {    
        // m_check_idx 当前 m_read_buf 中读取的位置,m_read_idx当前 m_read_buf 的长度
        tmp = m_read_buf[m_checked_idx];
        if (tmp == '\r') {
            // 判断是否已经被读取过
            if ((m_checked_idx + 1) == m_read_idx) { // 表示已经被读取过了
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') {
                // 表示一行读取完毕
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (tmp == '\n') {
            if (m_checked_idx > 1 && (m_read_buf[m_checked_idx - 1] == '\r')) {
                // 保证最后两个字符为 '\0' 
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool HttpConn::readOnce() {
    if (m_read_idx >= READ_BUFFER_SIZE) // 
        return false;
    
    if (m_TRIGMode == 0) {
        // 表示为 EPOLLIN 模式
        m_bytes_read = recv(m_sockfd, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
        m_read_idx = m_bytes_read;

        if (m_bytes_read <= 0) 
            return false;
    } else {
        // 表示为 EPOLLET 模式
        while (true) {  // 一次性读取所有数据
            m_bytes_read = recv(m_sockfd, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
            if (m_bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) 
                    break;
                return false;
            } else if (m_bytes_read == 0) {
                return false;
            }

            m_read_idx += m_bytes_read;
        }
        return true;
    }
}

// 解析http请求行，获得请求方法，目标url及http版本号
HttpConn::HTTP_CODE HttpConn::parseRequestLine(char *text) {
    m_url = strpbrk(text, " \t");
    if (!m_url)     // 解析失败，没有 m_url
        return BAD_REQUEST;
    
    char *tmp_url = strpbrk(text, " \t");
    if (tmp_url == nullptr) // 解析失败，在请求中没有 url 
        return BAD_REQUEST;
    
    *tmp_url++ = '\0';    // text首地址是请求方法，m_url首地址后面是 url 和版本号

    // 记录请求方法
    char *method = text;

    if (strcasecmp(method, "GET") == 0) {
        m_method = GET;
    } else if (strcasecmp(method, "POST") == 0) {
        m_method = POST;
        m_cgi = 1;      // ?
    } else {
        return BAD_REQUEST;
    }

    // 获取url
    tmp_url += strspn(tmp_url, " \t");
    DebugPrint("tmp_url: %s\n", tmp_url);
    
    // 得到 http 版本所在位置
    char *tmp_version = strpbrk(tmp_url, " \t");
    DebugPrint("tmp_version: %s\n", tmp_version);

    if (tmp_version == nullptr) {
        return BAD_REQUEST;
    }
    *tmp_version++ = '\0';  // 找到版本号所在位置

    // 获取版本号
    tmp_version += strspn(tmp_version, " \t");
    // 判断版本号是否正确
    if (strcasecmp(tmp_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    strcpy(m_version, tmp_version);
    DebugPrint("real version: %s\n", m_version);

    if (strncasecmp(tmp_url, "http://", 7) == 0) {
        // 获取真正的 url, 判断 http
        tmp_url += 7;
        tmp_url = strchr(tmp_url, '/');     // 找到第一个/所在位置
        strcpy(m_url, tmp_url);
    } else if (strncasecmp(tmp_url, "https://", 8) == 0) {
        // 获取真正的 url, 判断 https
        tmp_url += 8;
        tmp_url = strchr(tmp_url, '/');
        strcpy(m_url, tmp_url);
    } else {
        strcpy(m_url, tmp_url);
    }


    // 判断是否正确的 url 
    if (strlen(m_url) == 0 || m_url[0] != '/')
        return BAD_REQUEST;
    DebugPrint("real url: %s\n", m_url);
    DebugPrint("real version: %s\n", m_version);

    // 当 url 为 / 时显示主页面
    if (strlen(m_url) == 1) 
        strcat(m_url, "index.html");
    
    m_check_state = CHECK_STATE_HEADER; // 解析状态为头部
    return NO_REQUEST;
}

// 解析http请求的一个头部信息
HttpConn::HTTP_CODE HttpConn::parseHeaders(char *text) {
    if (text[0] == '\0') {
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;    // 表示有 content 
            return NO_REQUEST;
        }

        return GET_REQUEST; // 应该获取请求
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        // 获取连接类型
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
        DebugPrint("conn: %s\n", text);
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
        DebugPrint("len: %d\n", m_content_length);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        strcpy(m_host, text);
        DebugPrint("host: %s\n", m_host);
    } else {
        LogInfo("oop!unknow header: %s", text);
    }

    return NO_REQUEST;
}

// 判断http请求是否被完整读入
HttpConn::HTTP_CODE HttpConn::parseContent(char *text) {
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';
        m_string = text;
        DebugPrint("parseContent: %s\n", text);
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

// 读取数据进程
HttpConn::HTTP_CODE HttpConn::processRead() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = nullptr;


    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parseLine()) == LINE_OK)) {
        text = getLine();
        m_start_line = m_checked_idx;       // 更新起始位置
        LogInfo("%s", text);
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE:    // 读取请求行，获取 url 和 版本号以及请求方式
                ret = parseRequestLine(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            case CHECK_STATE_HEADER:   //  解析请求头
                ret = parseHeaders(text);
                if (ret = BAD_REQUEST)
                    return BAD_REQUEST;
                else if (ret == GET_REQUEST)    // 继续获取请求
                    return doRequest();
                break;
            case CHECK_STATE_CONTENT:
                ret = parseContent(text);
                line_status = LINE_OPEN;
                break;
            default:
                return INTERNAL_ERROR;  // 否则发生错误
        }
    }
    return NO_REQUEST;
}
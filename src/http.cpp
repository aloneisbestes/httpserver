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
                    int close_log, const string &user, const string passwd, const string &sqlname) {
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
    m_url = nullptr;
    m_version = nullptr;
    m_content_length = 0;
    m_host = nullptr;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_cgi = 0;
    m_state = 0;
    m_timer_falg = 0;
    m_improv = 0;

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

    }
}
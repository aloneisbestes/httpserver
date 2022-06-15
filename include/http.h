#ifndef __HTTP_H__
#define __HTTP_H__

/* c 头文件 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

/* c++ 头文件 */
#include <string>
#include <map>

/* 自定义头文件 */
#include "mysqlpool.h"

class HttpConn{ 
public:
    static const int FILENAME_LEN=200;          // 
    static const int READ_BUFFER_SIZE=2048;     // 读取数据缓冲区
    static const int WRITE_BUFFER_SIZE=1024;    // 写数据缓冲区

    enum METHOD {   // http 请求方式
        GET=0,      // 向特定的资源发出请求
        POST,       // 向指定资源提交数据进行处理请求（例如提交表单或者上传文件）
        HEAD,       // 向服务器索与GET请求相一致的响应，只不过响应体将不会被返回
        PUT,        // 向指定资源位置上传其最新内容
        DELETE,     // 请求服务器删除Request-URL所标识的资源
        CONNECT,    // HTTP/1.1协议中预留给能够将连接改为管道方式的代理服务器。
        OPTIONS,    // 返回服务器针对特定资源所支持的HTTP请求方法，也可以利用向web服务器发送‘*’的请求来测试服务器的功能性
        TRACE       // 回显服务器收到的请求，主要用于测试或诊断
    };

    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE=0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT,
    };

    enum HTTP_CODE {    // http 返回码
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    enum LINE_STATUS {
        LINE_OK=0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    /* 构造和析构 */
    HttpConn();
    ~HttpConn();

public:
    /* 共有成员函数 */
    // 初始化函数
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode, \
              int close_log, const std::string &user, const std::string passwd, const std::string &sqlname);
    // 关闭连接
    void closeConn(bool real_close=true);
    // 主进程
    void process();
    //循环读取客户数据，直到无数据可读或对方关闭连接
    //非阻塞ET工作模式下，需要一次性将数据读完
    bool readOnce();
    // 写数据
    bool write();
    // 返回 sockaddr_in
    sockaddr_in *getAddress() { }
    // 初始化 mysql 中存储的用户名和密码到程序
    void initMysqlResult(MysqlPool *conn_pool);

private:
    /* 私有变量 */
    int m_timer_falg;     // 定时器标志
    int m_improv;

private:
    /* 内部私有方法 */
    void init();
    // 读取数据进程
    HTTP_CODE processRead();
    // 写入数据进程
    bool processWrite(HTTP_CODE ret);   
    // 解析http请求行，获得请求方法，目标url及http版本号
    HTTP_CODE parseRequestLine(char *text); 
    // 解析http请求的一个头部信息
    HTTP_CODE parseHeaders(char *text);
    // 判断http请求是否被完整读入
    HTTP_CODE parseContent(char *text);
    // 读取请求
    HTTP_CODE doRequest();
    // 获取一行
    char *getLine() { }
    // 从状态机，用于分析出一行内容
    // 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
    LINE_STATUS parseLine();
    // 内存共享
    void unmap();
    // 添加响应
    bool addResponse(const char *format, ...);
    // 添加响应内容
    bool addContent(const char *content);
    // 添加头部信息
    bool addHeaders(int content_length);
    // 添加状态
    bool addStatusLine(int status, const char *title);
    // 添加内容的类型
    bool addContentType();
    // 添加内容的长度
    bool addContentLength(int content_length);
    // 添加？
    bool addLinger();
    // 添加?
    bool addBlankLine();

public:
    static int m_epollfd;           // epoll 描述符
    static int m_user_count;        // 用户计数
    MYSQL *m_sql;
    int m_state;                    // 是否为读或写，读为0，写为1

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;         // 当前 read_buffer 所在的索引
    int m_checked_idx;      // ?
    int m_start_line;       // ?
    char m_write_buf[WRITE_BUFFER_SIZE];
    CHECK_STATE m_check_state;
    METHOD m_method;        //请求类型
    char m_real_file[FILENAME_LEN];     // 读取文件
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;
    char *m_file_address;   // ?
    struct stat m_file_stat;        // 文件类型
    struct iovec m_iv[2];           // ? #include <sys/uio.h> 建议百度 iovec
    int m_iv_count;
    int cgi;    // 是否启用 POST
    char *m_string;  // 存储请求头数据
    int bytes_to_send;  // 发送的数据字节数 
    int bytes_have_send;    // 已发送的字节数
    char *doc_root;         // ?

    std::map<string, string> m_users;   // 用来存储用户名和密码
    int m_TRIGMode;     // ?
    int m_close_log;    // 是否开启日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif __HTTP_H__
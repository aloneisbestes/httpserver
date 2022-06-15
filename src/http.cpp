#include "http.h"
#include "locker.h"
#include "log.h"

#include <fstream>


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
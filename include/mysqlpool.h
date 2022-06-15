#ifndef __MYSQL_POOL_H__
#define __MYSQL_POOL_H__

/**
 * @file mysqlpool.h
 * @author garteryang (aloneisbestes@gmail.com)
 * @brief 
 * 作用: mysql 连接池 
 * @version 0.1
 * @date 2022-06-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <string>
#include <list>
#include <mysql/mysql.h>
#include "locker.h"

using std::string;

class MysqlPool {
private:
    // mysql 必须字段
    int         m_prot;
    std::string m_ip;
    std::string m_user;
    std::string m_passwd;
    std::string m_db;

    // 连接池相关变量
    int m_size;     // 连接池的大小
    int m_free;     // 连接池空闲的链接
    int m_use;      // 连接池已使用的链接
    std::list<MYSQL *> m_sql_pool;

    // 其他相关变量
    bool    m_close_log;    // 是否开启日志
    locker  m_mutex;        // 互斥锁
    sem     m_sem;          // 信号量

public:
    // 初始化msyql
    void    init(const string &ip, const string &user, const string passwd, const string &db, \
                 int port=3306, int max_conn=10, int close_log=false);
    MYSQL   *getMysqlConnect(); // 获取一个连接
    bool    freeMysqlConnect(MYSQL *sql); // 释放一个连接到连接池
    void    destroyMysqlConnect();  // 释放所有连接

    // 获取空闲的连接个数
    int     getFreeConnect() { return m_free; } 

    // 单例模式
    static MysqlPool *get() {
        static MysqlPool sql;
        return &sql;
    }

private:
    MysqlPool();
    ~MysqlPool();
};


class MysqlConnRAII {
public:
    MysqlConnRAII(MYSQL **sql, MysqlPool *sql_pool);
    ~MysqlConnRAII();

private:
    MYSQL *sqlRAII;
    MysqlPool *poolRAII;
};

#endif // __MYSQL_POOL_H__
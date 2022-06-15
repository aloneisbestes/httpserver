#include <exception>
#include "mysqlpool.h"
#include "log.h"

MysqlPool::MysqlPool() {
    m_free = 0;
    m_use = 0;
    m_size = 0;
    m_prot = 0;
}

MysqlPool::~MysqlPool() {
    destroyMysqlConnect();
}

// 初始化msyql
void MysqlPool::init(const string &ip, const string &user, const string passwd, const string &db, \
                     int port, int max_conn, int close_log) {
    m_ip = ip;
    m_user = user;
    m_passwd = passwd;
    m_db = db;
    m_prot = port;
    m_close_log = close_log;

    // 创建连接池
    LogInfo("start make mysql connect pool.");
    for (int i = 0; i < max_conn; ++i) {
        MYSQL *conn = nullptr;
        conn = mysql_init(nullptr);
        if (conn == nullptr) {
            LogError("mysql init failed.");
            exit(EXIT_FAILURE);
        }

        conn = mysql_real_connect(conn, m_ip.c_str(), m_user.c_str(), m_passwd.c_str(), m_db.c_str(), m_prot, nullptr, 0);
        if (conn == nullptr) {
            LogError("mysql real connect failed.");
            free(conn); // 释放产生的资源
            continue;
        }

        m_sql_pool.push_back(conn);
        ++m_free;
        LogDebug("mysql connect successfully.");
    }
    LogInfo("mysql connect number %d is successfully.", m_free);

    // 初始化信号量
    try {
        m_sem = sem(m_free);
    } catch (std::exception){
        LogError("sem init error.");
        exit(EXIT_FAILURE);
    }

    if (m_free == 0) {
        LogError("mysql connect pool create failed, quit program.");
        exit(EXIT_FAILURE);
    }

    m_size = m_free;
    m_use = 0;
}

// 获取一个连接
MYSQL *MysqlPool::getMysqlConnect() {
    if (m_free == 0)   // 如果当前的空闲连接为空，则返回nullptr
        return nullptr;

    m_sem.wait();

    MYSQL *retSql = nullptr;
    m_mutex.lock();
    // 从连接池中拿区一个
    retSql = m_sql_pool.front();
    m_sql_pool.pop_front();

    --m_free;
    ++m_use;

    m_mutex.unlock();
    return retSql;
}

// 释放一个连接到连接池
bool MysqlPool::freeMysqlConnect(MYSQL *sql) {
    if (sql == nullptr)
        return false;

    m_mutex.lock();

    m_sql_pool.push_back(sql);
    ++m_free;
    --m_use;

    m_mutex.unlock();

    m_sem.post();
    return true;
}

// 释放所有连接
void MysqlPool::destroyMysqlConnect() {
    m_mutex.lock();

    if (m_sql_pool.size() > 0) {
        std::list<MYSQL *>::iterator it;
        for (it = m_sql_pool.begin(); it != m_sql_pool.end(); ++it) {
            MYSQL *conn = *it;
            mysql_close(conn);
        }

        m_free = 0;
        m_use = 0;
        m_sem = 0;
        m_sql_pool.clear();
    }

    m_mutex.unlock();
}



MysqlConnRAII::MysqlConnRAII(MYSQL **sql, MysqlPool *sql_pool) {
    poolRAII = sql_pool;

    *sql = poolRAII->getMysqlConnect();
    sqlRAII = *sql;
}

MysqlConnRAII::~MysqlConnRAII() {
    poolRAII->freeMysqlConnect(sqlRAII);
}
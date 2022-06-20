#ifndef __LOG_H__
#define __LOG_H__

/**
 * 作用: 用来写入日志文件
 * user: garteryang
 * 邮箱: 910319432@qq.com
 */

#include <stdio.h>
#include <string>
#include <fstream>
#include "locker.h"
#include "macro.h"
#include "block.h"

class Log {
private:
    bool                        m_isasync;                              // 是否异步
    bool                        m_isclose;                              // 是否关闭日志文件
    int                         m_log_buffer_len;                       // log 缓存的大小
    int                         m_now_day;                              // log 文件以天分类
    pthread_t                   *m_tids;                                // 线程 id
    int                         m_thread_size;                          // 线程数量
    int                         m_thread_real_size;                     // 真正运行的线程数量
    std::ofstream               m_file_fp;                              // log 文件指针
    char                        *m_log_buffer;                          // log 缓冲区
    int                         m_log_max_line;                         // log 文件的最大行数
    int                         m_log_line_count;                       // log 行数记录
    int                         m_log_filename_count;                   // log 文件记数，当前天的第一个文件
    cond                        m_cond;                                 // 条件变量，用来异步写入日志文件
    locker                      m_mutex;                                // 互斥锁，用来保证数据的安全

    BlockQueue<std::string>     *m_log_block;                            // 阻塞队列
    char                        m_log_path[FILE_PATH_MAX_LINE];         // log 文件的路径
    char                        m_log_filename[FILE_PATH_MAX_LINE];     // log 文件名

private:
    Log();
    virtual ~Log();

    // 内部私有的线程处理函数
    void *asyncWriteLog();

    bool asNewFile(const char *filename);

public:
    /* log 日志使用单例模式 */
    static Log *getInstance() {
        static Log instance;
        return &instance;
    }

    /* log 日志的线程处理函数 */
    static void *logTrheadRun(void *) {
        // 单例调用，内部的线程处理函数
        Log::getInstance()->asyncWriteLog();
        return (void *)nullptr;
    }
    

public:
    // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    void init(const char *filename, bool isclose, int buffer_len=8192, \
              int line_max=FILE_MAX_LINE, int block_max=BLOCK_QUEUE_MAX_LEN, \
              int thread_size=1);

    // 写日志
    void writeLog(int level, const char *format, ...); 

    // 立即刷新
    void flush();
};

#define LogDebug(format, ...) { \
    if (!m_close_log) { \
        Log::getInstance()->writeLog(DEBUG_TYPE, format, ##__VA_ARGS__);\
    }\
}

#define LogInfo(format, ...) { \
    if (!m_close_log) { \
        Log::getInstance()->writeLog(INFO_TYPE, format, ##__VA_ARGS__);\
    }\
}

#define LogWarn(format, ...) { \
    if (!m_close_log) { \
        Log::getInstance()->writeLog(WARN_TYPE, format, ##__VA_ARGS__);\
    }\
}

#define LogError(format, ...) { \
    if (!m_close_log) { \
        Log::getInstance()->writeLog(ERROR_TYPE, format, ##__VA_ARGS__);\
    }\
}

#endif // __LOG_H__
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include "log.h"
#include "common.h"
#include "debug.h"



Log::Log() {
    // 初始化当前行数
    m_log_line_count = 0;
    m_thread_size = 0;
    m_thread_real_size = 0;
    m_log_filename_count = 0;

    // 初始化为不开启异步处理
    m_isasync = false;

    // 构造中初始化所有指针为空
    m_log_buffer = nullptr;
    m_log_block = nullptr;
    m_tids = nullptr;
}

Log::~Log() {
    // 如果 buffer 缓冲区不为空，则释放
    if (m_log_buffer) delete [] m_log_buffer;

    // 如果 阻塞队列不为空，则释放该指针
    if (m_log_block) delete m_log_block;

    if (m_tids) delete [] m_tids;
}

void *Log::asyncWriteLog() {
    std::string content;
    while (m_log_block->pop(content)) {
        m_mutex.lock();
        m_file_fp.write(content.c_str(), content.size());
        m_file_fp.flush();
        m_mutex.unlock();
    }

    return (void *)nullptr;
}

// 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
void Log::init(const char *filename, bool isclose, int buffer_len, int line_max, int block_max, int thread_size) {

    // 初始化线程数量及线程id数组
    m_thread_size = thread_size;
    m_tids = new pthread_t[m_thread_size];
    
    // 如果阻塞队列大于0，则开启异步线程
    if (block_max > 0) {
        int count;  // 记录真正的线程数量
        pthread_t *tmp_tids = m_tids;
        for (int i = 0; i < m_thread_size; ++i) {
            pthread_t tmp;
            if (pthread_create(&tmp, nullptr, logTrheadRun, nullptr) == 0) {
                *tmp_tids++ = tmp;
                m_thread_real_size++;   // 记录真正开启的线程
                pthread_detach(tmp);    // 线程脱离
            }
        }

        // 初始化阻塞队列
        m_log_block = new BlockQueue<std::string>(block_max);
        m_isasync = true;
    }
    
    // 初始化缓冲器大小及是否开启日志系统等等
    m_isclose = isclose;
    if (m_log_buffer == nullptr) {
        // 如果 m_log_buffer 为空在开辟，同时在更新 m_log_buffer_len
        m_log_buffer_len = buffer_len;
        m_log_buffer = new char[m_log_buffer_len];
    }
    m_log_max_line = line_max;

    /* 初始化和创建日志文件名称 */
    time_t t = time(nullptr);   // 获取当前时间的秒数
    struct tm *mid_time = localtime(&t);    // 转为年月日类型
    struct tm now_time = *mid_time;

    const char *tmp_path = strrchr(filename, '/');  // 找到路径中最后一个 '/'
    if (tmp_path == nullptr) {  // 如果 tmp_path 是 nullptr，则表示只有文件名
        const char *conf_path = GetConfigPath(nullptr, nullptr);

        // 从配置文件中读取 log 文件所在路径
        const char *log_paht = ReadConfig(conf_path, "log-path");
        if (log_paht == nullptr) {
            // 如果没有找到 log 文件的路径，则抛出一个异常
            throw "log path is not fond.";
            return ;
        }
        strcpy(m_log_path, log_paht);

        // 创建文件名指定日期等
        snprintf(m_log_filename, FILENAME_MAX, "%s_%d_%02d_%02d.log", filename, now_time.tm_year+1900, \
                                                                    now_time.tm_mon+1, now_time.tm_mday);
        DebugPrint("log file name: %s\n", m_log_filename);
    } else {
        const char *split = strrchr(filename, '/');
        if (split == nullptr) {
            throw "log file name get failed.";
            return ;
        }

        strncpy(m_log_path, filename, split - filename);
        strcpy(m_log_filename, split+1);
        DebugPrint("log file name: %s\n", m_log_filename);
    }

    // 检测log目录是否存在
    if (opendir(m_log_path) == nullptr) {
        // 目录不存在，则创建
        if (mkdir(m_log_path, 0775) != 0) {
            DebugPError("mkdir");
            exit(EXIT_FAILURE);
        }
    }

    // 打开文件
    char tmp_file_path[FILENAME_MAX];
    snprintf(tmp_file_path, FILENAME_MAX, "%s/%s", m_log_path, m_log_filename);
    m_file_fp.open(tmp_file_path, std::ofstream::app);
    DebugPrint("log file all path: %s\n", tmp_file_path);

    if (!m_file_fp.is_open()) {
        // 打开失败表示该文件不存在，创建一个
        if (!asNewFile(tmp_file_path)) {
            throw "create new file failed.";
            return ;
        }

        // 重写打开文件
        m_file_fp.open(tmp_file_path);
    }

    // 保存当前是那天
    m_now_day = now_time.tm_mday;
    // 当前当天的文件个数计数加1
    m_log_filename_count++;
    return ;
}


void Log::writeLog(int level, const char *format, ...) {
    // 获取当前时间
    struct timeval tm_val = {0, 0};
    gettimeofday(&tm_val, nullptr);
    time_t t = tm_val.tv_sec;
    struct tm *tmp_tm = localtime(&t);
    struct tm now_time = *tmp_tm;

    // 添加前缀，是debug，info，error，warn
    char s[20] = {0};
    switch (level) {
        case DEBUG_TYPE:
            strcpy(s, "[Debug]: ");
            break;
        case INFO_TYPE: 
            strcpy(s, "[Info]: ");
            break;
        case ERROR_TYPE:
            strcpy(s, "[Error]: ");
            break;
        case WARN_TYPE:
            strcpy(s, "[Warn]: ");
            break;
        default:
            strcpy(s, "[Info]: ");
            break;
    }

    m_mutex.lock();

    // 判断是否达到最大行，或者是当前的日志，日志以天分类
    if (now_time.tm_mday != m_now_day || m_log_line_count == m_log_max_line) {
        // 如果条件满足，则需要重新创建文件
        char tmp_file_path[FILE_PATH_MAX_LINE] = {0};
        char tmp_file_name[FILE_NAME_MAX] = {0};
        
        // 首先创建新的文件名
        if (m_log_line_count >= m_log_max_line) {
            snprintf(tmp_file_name, FILE_NAME_MAX, "%s_%d_%02d_%02d_%2d.log", m_log_filename, now_time.tm_year+1900, \
                                                                             now_time.tm_mon+1, now_time.tm_mday, \
                                                                             m_log_filename_count);
            m_log_filename_count++;
        } else {
            snprintf(tmp_file_name, FILE_NAME_MAX, "%s_%d_%02d_%02d.log",   m_log_filename, now_time.tm_year+1900, \
                                                                            now_time.tm_mon+1, now_time.tm_mday);
            m_now_day = now_time.tm_mday;
            m_log_filename_count = 0;   // 新的一天文件计数应该为0
        }
        m_log_line_count = 0;   // 重新计数文件当前行数

        // 重新打开文件新文件
        // 先尝试当前文件是否能打开
        snprintf(tmp_file_path, FILE_PATH_MAX_LINE, "%s/%s", m_log_path, tmp_file_name);
        std::ofstream tmp_file;
        tmp_file.open(tmp_file_path, std::ofstream::app);
        if (!tmp_file.is_open()) {  // 如果打开文件失败，表示该文件不存在
            // 重新创建一个该文件
            asNewFile(tmp_file_path);
        } else {
            tmp_file.close();   // 关闭文件，使用内部的 log 文件指针打开
        }

        // 关闭当前打开的文件
        m_file_fp.close(); 
        m_file_fp.open(tmp_file_path, std::ofstream::app);
    }
    m_log_line_count++;
    DebugPrint("m_log_line_cout: %d\n", m_log_line_count);
    m_mutex.unlock();

    va_list vlist;
    va_start(vlist, format);
    std::string log_srt;
    // 开始写入日志
    m_mutex.lock();

    // 写入日志的前缀，也就是写入的时间
    int n = snprintf(m_log_buffer, m_log_buffer_len, "%d-%0d-%0d %02d:%02d:%02d.%06d %s", now_time.tm_year+1900, \
                                                                     now_time.tm_mon+1,\
                                                                     now_time.tm_mday, now_time.tm_hour, now_time.tm_min, \
                                                                     now_time.tm_sec, tm_val.tv_usec, s);

    int m = vsnprintf(m_log_buffer+n, m_log_buffer_len-n, format, vlist);

    // 添加一个换行符号
    m_log_buffer[m+n] = '\n';
    m_log_buffer[m+n+1] = '\0';

    va_end(vlist);
    log_srt = m_log_buffer;
    m_mutex.unlock();
    
    if (m_isasync && !m_log_block->isfull()) {    // 如果开启异步日志，则写入阻塞队列
        m_log_block->push(log_srt);
    } else {
        m_mutex.lock();
        m_file_fp.write(log_srt.c_str(), log_srt.size());
        m_file_fp.flush();
        m_mutex.unlock();
    }

    return ;
}

bool Log::asNewFile(const char *filename) {
    std::ofstream outfile(filename);
    
    if (!outfile.is_open()) {
        return false;
    }

    outfile.close();

    return true;
}


// 立即刷新
void Log::flush() {
    m_mutex.lock();
    m_file_fp.flush();
    m_mutex.unlock();
}
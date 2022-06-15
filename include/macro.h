#ifndef __MACRO_H__
#define __MACRO_H__

/* 判断 debug macro 是否定义 */
#ifdef DEBUG_MACRO
// 如果定义 DEBUG_DEFINE，则定义 DEBUG 为 debug_true
#define DEBUG
#else
// 否则未定义，则定义 DEBUG 为 debug_false
#define NOT_DEBUG
#endif 

/* 定义文件和路径的最大长度 */
#define FILE_PATH_MAX_LINE      4096

/* 定义 log 日志默认的文件对大行数 */
#define FILE_MAX_LINE           8000000

/* 定义文件名的长度 */
#define FILE_NAME_MAX           256

/* 阻塞队列的对大长度 */
#define BLOCK_QUEUE_MAX_LEN     1000 

/* 定义一行的最大长度 */
#define LINE_MAX                1024

/* 定义配置文件目录名称 */
#define CONF_DIR_NAME           "conf"

/* 定义配置文件名 */
#define CONF_FILE_NAME          "httpserver.conf"

/* 定义写入日志的类型 */
#define DEBUG_TYPE              0
#define ERROR_TYPE              1
#define WARN_TYPE               2
#define INFO_TYPE               3

#endif // __MACRO_H__
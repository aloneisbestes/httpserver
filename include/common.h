#ifndef __COMMON_H__
#define __COMMON_H__

#include "macro.h"

/* 读取配置文件 */
// config 配置文件中对应的 value 值
extern char _conf_value[LINE_MAX];
// 获取配置文件key中对应的值，如果没有key就返回nullptr
char *ReadConfig(const char *confpath, const char *findkey, char *retvalue=nullptr);

/* 除去字符串中不需要的字符 */
bool ClearCharacter(char *src, char ch);        // ch: 字符
bool ClearCharacter(char *src, const char *ch_set);    // ch_set: 字符集

/* 获取配置文件路径及其文件名 */
extern char _conf_file_path[FILE_PATH_MAX_LINE];
const char *GetConfigPath(const char *dirname=nullptr, const char *filename=nullptr);

#endif // __COMMON_H__
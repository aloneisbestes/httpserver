#include <string.h>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"
#include "debug.h"

// config 配置文件中对应的 value 值
char _conf_value[LINE_MAX] = {0};
char _conf_file_path[FILE_PATH_MAX_LINE] = {0};

/* 读取配置文件 */
char *ReadConfig(const char *confpath, const char *findkey, char *retvalue) {
    // 判断配置文件路径是否为空
    if (confpath == nullptr || strlen(confpath) == 0) 
        return nullptr;

    // 打开配置文件
    std::ifstream conf_fp;
    conf_fp.open(confpath, std::ifstream::in);
    // 判断文件是否打开成功
    if (!conf_fp.is_open()) {
        DebugError("readconfig: config file open failed.\n");
        return nullptr;
    }

    // 打开成功读取数据，按行读取
    char buffer[LINE_MAX];
    while (!conf_fp.eof()) {    // 是否到达文件末尾，如果没有达到几继续循环
        memset(buffer, 0, LINE_MAX);
        conf_fp.getline(buffer, LINE_MAX);

        // 去除读取到的字符串中不需要的字符，如空格等
        ClearCharacter(buffer, " \",");

        // 使用strchr 分割字符串，找到需要的字符出现的第一个位置
        char key[LINE_MAX]={0};
        char value[LINE_MAX]={0};
        char *tmp_need = strchr(buffer, ':');
        if (tmp_need == nullptr)    // 如果没有找到 : 表示该行没有配置文件中件对应的值
            continue;
        // 拷贝键和值
        strncpy(key, buffer, tmp_need - buffer);
        strcpy(value, tmp_need+1);

        // 判断是否是需要的键
        if (strcmp(key, findkey) == 0) {
            if (retvalue != nullptr) 
                strcpy(retvalue, value);
            
            strcpy(_conf_value, value);  // 拷贝当前的值到全局的 conf_value 中，用于返回
            return _conf_value;   // 
        }
    }

    return nullptr;
}


/* 除去字符串中不需要的字符 */
bool ClearCharacter(char *src, char ch){
    // ch: 字符
    // int src_len = strlen(src);
    if (strlen(src) == 0 )  // 判断是否是空字符，如果是空字符返回 false
        return false;
    
    // 开始遍历除去指定字符
    for (char *tmp = src; *tmp != '\0'; ++tmp) {
        if (*tmp != ch) {
            *src++ = *tmp;
        }
    }

    *src = '\0';

    return true;
}

bool ClearCharacter(char *src, const char *ch_set){
    // ch_set: 字符集
    // int src_len = strlen(src);
    if (strlen(src) == 0) 
        return false;

    // 开始遍历除去指定字符集
    int ch_set_len = strlen(ch_set);
    int off = false;    // 是否存在字符集中的字符，存在为 true
    for (char *tmp = src; *tmp != '\0'; ++tmp) {
        for (int i = 0; i < ch_set_len; i++) {
            if (*tmp == ch_set[i]) {
                off = true;
                break;
            }
        }

        // 判断当前字符是否是字符集中的字符
        if (!off)  // 如果不是字符集中的字符，则更新
            *src++ = *tmp;
        
        off = false;
    }

    *src = '\0';

    return true;
}


const char *GetConfigPath(const char *dirname, const char *filename) {
    /* 获取配置文件路径 */
    if (strlen(_conf_file_path) != 0) 
        return _conf_file_path;

    // 确定配置文件目录名称
    char conf_dir_name[FILE_NAME_MAX] = {0};
    if (dirname != nullptr) {
        strcpy(conf_dir_name, dirname);
    } else {
        strcpy(conf_dir_name, CONF_DIR_NAME);
    }
    
    // 获取当前路径，也就是bin目录下
    char conf_path[FILENAME_MAX] = {0};
    char *tmp_path = getcwd(conf_path, FILENAME_MAX);
    if (tmp_path == nullptr)
        return nullptr;
    
    // 退回到上级路径下
    tmp_path = strrchr(conf_path, '/');
    if (tmp_path == nullptr)
        return nullptr;
    strncpy(_conf_file_path, conf_path, tmp_path-conf_path);
    DebugPrint("_conf_file_path: %s\n", _conf_file_path);

    // 找到 conf 配置文件目录，可用递归，可用循环，这里选择使用循环遍历
    DIR *dir = nullptr;             // 打开的文件目录
    struct dirent *dp = nullptr;    // 用于遍历目录中的文件
    struct stat st;                 // 判断当前是目录还是文件
    while (tmp_path != nullptr || strlen(_conf_file_path) != 1) {
        // 遍历当前目录下是否有 conf 目录
        strcpy(conf_path, _conf_file_path);
        if (!(dir = opendir(conf_path))) {
            // 目录打开失败
            DebugError("error: open dirent %s is failed.\n", conf_path);
            return nullptr;
        }

        // 打开目录成功，开始遍历该目录
        char cmp_path[FILE_PATH_MAX_LINE]={0};
        while ((dp = readdir(dir)) != nullptr) {
            // 忽略当前目录.和上级目录..
            if ((!strncmp(dp->d_name, ".", 1)) || (!strncmp(dp->d_name, "..", 2))) 
                continue;
            
            snprintf(cmp_path, FILE_PATH_MAX_LINE, "%s/%s", _conf_file_path, dp->d_name);
            DebugPrint("cmp_path: %s\n", cmp_path);
            // 判断是否是目录
            stat(cmp_path, &st);
            if (S_ISDIR(st.st_mode)) {
                // 如果是目录，则检测该目录是否是配置文件目录
                if (strcmp(dp->d_name, conf_dir_name) == 0) {
                    // 表示找到配置文件目录
                    memset(_conf_file_path, 0, FILE_PATH_MAX_LINE);
                    if (filename == nullptr) {
                        snprintf(_conf_file_path, FILE_PATH_MAX_LINE, "%s/%s", cmp_path, CONF_FILE_NAME);
                    } else {
                        snprintf(_conf_file_path, FILE_PATH_MAX_LINE, "%s/%s", cmp_path, filename);
                    }

                    return _conf_file_path;
                }
            }
        }

        // 在当前目录没有找到，就退到上级目录
        closedir(dir);
        tmp_path = strrchr(conf_path, '/');
        memset(_conf_file_path, 0, FILE_PATH_MAX_LINE);
        strncpy(_conf_file_path, conf_path, tmp_path-conf_path);
    }

    return nullptr;
}
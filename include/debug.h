#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "macro.h"

/**
 * 作用: 打印 debug 调试信息
 * user: garteryang
 * 邮箱: 910319432@qq.com
 */

#if defined(DEBUG)
// 导入头文件
#include <stdio.h>

// 定义 Debug 函数
#define DebugPrint(format, ...) { \
    printf("\e[32m"); \
    printf(format, ##__VA_ARGS__);\
    printf("\e[0m");\
} 
#define DebugError(format, ...) { \
    printf("\e[31m");\
    printf(format, ##__VA_ARGS__); \
    printf("\e[0m");\
}

#define DebugPError(format) {\
    printf("\e[31m"); \
    perror(format);\
    printf("\e[0m");\
}

#else
// 否则什么都不做
#define DebugPrint(format, ...)
#define DebugError(format, ...)
#define DebugPError(format)
#endif

#endif // __DEBUG_H__
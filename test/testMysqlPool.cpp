#include "mysqlpool.h"
#include "log.h"
#include "debug.h"


int main() {
    Log::getInstance()->init("httpserver", false);

    MysqlPool::get()->init("120.26.8.149", "root", "990428yy", "test");

    while (1);
    return 0;
}
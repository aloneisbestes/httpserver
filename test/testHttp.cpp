#include "debug.h"
#include "log.h"
#include "http.h"
#include "mysqlpool.h"
#include <string.h>


int main() {
    char text[100];
    strcpy(text, "GET / HTTP/1.1");
    Log::getInstance()->init("httpserver", false);

    MysqlPool::get()->init("120.26.8.149", "root", "990428yy", "test");

    HttpConn http;
    sockaddr_in s;
    http.init(1, s, nullptr, 1, false, "root", "990428yy", "test");
    http.parseRequestLine(text);
    http.parseHeaders("Connection: keep-alive");

    while(1);
    return 0;
}

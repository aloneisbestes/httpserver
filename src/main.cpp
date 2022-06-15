#include "log.h"

bool m_close_log = false;

int main() {

    Log::getInstance()->init("httpserver", false, 8192, 100);
    // Log::getInstance()->init("httpserver", false, 8192, 100, 0, 0);

    for (int i = 0; i < 1000; i++) {
        char buffer[1024];
        sprintf(buffer, "这是第: %d", i+1);
        LogError(buffer);
    }

    while (1);
    return 0;
}
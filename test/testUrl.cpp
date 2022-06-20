#include <iostream>
#include <string.h>

int main() {
    char text[100];
    strcpy(text, "GET / HTTP/1.1");
    const char *m_url = strpbrk(text, " \t");

    text[m_url-text] = '\0';

    char *url = &text[m_url-text+1];

    std::cout << text << std::endl;
    std::cout << strlen(text) << std::endl;

    // const char *tmp = "123";
    // const char *tmp_t = "14";

    url += strspn(url, " \t");
    // std::cout << t << std::endl;
    std::cout << url << std::endl;

    return 0;
}
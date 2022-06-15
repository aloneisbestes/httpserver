#include <iostream>
#include <string.h>

#include "common.h"

using namespace std;


int main() {

    char buffer[1000];
    strcpy(buffer, "    \"log-path\":\"/home/daisy/work/web/httpserver/conf\"");

#if 0
    // 测试两个函数是否正常
    cout << "clear 空格: " << endl;
    cout << buffer << endl;
    ClearCharacter(buffer, ' ');
    cout << buffer << endl;
    
    cout << "clear \": " << endl;
    cout << buffer << endl;
    ClearCharacter(buffer, '\"');
    cout << buffer << endl;
// #endif 

    cout << "clear : " << endl;
    cout << buffer << endl;
    ClearCharacter(buffer, " \",");
    cout << buffer << endl;
#endif 

    // const char *tmp =  ReadConfig("../../conf/httpserver.conf", "log-path");
    // cout << tmp << endl;

    // 测试GetConfigPath
    // GetConfigPath();
    

    return 0;
}
#include <iostream>
#include <exception>

void test() {
    throw std::exception();
}

int main() {

    try {
        test();
    } catch (std::exception &ex) { 
        std::cout << "Exp: " << ex.what() << std::endl;
    }

    return 0;
}

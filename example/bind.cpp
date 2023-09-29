#include <functional>
#include <iostream>
#include <string>


void print(const char *str, int num) {
    std::cout << str << num << std::endl;
}

int main() {
    auto func = std::bind(print, "hello", std::placeholders::_1);
    func(10);
    return 0;
}
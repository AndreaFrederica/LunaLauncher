#include "demo/demo.hpp"
#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        std::puts(demo::version());
        return 0;
    }
    const char* name = (argc >= 2) ? argv[1] : "world";
    std::puts(demo::greet(name));
    return 0;
}

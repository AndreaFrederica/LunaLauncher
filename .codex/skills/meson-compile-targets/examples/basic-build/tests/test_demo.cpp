#include "demo/demo.hpp"
#include <cassert>
#include <cstring>

int main() {
    assert(std::strcmp(demo::greet("Meson"), "Hello, Meson!") == 0);
    assert(std::strcmp(demo::version(), "0.1.0") == 0);
    return 0;
}

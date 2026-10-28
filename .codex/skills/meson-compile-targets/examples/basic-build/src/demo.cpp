#include "demo/demo.hpp"
#include <cstring>

namespace demo {

const char* greet(const char* name) {
    static char buf[256];
    // Avoid snprintf to keep the example self-contained (no libc format dependency).
    const char* prefix = "Hello, ";
    char* p = buf;
    for (const char* s = prefix; *s; ++s) *p++ = *s;
    for (const char* s = name;   *s; ++s) *p++ = *s;
    *p++ = '!';
    *p   = '\0';
    return buf;
}

const char* version() {
    return "0.1.0";
}

} // namespace demo

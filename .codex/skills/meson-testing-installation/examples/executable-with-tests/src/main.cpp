#include "example/core.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view(argv[1]) == "--version") {
    std::cout << example::version() << '\n';
    return 0;
  }

  std::cout << "answer=" << example::answer() << '\n';
  return 0;
}

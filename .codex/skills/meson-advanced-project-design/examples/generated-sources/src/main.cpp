#include "version.hpp"
#include "hello.hpp"
#include "farewell.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view(argv[1]) == "--version") {
    std::cout << GENERATED_VERSION << '\n';
    return 0;
  }

  std::cout << hello_message() << '\n';
  std::cout << farewell_message() << '\n';
  return 0;
}

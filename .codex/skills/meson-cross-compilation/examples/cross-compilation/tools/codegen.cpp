#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 3) {
    return 1;
  }

  std::ifstream in(argv[1]);
  std::ofstream out(argv[2]);
  std::string line;
  std::getline(in, line);
  out << "#pragma once\n";
  out << "static inline const char* GENERATED_TEXT = \"" << line << "\";\n";
  return 0;
}

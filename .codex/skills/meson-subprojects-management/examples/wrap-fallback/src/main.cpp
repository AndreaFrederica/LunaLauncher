#include "wrapdemo/wrapdemo.hpp"
#include "tinydep.hpp"

#include <iostream>

int main() {
  std::cout << wrapdemo::shout(tinydep::meaning()) << '\n';
  return 0;
}

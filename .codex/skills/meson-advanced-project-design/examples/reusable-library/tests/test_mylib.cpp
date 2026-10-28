#include "mylib/mylib.hpp"

#include <cassert>

int main() {
  assert(mylib::add(2, 3) == 5);
  assert(mylib::multiply(4, 5) == 20);
  return 0;
}

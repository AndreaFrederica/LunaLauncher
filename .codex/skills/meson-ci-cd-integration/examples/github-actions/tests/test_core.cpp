#include "gha/core.hpp"

#include <cassert>

int main() {
  assert(gha::meaning() == 42);
  return 0;
}

#include "plugdemo/plugin_api.hpp"

namespace plugdemo {
const char* plugin_name() {
  return "sample_plugin";
}

int plugin_entry() {
  return 7;
}
}

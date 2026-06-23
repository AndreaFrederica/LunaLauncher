#include "plugdemo/plugin_api.hpp"

#include <iostream>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
static void* open_library(const char* path) { return static_cast<void*>(LoadLibraryA(path)); }
static void* load_symbol(void* handle, const char* name) {
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}
#else
#include <dlfcn.h>
static void* open_library(const char* path) { return dlopen(path, RTLD_NOW); }
static void* load_symbol(void* handle, const char* name) { return dlsym(handle, name); }
#endif

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view(argv[1]) == "--help") {
    std::cout << "usage: plughost <plugin-path>\n";
    return 0;
  }

  if (argc < 2) {
    std::cerr << "missing plugin path\n";
    return 1;
  }

  void* handle = open_library(argv[1]);
  if (!handle) {
    std::cerr << "failed to open plugin\n";
    return 1;
  }

  auto* name = reinterpret_cast<const char* (*)()>(load_symbol(handle, "plugin_name"));
  auto* entry = reinterpret_cast<int (*)()>(load_symbol(handle, "plugin_entry"));
  if (!name || !entry) {
    std::cerr << "missing plugin symbols\n";
    return 1;
  }

  std::cout << name() << ": " << entry() << '\n';
  return 0;
}

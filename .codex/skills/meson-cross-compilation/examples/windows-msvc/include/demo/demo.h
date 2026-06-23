#pragma once

#if defined(_WIN32)
  #if defined(DEMO_BUILD)
    #define DEMO_API __declspec(dllexport)
  #else
    #define DEMO_API __declspec(dllimport)
  #endif
#else
  #define DEMO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

DEMO_API int demo_value(void);

#ifdef __cplusplus
}
#endif

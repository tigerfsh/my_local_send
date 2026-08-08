#pragma once

#if defined(_WIN32)
  #if defined(LOCALSEND_CORE_BUILD)
    #define LOCALSEND_API __declspec(dllexport)
  #else
    #define LOCALSEND_API __declspec(dllimport)
  #endif
#else
  #define LOCALSEND_API __attribute__((visibility("default")))
#endif

#define LOCALSEND_VERSION "1.0.0"

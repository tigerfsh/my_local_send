cmake_minimum_required(VERSION 3.5.0)
project(localsend_napi)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# 共享 C++ SDK 源码（嵌入本工程，DevEco 会按目标架构交叉编译）
set(LOCALSEND_SDK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/localsend-sdk/src)
set(LOCALSEND_SDK_INC ${CMAKE_CURRENT_SOURCE_DIR}/localsend-sdk/include)

set(LOCALSEND_SQLITE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sqlite)

# OpenHarmony NDK 不提供可链接的 sqlite3 系统库。
# 需在 cpp/sqlite/ 下放置 SQLite 合并版源码（sqlite3.c + sqlite3.h，来自 https://sqlite.org/amalgamation.html）。
if(EXISTS ${LOCALSEND_SQLITE_DIR}/sqlite3.c AND EXISTS ${LOCALSEND_SQLITE_DIR}/sqlite3.h)
  set(LOCALSEND_SQLITE_SRC ${LOCALSEND_SQLITE_DIR}/sqlite3.c)
  message(STATUS "Using bundled SQLite amalgamation at ${LOCALSEND_SQLITE_DIR}")
else()
  message(FATAL_ERROR
    "sqlite3 amalgamation not found. OpenHarmony NDK has no linkable sqlite3 system library.\n"
    "Please download sqlite3.c + sqlite3.h from https://sqlite.org/amalgamation.html\n"
    "and place them under: ${LOCALSEND_SQLITE_DIR}/")
endif()

add_library(libentry SHARED
  types/napi_init.cpp
  bridge/bridge_common.cpp
  bridge/bridge_core.cpp
  bridge/bridge_storage.cpp
  bridge/bridge_transport.cpp
  ${LOCALSEND_SQLITE_SRC}
  ${LOCALSEND_SDK_SRC}/util.cpp
  ${LOCALSEND_SDK_SRC}/json.cpp
  ${LOCALSEND_SDK_SRC}/crypto.cpp
  ${LOCALSEND_SDK_SRC}/storage.cpp
  ${LOCALSEND_SDK_SRC}/discovery.cpp
  ${LOCALSEND_SDK_SRC}/transport.cpp
  ${LOCALSEND_SDK_SRC}/cache.cpp
  ${LOCALSEND_SDK_SRC}/pairing.cpp
  ${LOCALSEND_SDK_SRC}/core.cpp
)

target_include_directories(libentry PUBLIC
  ${NATIVERENDER_ROOT_PATH}
  ${NATIVERENDER_ROOT_PATH}/types
  ${LOCALSEND_SDK_INC}
  ${LOCALSEND_SDK_SRC}
  ${LOCALSEND_SQLITE_DIR}
)

set_target_properties(libentry PROPERTIES CXX_STANDARD 17)

# libace_napi: NAPI 运行时；libcrypto/libssl: OpenHarmony 系统内置 OpenSSL。
# 注意：SDK 内 core.cpp 使用了 std::filesystem，若目标 OpenHarmony SDK 的 libc++ 不支持，
# 请将 core.cpp/cache.cpp 中的 filesystem 调用替换为 POSIX 实现（见 docs/architecture.md）。
target_link_libraries(libentry PUBLIC libace_napi.z.so libcrypto.z.so libssl.z.so)

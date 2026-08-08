cmake_minimum_required(VERSION 3.20)
project(localsend VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LOCALSEND_BUILD_SDK "Build shared C++ SDK" ON)
option(LOCALSEND_BUILD_TESTS "Build SDK tests" ON)
option(LOCALSEND_BUILD_UBUNTU "Build Ubuntu Qt6 client" ON)

if(LOCALSEND_BUILD_TESTS)
  enable_testing()
endif()

if(LOCALSEND_BUILD_SDK)
  add_subdirectory(sdk)
endif()

if(LOCALSEND_BUILD_UBUNTU AND LOCALSEND_BUILD_SDK)
  find_package(Qt6 QUIET COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia)
  if(Qt6_FOUND)
    add_subdirectory(ubuntu-client)
  else()
    message(STATUS "Qt6 not found, ubuntu-client skipped. Install with: sudo apt install qt6-base-dev qt6-declarative-dev qt6-multimedia-dev")
  endif()
endif()

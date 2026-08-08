add_library(localsend-core STATIC
  src/util.cpp
  src/json.cpp
  src/crypto.cpp
  src/storage.cpp
  src/discovery.cpp
  src/transport.cpp
  src/cache.cpp
  src/pairing.cpp
  src/core.cpp
)

target_include_directories(localsend-core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY sqlite3)
if(NOT SQLITE3_INCLUDE_DIR OR NOT SQLITE3_LIBRARY)
  message(FATAL_ERROR "sqlite3 development files not found")
endif()

target_include_directories(localsend-core PUBLIC ${SQLITE3_INCLUDE_DIR})
target_link_libraries(localsend-core PUBLIC
  OpenSSL::SSL
  OpenSSL::Crypto
  Threads::Threads
  ${SQLITE3_LIBRARY}
)
target_link_libraries(localsend-core PRIVATE ${CMAKE_DL_LIBS})

set_target_properties(localsend-core PROPERTIES
  OUTPUT_NAME localsend-core
  POSITION_INDEPENDENT_CODE ON
)

if(LOCALSEND_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

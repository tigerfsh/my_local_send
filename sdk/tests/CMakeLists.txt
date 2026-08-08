add_executable(test_crypto test_crypto.cpp)
target_link_libraries(test_crypto PRIVATE localsend-core)
add_test(NAME crypto COMMAND test_crypto)

add_executable(test_storage test_storage.cpp)
target_link_libraries(test_storage PRIVATE localsend-core)
add_test(NAME storage COMMAND test_storage)

add_executable(test_discovery test_discovery.cpp)
target_link_libraries(test_discovery PRIVATE localsend-core)
add_test(NAME discovery COMMAND test_discovery)

add_executable(test_core test_core.cpp)
target_link_libraries(test_core PRIVATE localsend-core)
add_test(NAME core COMMAND test_core)

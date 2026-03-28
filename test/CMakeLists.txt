add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE adb::libadb Catch2::Catch2WithMain)
add_subdirectory("targets")

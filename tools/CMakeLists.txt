add_executable(adb adb.cpp)
target_link_libraries(adb PRIVATE adb::libadb PkgConfig::libedit fmt::fmt)

include(GNUInstallDirs)
install(
    TARGETS adb
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

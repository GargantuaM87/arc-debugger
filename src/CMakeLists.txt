add_library(libadb process.cpp pipe.cpp registers.cpp breakpoint_site.cpp)
add_library(adb::libadb ALIAS libadb)

set_target_properties(
    libadb
    PROPERTIES OUTPUT_NAME adb
)

target_compile_features(libadb PUBLIC cxx_std_17)

target_include_directories(libadb
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
)

include(GNUInstallDirs)
install(TARGETS libadb
    EXPORT adb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY $(PROJECT_SOURCE_DIR)/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT adb-targets
    FILE adb-config.cmake
    NAMESPACE adb::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/adb
)

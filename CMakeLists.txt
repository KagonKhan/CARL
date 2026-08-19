cmake_minimum_required(VERSION 3.23)

project(CARL VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)


# =====================    DEPENDENCIES    =======================
find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)


# =======================    PROJECT    ==========================
add_library(carl
    src/config_group.cpp
)

target_compile_features(carl PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(carl PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

target_link_libraries(carl PUBLIC
    yaml-cpp::yaml-cpp
    fmt::fmt
)

target_include_directories(carl PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(carl::carl ALIAS carl)


# =======================    TESTS    ============================
option(CARL_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
if(CARL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


# =======================    INSTALL    ==========================
install(TARGETS carl
    EXPORT carlTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT carlTargets
    FILE carlTargets.cmake
    NAMESPACE carl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/carl
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/carl-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/carl-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/carl
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/carl-config-version.cmake
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/carl-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/carl-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/carl
)

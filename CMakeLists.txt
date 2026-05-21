cmake_minimum_required(VERSION 3.23)

project(CARL VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# =====================    DEPENDENCIES    =======================
find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)


# =======================    PROJECT    ==========================
add_library(carl
    src/config_group.cpp
)

target_compile_features(carl PUBLIC cxx_std_17)

target_link_libraries(carl PUBLIC
    yaml-cpp::yaml-cpp
    fmt::fmt
)

target_include_directories(carl PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_library(carl::carl ALIAS carl)


# =======================    TESTS    ============================
option(CARL_BUILD_TESTS "Build tests" ON)
if(CARL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


# =======================    INSTALL    ==========================
include(GNUInstallDirs)

install(TARGETS carl
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

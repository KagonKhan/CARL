cmake_minimum_required (VERSION 3.23)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (CARL CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# =====================    DEPENDENCIES    =======================
find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)


# =======================    PROJECT    ==========================


add_library(carl
    src/config_value.cpp
)
set_target_properties(carl PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(carl PUBLIC
    yaml-cpp::yaml-cpp
    fmt::fmt
)
target_include_directories(carl PUBLIC include)

add_executable(main main.cpp)
target_link_libraries(main carl)


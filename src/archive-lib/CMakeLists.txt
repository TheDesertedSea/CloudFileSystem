find_package(archive MODULE REQUIRED)

configure_file(path.h.in path.h @ONLY)

add_library(archive-util archive_util.cc)
target_include_directories(archive-util PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(archive-util PUBLIC archive)

add_executable(archive-example archive_example.cc)
# for path.h include to work
target_include_directories(archive-example PRIVATE ${CMAKE_BINARY_DIR}/archive-lib)
target_link_libraries(archive-example PRIVATE archive-util)
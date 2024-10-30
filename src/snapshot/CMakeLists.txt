add_library(snapshot-api INTERFACE)
target_sources(snapshot-api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/snapshot-api.h)
target_include_directories(snapshot-api INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_executable(snapshot snapshot-test.c)
target_link_libraries(snapshot snapshot-api)

find_package(OpenSSL REQUIRED)

add_library(dedup rabinpoly.cc msb.cc)
target_include_directories(dedup PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(dedup PUBLIC -Wall -fPIC)
target_link_libraries(dedup PUBLIC OpenSSL::Crypto OpenSSL::SSL)

add_executable(rabin-example rabin-example.cc)
target_link_libraries(rabin-example dedup)
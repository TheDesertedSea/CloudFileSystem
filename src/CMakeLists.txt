project(amr-tools)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)

# If you add more .cc files, modify this variable to include them
set(cloudfs_srcs
        cloud-lib/cloudapi.cc
        cloud-lib/cloudapi_print.cc
        cloudfs/cloudfs.cc
        cloudfs/main.cc
        cloudfs/util.h
        cloudfs/util.cc
        cloudfs/buffer_file.h
        cloudfs/buffer_file.cc
        cloudfs/chunk_table.h
        cloudfs/chunk_table.cc
        cloudfs/chunk_splitter.h
        cloudfs/chunk_splitter.cc
        cloudfs/cloudfs_controller.h
        cloudfs/cloudfs_controller.cc
        cloudfs/snapshot.h
        cloudfs/snapshot.cc
        cloudfs/cache_replacer.h
        cloudfs/cache_replacer.cc
        )


# add ROOT/cmake to Find module search path
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(cloudfs-libs
        s3
        fuse
        OpenSSL::SSL
        OpenSSL::Crypto)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/archive-lib")
    add_subdirectory(archive-lib)
    list(APPEND cloudfs-libs archive-util)
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dedup-lib")
    add_subdirectory(dedup-lib)
    list(APPEND cloudfs-libs dedup)
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/snapshot")
    add_subdirectory(snapshot)
    list(APPEND cloudfs-libs snapshot-api)
endif ()

# find library s3 in the standard path
find_package(s3 MODULE REQUIRED)
find_package(fuse MODULE REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(cloudfs ${cloudfs_srcs})

# add PROJECT_ROOT/cloud-lib to include path using the build interface generator expression
target_include_directories(cloudfs PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/cloud-lib>)
# FUSE specifically asks for this
target_compile_definitions(cloudfs PRIVATE _FILE_OFFSET_BITS=64 FUSE_USE_VERSION=26)
target_link_libraries(cloudfs ${cloudfs-libs})

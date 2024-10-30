# FindFuse.cmake

# Look for the header file.
find_path(FUSE_INCLUDE_DIR NAMES fuse.h)

# Look for the library.
find_library(FUSE_LIBRARY NAMES fuse)

# Handle the results of the search.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(fuse DEFAULT_MSG FUSE_INCLUDE_DIR FUSE_LIBRARY)

if(FUSE_FOUND)
  set(FUSE_LIBRARIES ${FUSE_LIBRARY})
  set(FUSE_INCLUDE_DIRS ${FUSE_INCLUDE_DIR})
endif()
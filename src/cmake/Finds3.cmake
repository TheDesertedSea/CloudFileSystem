# Finds3.cmake
find_path(S3_INCLUDE_DIR NAMES libs3.h)
find_library(S3_LIBRARY NAMES s3)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(s3 DEFAULT_MSG S3_INCLUDE_DIR S3_LIBRARY)

if(S3_FOUND)
  set(S3_LIBRARIES ${S3_LIBRARY})
  set(S3_INCLUDE_DIRS ${S3_INCLUDE_DIR})
endif()
#include "cloudapi.h"

void cloud_print_error(FILE*) {}

void cloud_print_error()
{
  int statusG = cloud_get_status();

  if (statusG < S3StatusErrorAccessDenied) {
    fprintf(stderr, "Return status: %s\n", S3_get_status_name(static_cast<S3Status>(statusG)));
  }
  else {
    fprintf(stderr, "Return status: %s\n", S3_get_status_name(static_cast<S3Status>(statusG)));
    fprintf(stderr, "%s\n", cloud_get_ErrorDetails());
  }
}

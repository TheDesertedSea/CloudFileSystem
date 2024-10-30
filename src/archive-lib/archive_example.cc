#include <unistd.h>

#include "archive_util.h"

#define LOG_INFO(...) fprintf(stderr, __VA_ARGS__)

void chdir_data_dir();

int main() {
  /*
  You can use `setfattr`/`getfattr`/`removexattr` to set/get extended
  attributes.

  Testing files:

  $ ls -l
  -r--r--r-- 1 student student  16023 Nov 18 04:19 foo_read_only.txt
  -rw-rw-r-- 1 student student  16023 Nov  5 17:24 foo.txt
  ...

  $ getfattr --dump foo.txt
  # file: foo.txt
  user.test="xxx"

  $ getfattr --dump foo_read_only.txt
  # file: foo_read_only.txt
  user.txt="xxx"
  */

  chdir_data_dir();

  /* archive and extract an entire directory */
  {
    LOG_INFO("Running archive_directory() and extract() on ./dir\n");
    const char *input_dir_name = "dir";
    const char *output_tar_name = "res.tar";
    archive_directory(output_tar_name, input_dir_name);
    extract(output_tar_name, "./temp");
  }

  /* archive and extract a single file in current directory */
  {
    LOG_INFO("Running archive_file() and extract() on foo.txt/foo.bar\n");
    const char *input_file_name = "foo.txt";
    const char *output_file_name = "foo.tar";
    archive_file(input_file_name, "./", output_file_name);
    extract(output_file_name, "./temp");
  }

  /* archive and extract a READ-ONLY single file in current directory */
  {
    LOG_INFO("Running archive_file() and extract() on foo_read_only.txt\n");
    const char *input_file_name = "foo_read_only.txt";
    const char *output_file_name = "foo.tar";
    archive_file(input_file_name, "./", output_file_name);
    extract(output_file_name, "./temp");
  }

  /* archive and extract a single file in a nested directory */
  {
    LOG_INFO("Running archive_file() and extract() in ./dir/dir2\n");
    string input_file_name = "./dir/dir2/bar.txt";
    archive_file(input_file_name, "./", "bar.tar");
    extract("bar.tar", "./temp");
  }

  LOG_INFO("Example completed.\n");
}

void chdir_data_dir() {
#include "path.h"
  const char *data_dir = CMAKE_PROJECT_ROOT "/archive-lib/example-data";
  fprintf(stderr, "Changing working dir to: %s\n", data_dir);
  if (chdir(data_dir)) {
    perror("chdir() failed");
    exit(-1);
  }
}
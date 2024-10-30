#include "archive_util.h"

#include <archive.h>
#include <archive_entry.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;
using std::vector;

// archive_files_ - archive files in `filepaths` into `output_path`, with
// respect to `root`. Reference:
// https://github.com/libarchive/libarchive/wiki/Examples#a-basic-write-example
// Use pax format to support extended attributes:
// https://linux.die.net/man/5/libarchive-formats
//
// Example: Consider the following direcory structure.
// .
// └── outer
//     └── inner
//         └── a
//
// Call archive({"./outer/inner/a"}, "./outer", "tar1.tar"). When you untar
// tar1.tar, you get
// .
// └── inner
//     └── a
//
// Call archive({"./outer/inner/a"}, "./outer/inner", "tar2.tar"). When you
// untar tar2.tar, you get
// .
// └── a
static void archive_files_(const vector<string> &filepaths, const string &root,
                           const string &output_path) {
  // Create an archive
  struct archive *a = archive_write_new();

  // Configure to compress with gzip, and include extended attribute
  archive_write_add_filter_gzip(a);
  archive_write_set_format_pax(a);

  // Open to write
  archive_write_open_filename(a, output_path.c_str());

  for (const string &filepath : filepaths) {
    string filepath_wrt_root = filepath;
    if (!root.empty() && filepath.size() >= root.size() &&
        filepath.substr(0, root.size()) == root) {
      filepath_wrt_root = filepath.substr(root.size());
    }

    struct stat st;
    lstat(filepath.c_str(), &st);

    char buff[8192];
    int fd;
    int bytes_read;
    struct archive_entry *entry = archive_entry_new();
    fd = open(filepath.c_str(), O_RDONLY);

    // Write header: handler directory and regular file differently
    switch (st.st_mode & S_IFMT) {
      case S_IFDIR: {
        archive_entry_set_filetype(entry, AE_IFDIR);
        archive_entry_copy_pathname(entry, filepath_wrt_root.c_str());
        archive_entry_set_size(entry, st.st_size);
        archive_entry_set_perm(entry, st.st_mode & 0777);
        archive_write_header(a, entry);

        break;
      }
      case S_IFREG: {
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_copy_pathname(entry, filepath_wrt_root.c_str());

        struct archive *ard = archive_read_disk_new();
        archive_read_disk_set_standard_lookup(ard);
        archive_read_disk_entry_from_file(ard, entry, fd, NULL);

        archive_write_header(a, entry);
        archive_read_free(ard);
        break;
      }
      default:
        std::cout << "Unexpected format: neither directory nor regular file?"
                  << std::endl;
        break;
    }

    // Write data
    while ((bytes_read = read(fd, buff, sizeof(buff))) > 0) {
      archive_write_data(a, buff, bytes_read);
    }

    archive_write_finish_entry(a);

    close(fd);
    archive_entry_free(entry);
  }

  // Finish writing
  archive_write_close(a);
  archive_write_free(a);
}

// List all files recursively under a path.
static void list_files_(const std::string &path, vector<string> &res) {
  // Ensure '/' is added
  string dir_path = path;
  if (dir_path[dir_path.size() - 1] != '/') {
    dir_path += '/';
  }

  static const std::unordered_set<string> skipset(
      {".", "..", ".snapshot", ".cachemeta", ".cache"});
  if (auto dir = opendir(dir_path.c_str())) {
    res.push_back(dir_path);

    while (auto f = readdir(dir)) {
      if (!f->d_name || skipset.find(string(f->d_name)) != skipset.end())
        continue;
      if (f->d_type == DT_REG) res.push_back(dir_path + f->d_name);
      if (f->d_type == DT_DIR) list_files_(dir_path + f->d_name + "/", res);
    }
    closedir(dir);
  }
}

// Archive a single file, including extended attributes
void archive_file(const string &filepath, const string &root,
                  const string &output_path) {
  archive_files_({filepath}, root, output_path);
}

// archive_directory - archive an entire directory
void archive_directory(const char *outname, const string &dirname,
                       string root) {
  if (root.empty()) root = dirname;

  vector<string> filepaths;
  list_files_(dirname, filepaths);
  archive_files_(filepaths, root, outname);
}

static int copy_data_(struct archive *ar, struct archive *aw) {
  int r;
  const void *buff;
  size_t size;
  off_t offset;

  for (;;) {
    r = archive_read_data_block(ar, &buff, &size, &offset);
    if (r == ARCHIVE_EOF) return (ARCHIVE_OK);
    if (r < ARCHIVE_OK) return (r);

    r = archive_write_data_block(aw, buff, size, offset);
    if (r < ARCHIVE_OK) {
      fprintf(stderr, "%s\n", archive_error_string(aw));
      return r;
    }
  }
}

// extract - extract an archive to specified directory
// Reference:
// https://github.com/libarchive/libarchive/wiki/Examples#a-complete-extractor
void extract(const char *filename, const string &d) {
  // As we are concatenating dst to pathname, dst must end with '/'.
  string dst = d;
  if (dst[dst.size() - 1] != '/') dst += '/';

  int r;

  // create for read and configure
  struct archive *a_read = archive_read_new();
  archive_read_support_filter_gzip(
      a_read);  // Enables all available decompression filters.
  archive_read_support_format_all(a_read);  // Enables support for all available
                                            // formats except the "raw" format.
  if ((r = archive_read_open_filename(a_read, filename, 10240))) {
    return;
  }

  // create for write and configure
  struct archive *a_write = archive_write_disk_new();
  int flags = ARCHIVE_EXTRACT_TIME;
  flags |= ARCHIVE_EXTRACT_PERM;
  flags |= ARCHIVE_EXTRACT_FFLAGS;
  flags |= ARCHIVE_EXTRACT_XATTR;  // Extract a_writeended attribute
  archive_write_disk_set_options(a_write, flags);
  archive_write_disk_set_standard_lookup(a_write);

  std::unordered_map<std::string, mode_t> true_modes;
  struct archive_entry *entry;
  for (;;) {
    r = archive_read_next_header(a_read, &entry);
    if (r == ARCHIVE_EOF) break;
    if (r < ARCHIVE_OK) printf("%s\n", archive_error_string(a_read));
    if (r < ARCHIVE_WARN) return;

    // Change output directory: https://stackoverflow.com/a/25991813/9057530
    const char *curr_file = archive_entry_pathname(entry);
    const std::string full_output_path = dst + curr_file;
    archive_entry_set_pathname(entry, full_output_path.c_str());

    // Handle edge case: https://github.com/libarchive/libarchive/issues/497
    // When file is read only, libarchive cannot set its extended attribute.
    // The trick is to set the file writable here, and restore the true mode
    // later.
    mode_t true_mode = archive_entry_perm(entry);
    true_modes[full_output_path] = true_mode;
    archive_entry_set_perm(entry, 0777);

    r = archive_write_header(a_write, entry);
    if (r < ARCHIVE_OK) {
      printf("%s\n", archive_error_string(a_write));
    } else if (archive_entry_size(entry) > 0) {
      r = copy_data_(a_read, a_write);
      if (r < ARCHIVE_OK) printf("%s\n", archive_error_string(a_write));
      if (r < ARCHIVE_WARN) return;
    }

    r = archive_write_finish_entry(a_write);
    if (r < ARCHIVE_OK) printf("%s\n", archive_error_string(a_write));
    if (r < ARCHIVE_WARN) return;
  }

  archive_read_close(a_read);
  archive_read_free(a_read);

  archive_write_close(a_write);
  archive_write_free(a_write);

  for (auto it : true_modes) {
    auto path = it.first;
    auto mode = it.second;
    assert(chmod(path.c_str(), mode) == 0);
  }
}
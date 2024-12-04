#include "util.h"

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <fstream>

void debug_print(const std::string &msg, FILE *file) {
  fprintf(file, "%s\n", msg.c_str());
}

std::string generate_object_key(const std::string &path) {
  std::string key = path;
  for (auto &c : key) {
    if (c == '/') {
      c = '_';
    }
  }
  return key;
}

std::string main_path_to_buffer_path(const std::string &main_path) {
  std::string buffer_path = main_path;
  buffer_path.insert(buffer_path.find_last_of("/") + 1, ".");
  return buffer_path;
}

bool is_buffer_path(const std::string &path) {
  if (path == "." || path == "..") {
    return false;
  }
  return path[0] == '.';
}

int tar_file(const std::string &tar_path, const std::string &file_path) {
  struct archive *a;
  struct archive_entry *entry;
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return -1;
  }

  a = archive_write_new();
  if (!a) {
    return -1;
  }
  archive_write_set_compression_gzip(a);
  archive_write_set_format_ustar(a);
  if (archive_write_open_filename(a, tar_path.c_str()) != ARCHIVE_OK) {
    archive_write_free(a);
    return -1;
  }
  entry = archive_entry_new();
  archive_entry_set_pathname(entry, file_path.c_str());
  file.seekg(0, std::ios::end);
  archive_entry_set_size(entry, file.tellg());
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0777);
  archive_write_header(a, entry);

  file.seekg(0, std::ios::beg);
  char buffer[MEM_BUFFER_LEN];
  while (file.read(buffer, sizeof(buffer))) {
    archive_write_data(a, buffer, file.gcount());
  }

  archive_write_data(a, buffer, file.gcount());
  archive_entry_free(entry);
  archive_write_close(a);
  archive_write_free(a);
  return 0;
}

int untar_file(const std::string &tar_path, const std::string &dir_path) {
  struct archive *a;
  struct archive *ext;
  struct archive_entry *entry;

  a = archive_read_new();
  archive_read_support_format_tar(a);
  archive_read_support_filter_gzip(a);
  if (archive_read_open_filename(a, tar_path.c_str(), MEM_BUFFER_LEN) !=
      ARCHIVE_OK) {
    return -1;
  }

  ext = archive_write_disk_new();
  archive_write_disk_set_options(
      ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
               ARCHIVE_EXTRACT_FFLAGS);

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    const char *entry_path = archive_entry_pathname(entry);
    std::string full_path = dir_path + "/" + entry_path;

    archive_entry_set_pathname(entry, full_path.c_str());

    if (archive_write_header(ext, entry) != ARCHIVE_OK) {
      return -1;
    }

    const void *buff;
    size_t size;
    la_int64_t offset;

    while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
      if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
        return -1;
      }
    }
  }

  archive_write_close(ext);
  archive_write_free(ext);
  archive_read_close(a);
  archive_read_free(a);
  return 0;
}

DebugLogger::DebugLogger(const std::string &log_path) {
  file_ = fopen(log_path.c_str(), "w");
  setvbuf(file_, NULL, _IOLBF, 0);
}

DebugLogger::~DebugLogger() { fclose(file_); }

int DebugLogger::error(const std::string &error_str) {
  debug_print("[CloudFS Error] " + error_str +
                  ", errno = " + std::to_string(errno),
              file_);
  return -errno;
}

void DebugLogger::info(const std::string &info_str) {
  debug_print("[CloudFS Info] " + info_str, file_);
}

void DebugLogger::debug(const std::string &debug_str) {
  debug_print("[CloudFS Debug] " + debug_str, file_);
}

FILE *DebugLogger::get_file() { return file_; }

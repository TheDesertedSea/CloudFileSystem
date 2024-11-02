#include "util.h"

#include <cstdio>
#include <unistd.h>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>

#define PATH_MAX 4096



void debug_print(const std::string& msg, FILE* file) {
    fprintf(file, "%s\n", msg.c_str());
}

std::vector<std::string> get_absolute_path(const std::string& path) {
  std::vector<std::string> path_segments = split_path(path);
  path_segments = normalize_path(path_segments);

  std::vector<std::string> absolute_path;
  if(path[0] != '/') {
    char cwd[PATH_MAX];
    if(getcwd(cwd, PATH_MAX) == NULL) {
      perror("getcwd");
      exit(1);
    }
    absolute_path = split_path(cwd);
  }

  for(auto& seg : path_segments) {
    absolute_path.push_back(seg);
  }

  return absolute_path;
}

std::vector<std::string> split_path(const std::string& path) {
  std::vector<std::string> path_segments;
  std::string path_segment;
  std::stringstream path_stream(path);

  if(path[0] == '/') {
    path_segments.push_back("/");
  }

  while(std::getline(path_stream, path_segment, '/')) {
    if(path_segment.length() > 0) {
      path_segments.push_back(path_segment);
    }
  }

  return path_segments;
}

std::vector<std::string> normalize_path(const std::vector<std::string>& path_segments) {
  std::vector<std::string> normalized_path;
  for(auto& seg : path_segments) {
    if(seg == "..") {
      if(normalized_path.size() > 0) {
        normalized_path.pop_back();
      }
    } else if(seg != ".") {
      normalized_path.push_back(seg);
    }
  }

  return normalized_path;
}

std::string get_metadata_path(const std::string& path) {
  auto filename_start = path.find_last_of("/") + 1;
  auto path_head = path.substr(0, filename_start);
  return path_head + "." + path.substr(filename_start);
}

std::string generate_object_key(const std::string& path) {
  std::string key = path;
  for(auto& c : key) {
    if(c == '/') {
      c = '_';
    }
  }
  return key;
}

std::string get_temp_file_path(const std::string& path) {
  return path + ".temp";
}

std::string get_ssd_path(const std::string& proxy_path) {
  return proxy_path.substr(0, proxy_path.length() - 1);
}

int save_time(const std::string& path, timespec tv[]) {
    struct stat stat_buf;
    lstat(path.c_str(), &stat_buf);
    tv[0] = stat_buf.st_atim;
    tv[1] = stat_buf.st_mtim;
    return 0;
}

int restore_time(const std::string& path, const timespec tv[]) {
    utimensat(AT_FDCWD, path.c_str(), tv, 0);
    return 0;
}

#include "util.h"

#define PATH_MAX 4096

#include <unistd.h>
#include <iostream>
#include <sstream>

void debug_print(const std::string& msg) {
    std::cerr << msg << std::endl;
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

std::string get_proxy_path(const std::string& ssd_path) {
  auto filename_start = ssd_path.find_last_of("/") + 1;
  auto path_head = ssd_path.substr(0, filename_start);
  return path_head + "." + ssd_path.substr(filename_start);
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

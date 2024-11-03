#include "util.h"

void debug_print(const std::string& msg, FILE* file) {
    fprintf(file, "%s\n", msg.c_str());
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

std::string get_data_path(const std::string& path) {
  // '/a/b/c/data' -> '/a/b/c/.data'
  std::string data_path = path;
  data_path.insert(data_path.find_last_of("/") + 1, ".");
  return data_path;
}

bool is_data_path(const std::string& path) {
  if(path == "." || path == "..") {
    return false;
  }
  return path[0] == '.';
}

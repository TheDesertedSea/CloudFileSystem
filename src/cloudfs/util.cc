#include "util.h"

void debug_print(const std::string& msg, FILE* file) {
    fprintf(file, "%s\n", msg.c_str());
    setvbuf(file, NULL, _IOLBF, 0);
}

std::string generate_object_key(const std::string& path) {
  std::string key = path;
  for(auto& c : key) {
    if(c == '/') {
      c = '_';
    } else if(c == '.') {
      c = '+';
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

std::string main_path_to_buffer_path(const std::string& main_path) {
  // '/a/b/c/main' -> '/a/b/c/.main'
  std::string buffer_path = main_path;
  buffer_path.insert(buffer_path.find_last_of("/") + 1, ".");
  return buffer_path;
}

bool is_data_path(const std::string& path) {
  if(path == "." || path == "..") {
    return false;
  }
  return path[0] == '.';
}

bool is_buffer_path(const std::string& path) {
  if(path == "." || path == "..") {
    return false;
  }
  return path[0] == '.';
}

DebugLogger::DebugLogger(const std::string& log_path){
  file_ = fopen(log_path.c_str(), "w");
  /* To ensure that a log of content is not buffered */
  setvbuf(file_, NULL, _IOLBF, 0);
}

DebugLogger::~DebugLogger() {
  fclose(file_);
}

int DebugLogger::error(const std::string& error_str) {
  debug_print("[CloudFS Error] " +  error_str + ", errno = " + std::to_string(errno), file_);
  return -errno;
}

void DebugLogger::info(const std::string& info_str) {
  debug_print("[CloudFS Info] " + info_str, file_);
}

void DebugLogger::debug(const std::string& debug_str) {
  debug_print("[CloudFS Debug] " + debug_str, file_);
}

FILE* DebugLogger::get_file() {
  return file_;
}

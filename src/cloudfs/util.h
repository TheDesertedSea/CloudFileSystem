#pragma once

#include <cstdio>
#include <string>
#include <stdio.h>
#include <sys/stat.h>

void debug_print(const std::string& msg, FILE* file = stderr);

std::string generate_object_key(const std::string& path);

std::string get_data_path(const std::string& path);

std::string main_path_to_buffer_path(const std::string& main_path);

bool is_data_path(const std::string& path);

bool is_buffer_path(const std::string& path);

int tar_file(const std::string& tar_path, const std::string& file_path);

int untar_file(const std::string& tar_path, const std::string& dir_path);

class DebugLogger{
  FILE* file_;
public:
  DebugLogger(const std::string& log_path);
  ~DebugLogger();
  int error(const std::string& error_str);
  void info(const std::string& info_str);
  void debug(const std::string& debug_str);

  FILE* get_file();
};

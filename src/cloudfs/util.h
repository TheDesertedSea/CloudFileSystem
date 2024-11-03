#pragma once

#include <string>
#include <stdio.h>
#include <sys/stat.h>

void debug_print(const std::string& msg, FILE* file = stderr);

std::string generate_object_key(const std::string& path);

std::string get_data_path(const std::string& path);

bool is_data_path(const std::string& path);

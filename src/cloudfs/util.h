#pragma once

#include <string>
#include <vector>

void debug_print(const std::string& msg);

// Returns the absolute path segments of the given path
std::vector<std::string> get_absolute_path(const std::string& path);

// Splits a path string into its segments
std::vector<std::string> split_path(const std::string& path);

// Normalizes a path by removing ".." and "." segments
std::vector<std::string> normalize_path(const std::vector<std::string>& path_segments);

std::string get_proxy_path(const std::string& ssd_path);

std::string generate_object_key(const std::string& path);

std::string get_temp_file_path(const std::string& path);

std::string get_ssd_path(const std::string& proxy_path);
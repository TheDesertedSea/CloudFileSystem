#pragma once

#include <cstddef>
#include <string>

void download_file(const std::string& path, const std::string& bucket_name, const std::string& object_key);

void upload_file(const std::string& path, const std::string& bucket_name, const std::string& object_key, size_t size);
#pragma once

#include <string>

void download_data(const std::string& path, const std::string& bucket_name, const std::string& object_key);
void upload_data(const std::string& path, const std::string& bucket_name, const std::string& object_key, size_t size);
#pragma once

#include <string>

void data_service_init(const std::string& hostname, const std::string& bucket_name);

void download_data(const std::string& path, const std::string& bucket_name, const std::string& object_key);
void upload_data(const std::string& path, const std::string& bucket_name, const std::string& object_key, size_t size);
void delete_data(const std::string& bucket_name, const std::string& object_key);

void data_service_destroy();

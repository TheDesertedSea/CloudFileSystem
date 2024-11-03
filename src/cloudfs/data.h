#pragma once

#include <string>

void data_service_init(const std::string& hostname, const std::string& bucket_name);
void data_service_destroy();

int download_data(const std::string& path, const std::string& bucket_name, const std::string& object_key);
int upload_data(const std::string& path, const std::string& bucket_name, const std::string& object_key, size_t size);
int clear_local(const std::string& path);
int persist_data(const std::string& data_path, const std::string& path);
int delete_object(const std::string& bucket_name, const std::string& object_key);

int delete_data_file(const std::string& path);

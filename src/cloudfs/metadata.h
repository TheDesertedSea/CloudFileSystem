#pragma once

#include <string>
#include <time.h>
#include <sys/types.h>

#define SIZE_NAME "user.cloudfs.size"
#define ON_CLOUD_NAME "user.cloudfs.on_cloud"
#define DIRTY_NAME "user.cloudfs.dirty"

constexpr size_t SIZE_LEN = sizeof(size_t);
constexpr size_t ON_CLOUD_LEN = sizeof(char);
constexpr size_t DIRTY_LEN = sizeof(char);

int get_size(const std::string& path, off_t& size);
int set_size(const std::string& path, off_t size);

int is_on_cloud(const std::string& path, bool& on_cloud);
int set_on_cloud(const std::string& path, bool on_cloud);

int get_timestamps(const std::string& path, timespec tv[]);
int set_timestamps(const std::string& path, const timespec tv[]);

int is_dirty(const std::string& path, bool& dirty);
int set_dirty(const std::string& path, bool dirty);

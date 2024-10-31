#pragma once

#include <string>

constexpr size_t METADATA_SIZE = sizeof(size_t) + sizeof(bool);

struct Metadata {
    size_t size;
    bool is_on_cloud;
};

int get_metadata(const std::string& path, Metadata& metadata);
int set_metadata(const std::string& path, const Metadata& metadata);


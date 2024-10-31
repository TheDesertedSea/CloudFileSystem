#pragma once

#include <fstream>
#include <string>

struct Metadata {
    size_t size;
    bool is_on_cloud;
};

std::string serialize_metadata(const Metadata& metadata);
Metadata deserialize_metadata(std::fstream& metadata_file);

Metadata get_metadata(const std::string& path);
void set_metadata(const std::string& path, const Metadata& metadata);


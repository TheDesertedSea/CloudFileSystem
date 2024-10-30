#pragma once

#include <fstream>
#include <sys/stat.h>
#include <string>
#include <vector>

struct ExtendedAttr{
    std::string attr_name;
    std::string attr_value;
};

struct ProxyMetadata {
    struct stat st;
    std::vector<ExtendedAttr> xattrs;
};

inline bool is_proxy_file(const std::string& proxy_path);

std::string serialize_metadata(const ProxyMetadata& metadata);
ProxyMetadata deserialize_metadata(std::fstream& proxy_file);

ProxyMetadata get_proxy_metadata(const std::string& proxy_path);
void set_proxy_metadata(const std::string& proxy_path, const ProxyMetadata& metadata);


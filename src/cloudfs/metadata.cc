#include "metadata.h"
#include <unistd.h>
#include <fstream>

inline bool is_proxy_file(const std::string& proxy_path) {
    return access(proxy_path.c_str(), F_OK) == 0;
}

ProxyMetadata get_proxy_metadata(const std::string& proxy_path) {
    std::fstream proxy_file(proxy_path, std::ios::in);
    return deserialize_metadata(proxy_file);
}

void set_proxy_metadata(const std::string& proxy_path, const ProxyMetadata& metadata) {
    std::fstream proxy_file(proxy_path, std::ios::out | std::ios::trunc);
    proxy_file << serialize_metadata(metadata);
}

std::string serialize_metadata(const ProxyMetadata& metadata) {
    std::string serialized_stat(reinterpret_cast<const char*>(&metadata.st), sizeof(metadata.st));

    std::string serialized_xattrs;
    for (const auto& xattr : metadata.xattrs) {
        serialized_xattrs += xattr.attr_name + '\t' + xattr.attr_value + '\n';
    }

    return serialized_stat + serialized_xattrs;
}

ProxyMetadata deserialize_metadata(std::fstream& proxy_file) {
    ProxyMetadata metadata;
    proxy_file.read(reinterpret_cast<char*>(&metadata.st), sizeof(metadata.st));

    std::string line;
    while (std::getline(proxy_file, line)) {
        size_t tab_pos = line.find('\t');
        if (tab_pos == std::string::npos) {
            // Invalid line
            continue;
        }

        ExtendedAttr xattr;
        xattr.attr_name = line.substr(0, tab_pos);
        xattr.attr_value = line.substr(tab_pos + 1);
        metadata.xattrs.push_back(xattr);
    }

    return metadata;
}
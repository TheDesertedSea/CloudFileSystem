#include "metadata.h"

#include <fstream>

Metadata get_metadata(const std::string& path) {
    std::fstream proxy_file(path, std::ios::in);
    return deserialize_metadata(proxy_file);
}

void set_metadata(const std::string& path, const Metadata& metadata) {
    std::fstream metadata_file(path, std::ios::out | std::ios::trunc);
    metadata_file << serialize_metadata(metadata);
}

std::string serialize_metadata(const Metadata& metadata) {
    std::string serialized_data;
    serialized_data += std::to_string(metadata.size) + "\n";
    serialized_data += std::to_string(metadata.is_on_cloud) + "\n";
    return serialized_data;
}

Metadata deserialize_metadata(std::fstream& metadata_file) {
    Metadata metadata;
    std::string line;

    if (std::getline(metadata_file, line)) {
        metadata.size = std::stoul(line);
    }
    if (std::getline(metadata_file, line)) {
        metadata.is_on_cloud = static_cast<bool>(std::stoi(line));
    }

    return metadata;
}
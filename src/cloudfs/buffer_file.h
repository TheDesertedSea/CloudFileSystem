#pragma once

#include <cstddef>
#include <string>
#include <sys/types.h>
#include <memory>
#include <vector>

#include "util.h"

class BufferController {
    std::string bucket_name_;
    std::shared_ptr<DebugLogger> logger_;
    static FILE* outfile;
    static FILE* infile;
    static std::vector<std::string> bucket_list;

public:
    BufferController(const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger);
    ~BufferController();

    int download_chunk(const std::string& key, const std::string& buffer_path, off_t offset);
    int upload_chunk(const std::string& key, const std::string& buffer_path, off_t offset, size_t size);
    int download_file(const std::string& key, const std::string& buffer_path);
    int upload_file(const std::string& key, const std::string& buffer_path, size_t size);
    int clear_file(const std::string& buffer_path);
    int delete_object(const std::string& key);

private:
    static int get_buffer(const char* buffer, int len) {
        return fwrite(buffer, 1, len, outfile);
    }

    static int put_buffer(char* buffer, int len) {
        return fread(buffer, 1, len, infile);
    }

    static int list_service(const char* bucketName) {
        bucket_list.push_back(bucketName);
        return 0;
    }
};

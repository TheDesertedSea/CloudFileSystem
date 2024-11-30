#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <unistd.h>

#include "cache_replacer.h"
#include "util.h"
#include "cloudfs.h"

class BufferController {
    std::string bucket_name_;
    std::shared_ptr<DebugLogger> logger_;
    static FILE* outfile;
    static FILE* infile;
    static int64_t outfd;
    static int64_t infd;
    static off_t out_offset;
    static off_t in_offset;
    static std::vector<std::string> bucket_list;
    static std::vector<std::pair<std::string, uint64_t>> object_list;

    struct CachedObject {
        size_t size_;
        bool dirty_;

        CachedObject() : size_(0), dirty_(false) {}
        CachedObject(size_t size, bool dirty) : size_(size), dirty_(dirty) {}
    };

    std::unordered_map<std::string, CachedObject> cached_objects;
    int64_t cache_size_;
    int64_t cache_used_;
    std::string cache_root_;

    std::shared_ptr<CacheReplacer> cache_replacer_;

public:
    BufferController(struct cloudfs_state* state, std::string bucket_name, std::shared_ptr<DebugLogger> logger);
    ~BufferController();

    int download_chunk(const std::string& key, uint64_t fd, off_t offset, size_t size);
    int upload_chunk(const std::string& key, uint64_t fd, off_t offset, size_t size);
    int download_file(const std::string& key, const std::string& buffer_path);
    int upload_file(const std::string& key, const std::string& buffer_path, size_t size);
    int clear_file(const std::string& buffer_path);
    int clear_file(uint64_t fd);
    int delete_object(const std::string& key);

    int persist_cache_state();
    void PrintCache();

private:
    static int get_buffer(const char* buffer, int len) {
        return fwrite(buffer, 1, len, outfile);
    }

    static int get_buffer_fd(const char* buffer, int len) {
        auto ret = pwrite(outfd, buffer, len, out_offset);
        out_offset += len;
        return ret;
    }

    static int put_buffer(char* buffer, int len) {
        return fread(buffer, 1, len, infile);
    }

    static int put_buffer_fd(char* buffer, int len) {
        auto ret = pread(infd, buffer, len, in_offset);
        in_offset += len;
        return ret;
    }

    static int list_service(const char* bucketName) {
        bucket_list.push_back(bucketName);
        return 0;
    }

    static int list_bucket_filler(const char *key, time_t modified_time, uint64_t size) {
        object_list.push_back(std::make_pair(key, size));
        return 0;
    }

    int evict_to_size(size_t required_size);

};

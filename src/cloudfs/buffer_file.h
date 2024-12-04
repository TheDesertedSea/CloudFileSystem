/**
 * @file buffer_file.h
 * @brief Controller for operating buffer files and cloud objects
 * @author Cundao Yu <cundaoy@andrew.cmu.edu>
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "cache_replacer.h"
#include "cloudfs.h"
#include "util.h"

/**
 * Controller for operating buffer files and cloud objects
 * 
 * CloudFS creates a buffer file for each user file to store small file contents
 * or to buffer cloud objects for operating large files.
 * 
 * BufferFileController provides APIs to download/upload objects from/to cloud.
 * It also has a cache to store objects that are frequently accessed. It uses
 * a cache replacer to record the access history and choose objects to evict.
 */
class BufferFileController {

  std::string bucket_name_;             // bucket name
  std::shared_ptr<DebugLogger> logger_; // logger
  static FILE *outfile_;                // file pointer for download
  static FILE *infile_;                 // file pointer for upload
  static int64_t outfd_;                // file descriptor for download
  static int64_t infd_;                 // file descriptor for upload
  static off_t out_offset_;             // offset for download
  static off_t in_offset_;              // offset for upload
  static std::vector<std::string>
      bucket_list_; // tmp bucket list for list service
  static std::vector<std::pair<std::string, uint64_t>>
      object_list_; // tmp object list for list service

  /**
   * Cached object
   */
  struct CachedObject {
    size_t size_; // size of the object
    bool dirty_;  // true if the object hasn't been written back to cloud

    CachedObject() : size_(0), dirty_(false) {}
    CachedObject(size_t size, bool dirty) : size_(size), dirty_(dirty) {}
  };

  std::unordered_map<std::string, CachedObject>
      cached_objects_;     // cached objects
  int64_t cache_size_;     // cache size
  int64_t cache_used_;     // cache used
  std::string cache_root_; // cache root

  std::shared_ptr<CacheReplacer> cache_replacer_; // cache replacer

public:
  /**
   * Constructor
   * @param state cloudfs state
   * @param bucket_name bucket name
   * @param logger logger
   */
  BufferFileController(struct cloudfs_state *state, std::string bucket_name,
                   std::shared_ptr<DebugLogger> logger);

  /**
   * Destructor
   */
  ~BufferFileController();

  /**
   * Download a chunk of data from cloud to buffer file at the given offset
   * @param key object key
   * @param fd file descriptor
   * @param offset offset
   * @param size size
   * @return 0 on success, negative errno on failure
   */
  int download_chunk(const std::string &key, uint64_t fd, off_t offset,
                     size_t size);

  /**
   * Upload a chunk of data from buffer file to cloud at the given offset
   * @param key object key
   * @param fd file descriptor
   * @param offset offset
   * @param size size
   * @return 0 on success, negative errno on failure
   */
  int upload_chunk(const std::string &key, uint64_t fd, off_t offset,
                   size_t size);

  /**
   * Download a file from cloud to buffer file
   * @param key object key
   * @param buffer_path buffer file path
   * @return 0 on success, negative errno on failure
   */
  int download_file(const std::string &key, const std::string &buffer_path);

  /**
   * Upload a file from buffer file to cloud
   * @param key object key
   * @param buffer_path buffer file path
   * @param size size
   * @return 0 on success, negative errno on failure
   */
  int upload_file(const std::string &key, const std::string &buffer_path,
                  size_t size);

  /**
   * Clear a buffer file
   * @param buffer_path buffer file path
   * @return 0 on success, negative errno on failure
   */
  int clear_file(const std::string &buffer_path);

  /**
   * Clear a buffer file by file descriptor
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  int clear_file(uint64_t fd);

  /**
   * Delete an object from cloud
   * @param key object key
   * @return 0 on success, negative errno on failure
   */
  int delete_object(const std::string &key);

  /**
   * Persist the cache state to cloud
   * @return 0 on success, negative errno on failure
   */
  int persist_cache_state();

  /**
   * Print the cache state
   */
  void print_cache();

private:
  static int get_buffer(const char *buffer, int len) {
    return fwrite(buffer, 1, len, outfile_);
  }

  static int get_buffer_fd(const char *buffer, int len) {
    auto ret = pwrite(outfd_, buffer, len, out_offset_);
    out_offset_ += len;
    return ret;
  }

  static int put_buffer(char *buffer, int len) {
    return fread(buffer, 1, len, infile_);
  }

  static int put_buffer_fd(char *buffer, int len) {
    auto ret = pread(infd_, buffer, len, in_offset_);
    in_offset_ += len;
    return ret;
  }

  static int list_service(const char *bucketName) {
    bucket_list_.push_back(bucketName);
    return 0;
  }

  static int list_bucket_filler(const char *key, time_t modified_time,
                                uint64_t size) {
    object_list_.push_back(std::make_pair(key, size));
    return 0;
  }

  /**
   * Evict objects to free up space
   * @param required_size required size
   * @return 0 on success, negative errno on failure
   */
  int evict_to_size(size_t required_size);
};

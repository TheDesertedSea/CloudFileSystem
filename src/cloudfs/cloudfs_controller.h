#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <memory>
#include <unordered_map>

#include "chunk_splitter.h"
#include "chunk_table.h"
#include "cloudfs.h"
#include "util.h"
#include "buffer_file.h"

class CloudfsController {
  protected:
    struct OpenFile {
      std::string main_path_;
      off_t start_;
      size_t len_;
      bool is_dirty_;
      std::vector<Chunk> chunks_;

      OpenFile() = default;
      OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty) {}
      OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty, std::vector<Chunk> chunks)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty), chunks_(std::move(chunks)) {}
    };

    struct cloudfs_state *state_;
    std::string bucket_name_;
    std::shared_ptr<DebugLogger> logger_;
    std::unordered_map<uint64_t, OpenFile> open_files_;
    std::shared_ptr<BufferController> buffer_controller_;

  public:
    CloudfsController(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger);
    ~CloudfsController();

    int stat_file(const std::string& path, struct stat* stbuf);

    int create_file(const std::string& path, mode_t mode);

    virtual int open_file(const std::string& path, int flags, uint64_t* fd) = 0;

    virtual int read_file(const std::string& path, uint64_t fd, char* buf, size_t size, off_t offset) = 0;

    virtual int write_file(const std::string& path, uint64_t fd, const char* buf, size_t size, off_t offset) = 0;

    virtual int close_file(const std::string& path, uint64_t fd) = 0;

    virtual int unlink_file(const std::string& path) = 0;

    virtual int truncate_file(const std::string& path, off_t size) = 0;
  protected:
    int get_buffer_path(const std::string& path, std::string& buffer_path);

    int set_buffer_path(const std::string& path, const std::string& buffer_path);

    int get_size(const std::string& path, off_t& size);

    int get_size(uint64_t fd, off_t& size);

    int set_size(const std::string& path, off_t size);

    int set_size(uint64_t fd, off_t size);

    int get_is_on_cloud(const std::string& path, bool& is_on_cloud);

    int set_is_on_cloud(const std::string& path, bool is_on_cloud);

    int set_chunkinfo(const std::string& main_path, std::vector<Chunk>& chunks);

    int get_chunkinfo(const std::string& main_path, std::vector<Chunk>& chunks);

    int get_chunk_idx(const std::vector<Chunk>& chunks, off_t offset);

};

class CloudfsControllerNoDedup : public CloudfsController {
  public:
    CloudfsControllerNoDedup(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger);
    ~CloudfsControllerNoDedup();

    int open_file(const std::string& path, int flags, uint64_t* fd) override;

    int read_file(const std::string& path, uint64_t fd, char* buf, size_t size, off_t offset) override;

    int write_file(const std::string& path, uint64_t fd, const char* buf, size_t size, off_t offset) override;

    int close_file(const std::string& path, uint64_t fd) override;

    int unlink_file(const std::string& path) override;

    int truncate_file(const std::string& path, off_t size) override;
};

class CloudfsControllerDedup : public CloudfsController {

  ChunkTable chunk_table_;
  ChunkSplitter chunk_splitter_;
  static const size_t BUFFER_SIZE_MAX;

  public:
    CloudfsControllerDedup(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, 
      std::shared_ptr<DebugLogger> logger, int window_size, int avg_seg_size, int min_seg_size, int max_seg_size);
    ~CloudfsControllerDedup();

    int open_file(const std::string& path, int flags, uint64_t* fd) override;

    int read_file(const std::string& path, uint64_t fd, char* buf, size_t size, off_t offset) override;

    int write_file(const std::string& path, uint64_t fd, const char* buf, size_t size, off_t offset) override;

    int close_file(const std::string& path, uint64_t fd) override;

    int unlink_file(const std::string& path) override;

    int truncate_file(const std::string& path, off_t size) override;
  
  private:
    int prepare_read_data(const std::string& buffer_path, off_t offset, off_t& start, size_t& buffer_len, std::vector<Chunk>& chunks);
    int prepare_write_data(const std::string& buffer_path, off_t offset, off_t& start, size_t& buffer_len, std::vector<Chunk>& chunks, size_t write_size);
};

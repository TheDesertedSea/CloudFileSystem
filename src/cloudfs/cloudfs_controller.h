#pragma once

#include <cstdint>
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
      uint64_t op_fd_;

      OpenFile() = default;
      OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty) {}
      OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty, std::vector<Chunk> chunks)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty), chunks_(std::move(chunks)) {}
      OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty, std::vector<Chunk> chunks, uint64_t op_fd)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty), chunks_(std::move(chunks)), op_fd_(op_fd) {}
    };


    struct cloudfs_state *state_;
    std::string bucket_name_;
    std::shared_ptr<DebugLogger> logger_;
    std::unordered_map<uint64_t, OpenFile> open_files_;
    std::shared_ptr<BufferController> buffer_controller_;
    ChunkTable chunk_table_;

  public:
    CloudfsController(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger);
    virtual ~CloudfsController();

    std::shared_ptr<BufferController> get_buffer_controller() {
      return buffer_controller_;
    }

    int stat_file(const std::string& path, struct stat* stbuf);

    int create_file(const std::string& path, mode_t mode);

    virtual int open_file(const std::string& path, int flags, uint64_t* fd) = 0;

    virtual int read_file(const std::string& path, uint64_t fd, char* buf, size_t size, off_t offset) = 0;

    virtual int write_file(const std::string& path, uint64_t fd, const char* buf, size_t size, off_t offset) = 0;

    virtual int close_file(const std::string& path, uint64_t fd) = 0;

    virtual int unlink_file(const std::string& path) = 0;

    virtual int truncate_file(const std::string& path, off_t size) = 0;

    virtual void destroy() = 0;

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

    int set_truncated(const std::string& path, bool truncated);

    int get_truncated(const std::string& path, bool& truncated);

    ChunkTable& get_chunk_table() {
      return chunk_table_;
    }

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

    void destroy() override;
};

class CloudfsControllerDedup : public CloudfsController {


  ChunkSplitter chunk_splitter_;
  static const size_t RECHUNK_BUF_SIZE;

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

    void destroy() override;
  
  private:
    int prepare_read_data(off_t offset, size_t r_size, uint64_t fd);
    int prepare_write_data(off_t offset, size_t w_size, uint64_t fd, int& rechunk_start_idx, int& buffer_end_idx);
};

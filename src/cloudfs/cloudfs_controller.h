/**
 * @file cloudfs_controller.h
 * @brief Cloudfs controller, handling a subset of file operations and managing buffer files and cloud objects
 * @author Cundao Yu <cundaoy@andrew.cmu.edu>
 */
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

/**
 * Cloudfs controller base class
 *
 * Defines a set of APIs for file operations and managing buffer files and cloud objects
 */
class CloudfsController
{
protected:
  /**
   * Open file info
   *
   * Stores state of an open file
   */
  struct OpenFile
  {
    std::string main_path_;     // main file path
    off_t start_;               // start offset of the file
    size_t len_;                // length of the file
    bool is_dirty_;             // true if the file is dirty
    std::vector<Chunk> chunks_; // chunks list
    uint64_t op_fd_;            // file descriptor of the buffer file

    OpenFile() = default;
    OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty) {}
    OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty, std::vector<Chunk> chunks)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty), chunks_(std::move(chunks)) {}
    OpenFile(const std::string main_path, off_t start, size_t len, bool is_dirty, std::vector<Chunk> chunks, uint64_t op_fd)
        : main_path_(std::move(main_path)), start_(start), len_(len), is_dirty_(is_dirty), chunks_(std::move(chunks)), op_fd_(op_fd) {}
  };

  struct cloudfs_state *state_;                             // cloudfs state
  std::string bucket_name_;                                 // bucket name
  std::shared_ptr<DebugLogger> logger_;                     // logger
  std::unordered_map<uint64_t, OpenFile> open_files_;       // open files
  std::shared_ptr<BufferFileController> buffer_controller_; // buffer file controller
  std::shared_ptr<ChunkTable> chunk_table_;                 // chunk table

public:
  /**
   * Constructor
   * @param state cloudfs state
   * @param host_name host name
   * @param bucket_name bucket name
   * @param logger logger
   */
  CloudfsController(struct cloudfs_state *state, const std::string &host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger);
  virtual ~CloudfsController();

  /**
   * Get buffer file controller
   * @return buffer file controller
   */
  std::shared_ptr<BufferFileController> get_buffer_file_controller()
  {
    return buffer_controller_;
  }

  /**
   * Handling 'getattr' fuse operation
   * @param path file path
   * @param stbuf stat buffer
   * @return 0 on success, negative errno on failure
   */
  int stat_file(const std::string &path, struct stat *stbuf);

  /**
   * Handling 'create' fuse operation
   * This function won't open the file and return a file descriptor after creation
   * @param path file path
   * @param mode file mode
   * @return 0 on success, negative errno on failure
   */
  int create_file(const std::string &path, mode_t mode);

  /**
   * Handling 'open' fuse operation
   * This fuction should be implemented by derived classes
   * @param path file path
   * @param flags file flags
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  virtual int open_file(const std::string &path, int flags, uint64_t *fd) = 0;

  /**
   * Handling 'read' fuse operation
   * This fuction should be implemented by derived classes
   * @param path file path
   * @param fd file descriptor
   * @param buf buffer
   * @param size size
   * @param offset offset
   * @return 0 on success, negative errno on failure
   */
  virtual int read_file(const std::string &path, uint64_t fd, char *buf, size_t size, off_t offset) = 0;

  /**
   * Handling 'write' fuse operation
   * This fuction should be implemented by derived classes
   * @param path file path
   * @param fd file descriptor
   * @param buf buffer
   * @param size size
   * @param offset offset
   * @return 0 on success, negative errno on failure
   */
  virtual int write_file(const std::string &path, uint64_t fd, const char *buf, size_t size, off_t offset) = 0;

  /**
   * Handling 'release' fuse operation
   * This fuction should be implemented by derived classes
   * @param path file path
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  virtual int close_file(const std::string &path, uint64_t fd) = 0;

  /**
   * Handling 'unlink' fuse operation
   * This fuction should be implemented by derived classes
   * @param path file path
   * @return 0 on success, negative errno on failure
   */
  virtual int unlink_file(const std::string &path) = 0;

  /**
   * Handling 'truncate' fuse operation
   * This fuction should be implemented by derived classes
   * @param path file path
   * @param size size
   * @return 0 on success, negative errno on failure
   */
  virtual int truncate_file(const std::string &path, off_t size) = 0;

  /**
   * Handling 'destroy' fuse operation
   * This fuction should be implemented by derived classes
   */
  virtual void destroy() = 0;

  /**
   * Get buffer path of a file
   * @param path main file path
   * @param buffer_path buffer file path
   * @return 0 on success, negative errno on failure
   */
  int get_buffer_path(const std::string &path, std::string &buffer_path);

  /**
   * Set buffer path of a file
   * @param path main file path
   * @param buffer_path buffer file path
   * @return 0 on success, negative errno on failure
   */
  int set_buffer_path(const std::string &path, const std::string &buffer_path);

  /**
   * Get size of a file
   * @param path buffer file path
   * @param size file size
   * @return 0 on success, negative errno on failure
   */
  int get_size(const std::string &path, off_t &size);

  /**
   * Get size of a file
   * @param fd file descriptor of the buffer file
   * @param size file size
   * @return 0 on success, negative errno on failure
   */
  int get_size(uint64_t fd, off_t &size);

  /**
   * Set size of a file
   * @param path buffer file path
   * @param size file size
   * @return 0 on success, negative errno on failure
   */
  int set_size(const std::string &path, off_t size);

  /**
   * Set size of a file
   * @param fd file descriptor of the buffer file
   * @param size file size
   * @return 0 on success, negative errno on failure
   */
  int set_size(uint64_t fd, off_t size);

  /**
   * Get is_on_cloud attribute of a file
   * @param path main file path
   * @param is_on_cloud is_on_cloud attribute
   * @return 0 on success, negative errno on failure
   */
  int get_is_on_cloud(const std::string &path, bool &is_on_cloud);

  /**
   * Set is_on_cloud attribute of a file
   * @param path main file path
   * @param is_on_cloud is_on_cloud attribute
   * @return 0 on success, negative errno on failure
   */
  int set_is_on_cloud(const std::string &path, bool is_on_cloud);

  /**
   * Set chunks list of a file
   * @param main_path main file path
   * @param chunks chunks list
   * @return 0 on success, negative errno on failure
   */
  int set_chunkinfo(const std::string &main_path, std::vector<Chunk> &chunks);

  /**
   * Get chunks list of a file
   * @param main_path main file path
   * @param chunks chunks list
   * @return 0 on success, negative errno on failure
   */
  int get_chunkinfo(const std::string &main_path, std::vector<Chunk> &chunks);

  /**
   * Get the index of the chunk that contains the given offset
   * @param chunks chunks list
   * @param offset file offset
   * @return chunk index
   */
  int get_chunk_idx(const std::vector<Chunk> &chunks, off_t offset);

  /**
   * Set truncated attribute of a file
   * truncated is true if the file has been truncated after the last write.
   * It means the open file needs to reload chunks list before next write.
   * @param path main file path
   * @param truncated truncated attribute
   * @return 0 on success, negative errno on failure
   */
  int set_truncated(const std::string &path, bool truncated);

  /**
   * Get truncated attribute of a file
   * truncated is true if the file has been truncated after the last write.
   * It means the open file needs to reload chunks list before next write.
   * @param path main file path
   * @param truncated truncated attribute
   * @return 0 on success, negative errno on failure
   */
  int get_truncated(const std::string &path, bool &truncated);

  /**
   * Get chunk table
   * @return chunk table
   */
  std::shared_ptr<ChunkTable> get_chunk_table()
  {
    return chunk_table_;
  }
};

/**
 * Cloudfs controller implementation without deduplication
 */
class CloudfsControllerNoDedup : public CloudfsController
{
public:
  /**
   * Constructor
   * @param state cloudfs state
   * @param host_name host name
   * @param bucket_name bucket name
   * @param logger logger
   */
  CloudfsControllerNoDedup(struct cloudfs_state *state, const std::string &host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger);
  ~CloudfsControllerNoDedup();

  /**
   * Handling 'open' fuse operation
   * implementation without deduplication
   * @param path file path
   * @param flags file flags
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  int open_file(const std::string &path, int flags, uint64_t *fd) override;

  /**
   * Handling 'read' fuse operation
   * implementation without deduplication
   * @param path file path
   * @param fd file descriptor
   * @param buf buffer
   * @param size size
   * @param offset offset
   * @return 0 on success, negative errno on failure
   */
  int read_file(const std::string &path, uint64_t fd, char *buf, size_t size, off_t offset) override;

  /**
   * Handling 'write' fuse operation
   * implementation without deduplication
   * @param path file path
   * @param fd file descriptor
   * @param buf buffer
   * @param size size
   * @param offset offset
   * @return 0 on success, negative errno on failure
   */
  int write_file(const std::string &path, uint64_t fd, const char *buf, size_t size, off_t offset) override;

  /**
   * Handling 'release' fuse operation
   * implementation without deduplication
   * @param path file path
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  int close_file(const std::string &path, uint64_t fd) override;

  /**
   * Handling 'unlink' fuse operation
   * implementation without deduplication
   * @param path file path
   * @return 0 on success, negative errno on failure
   */
  int unlink_file(const std::string &path) override;

  /**
   * Handling 'truncate' fuse operation
   * implementation without deduplication
   * @param path file path
   * @param size size
   * @return 0 on success, negative errno on failure
   */
  int truncate_file(const std::string &path, off_t size) override;

  /**
   * Handling 'destroy' fuse operation
   * implementation without deduplication
   */
  void destroy() override;
};

/**
 * Cloudfs controller implementation with deduplication
 */
class CloudfsControllerDedup : public CloudfsController
{

  ChunkSplitter chunk_splitter_;        // chunk splitter
  static const size_t RECHUNK_BUF_SIZE; // rechunk buffer size

public:
  /**
   * Constructor
   * @param state cloudfs state
   * @param host_name host name
   * @param bucket_name bucket name
   * @param logger logger
   * @param window_size window size
   * @param avg_seg_size average segment size
   * @param min_seg_size minimum segment size
   * @param max_seg_size maximum segment size
   */
  CloudfsControllerDedup(struct cloudfs_state *state, const std::string &host_name, std::string bucket_name,
                         std::shared_ptr<DebugLogger> logger, int window_size, int avg_seg_size, int min_seg_size, int max_seg_size);
  ~CloudfsControllerDedup();

  /**
   * Handling 'open' fuse operation
   * implementation with deduplication
   * @param path file path
   * @param flags file flags
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  int open_file(const std::string &path, int flags, uint64_t *fd) override;

  /**
   * Handling 'read' fuse operation
   * implementation with deduplication
   * @param path file path
   * @param fd file descriptor
   * @param buf buffer
   * @param size size
   * @param offset offset
   * @return 0 on success, negative errno on failure
   */
  int read_file(const std::string &path, uint64_t fd, char *buf, size_t size, off_t offset) override;

  /**
   * Handling 'write' fuse operation
   * implementation with deduplication
   * @param path file path
   * @param fd file descriptor
   * @param buf buffer
   * @param size size
   * @param offset offset
   * @return 0 on success, negative errno on failure
   */
  int write_file(const std::string &path, uint64_t fd, const char *buf, size_t size, off_t offset) override;

  /**
   * Handling 'release' fuse operation
   * implementation with deduplication
   * @param path file path
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  int close_file(const std::string &path, uint64_t fd) override;

  /**
   * Handling 'unlink' fuse operation
   * implementation with deduplication
   * @param path file path
   * @return 0 on success, negative errno on failure
   */
  int unlink_file(const std::string &path) override;

  /**
   * Handling 'truncate' fuse operation
   * implementation with deduplication
   * @param path file path
   * @param size size
   * @return 0 on success, negative errno on failure
   */
  int truncate_file(const std::string &path, off_t size) override;

  /**
   * Handling 'destroy' fuse operation
   * implementation with deduplication
   */
  void destroy() override;

private:
  /**
   * Prepare data for read operation
   * Download chunks that the read range belongs to
   * @param offset offset
   * @param r_size read size
   * @param fd file descriptor
   * @return 0 on success, negative errno on failure
   */
  int prepare_read_data(off_t offset, size_t r_size, uint64_t fd);

  /**
   * Prepare data for write operation
   * Download chunks that the write range belongs to
   * @param offset offset
   * @param w_size write size
   * @param fd file descriptor
   * @param rechunk_start_idx rechunk start index
   * @param buffer_end_idx buffer end index
   * @return 0 on success, negative errno on failure
   */
  int prepare_write_data(off_t offset, size_t w_size, uint64_t fd, int &rechunk_start_idx, int &buffer_end_idx);
};

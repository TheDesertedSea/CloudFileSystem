#include "cloudfs_controller.h"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "chunk_splitter.h"
#include "buffer_file.h"

int CloudfsController::get_buffer_path(const std::string& path, std::string& buffer_path) {
  char buf[PATH_MAX + 1];
  auto ret = lgetxattr(path.c_str(), "user.cloudfs.buffer_path", buf, PATH_MAX);
  if(ret == -1) {
    return -1;
  }
  buf[ret] = '\0';
  buffer_path = buf;
  return 0;
}

int CloudfsController::set_buffer_path(const std::string& path, const std::string& buffer_path) {
  auto ret = lsetxattr(path.c_str(), "user.cloudfs.buffer_path", buffer_path.c_str(), buffer_path.size(), 0);
  if(ret == -1) {
    return -1;
  }
  return 0;
}

int CloudfsController::get_size(const std::string& path, off_t& size) {
  char buf[sizeof(off_t)];
  auto ret = lgetxattr(path.c_str(), "user.cloudfs.size", buf, sizeof(off_t));
  if(ret == -1) {
    return -1;
  }
  size = *(off_t*)buf;
  return 0;
}

int CloudfsController::get_size(uint64_t fd, off_t& size) {
  char buf[sizeof(off_t)];
  auto ret = fgetxattr(fd, "user.cloudfs.size", buf, sizeof(off_t));
  if(ret == -1) {
    return -1;
  }
  size = *(off_t*)buf;
  return 0;
}

int CloudfsController::set_size(const std::string& path, off_t size) {
  auto ret = lsetxattr(path.c_str(), "user.cloudfs.size", &size, sizeof(off_t), 0);
  if(ret == -1) {
    return -1;
  }
  return 0;
}

int CloudfsController::set_size(uint64_t fd, off_t size) {
  auto ret = fsetxattr(fd, "user.cloudfs.size", &size, sizeof(off_t), 0);
  if(ret == -1) {
    return -1;
  }
  return 0;
}

int CloudfsController::get_is_on_cloud(const std::string& path, bool& is_on_cloud) {
  char buf[sizeof(bool)];
  auto ret = lgetxattr(path.c_str(), "user.cloudfs.is_on_cloud", buf, sizeof(bool));
  if(ret == -1) {
    return -1;
  }
  is_on_cloud = *(bool*)buf;
  return 0;
}

int CloudfsController::set_is_on_cloud(const std::string& path, bool is_on_cloud) {
  auto ret = lsetxattr(path.c_str(), "user.cloudfs.is_on_cloud", &is_on_cloud, sizeof(bool), 0);
  if(ret == -1) {
    return -1;
  }
  return 0;
}

int CloudfsController::get_chunkinfo(const std::string& main_path, std::vector<Chunk>& chunks) {
  FILE* file = fopen(main_path.c_str(), "r");
  if(file == NULL) {
    return -1;
  }

  size_t num_chunks;
  fread(&num_chunks, sizeof(size_t), 1, file);

  // logger_->info("get_chunkinfo: num_chunks " + std::to_string(num_chunks));

  chunks.clear();
  chunks.reserve(num_chunks);
  for(size_t i = 0; i < num_chunks; i++) {
    off_t start;
    size_t len;
    fread(&start, sizeof(off_t), 1, file);
    fread(&len, sizeof(size_t), 1, file);
    size_t key_len;
    fread(&key_len, sizeof(size_t), 1, file);
    std::vector<char> key(key_len);
    fread(key.data(), sizeof(char), key_len, file);
    std::string key_str(key.begin(), key.end());
    chunks.emplace_back(start, len, key_str);
    // logger_->info("get_chunkinfo: start " + std::to_string(start) + ", len " + std::to_string(len) + ", key " + key_str);
  }
  fclose(file);

  return 0;
}

int CloudfsController::set_chunkinfo(const std::string& main_path, std::vector<Chunk>& chunks) {
  FILE* file = fopen(main_path.c_str(), "w");
  if(file == NULL) {
    return -1;
  }

  // clear file first
  ftruncate(fileno(file), 0);

  size_t num_chunks = chunks.size();
  fwrite(&num_chunks, sizeof(size_t), 1, file);
  // logger_->info("set_chunkinfo: num_chunks " + std::to_string(num_chunks));

  for(size_t i = 0; i < num_chunks; i++) {
    fwrite(&chunks[i].start_, sizeof(off_t), 1, file);
    fwrite(&chunks[i].len_, sizeof(size_t), 1, file);
    size_t key_len = chunks[i].key_.size();
    fwrite(&key_len, sizeof(size_t), 1, file);
    fwrite(chunks[i].key_.data(), sizeof(char), key_len, file);
  }
  fclose(file);

  return 0;
}

int CloudfsController::get_chunk_idx(const std::vector<Chunk>& chunks, off_t offset) {
  off_t cur = 0;
  for(int i = 0; i < (int)chunks.size(); i++) {
    if(offset >= cur && offset < cur + (off_t)chunks[i].len_) {
      return i;
    }
    cur += chunks[i].len_;
  }

  return -1;
}

int CloudfsController::set_truncated(const std::string& path, bool truncated) {
  auto ret = lsetxattr(path.c_str(), "user.cloudfs.truncated", &truncated, sizeof(bool), 0);
  if(ret == -1) {
    return -1;
  }
  return 0;
}

int CloudfsController::get_truncated(const std::string& path, bool& truncated) {
  char buf[sizeof(bool)];
  auto ret = lgetxattr(path.c_str(), "user.cloudfs.truncated", buf, sizeof(bool));
  if(ret == -1) {
    return -1;
  }
  truncated = *(bool*)buf;
  return 0;
}

CloudfsController::CloudfsController(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger) 
  : state_(state), bucket_name_(std::move(bucket_name)), logger_(std::move(logger)), buffer_controller_(std::make_shared<BufferController>(host_name, bucket_name_, logger_)), chunk_table_(state->ssd_path, logger) {

}

CloudfsController::~CloudfsController() {
  
}

CloudfsControllerNoDedup::CloudfsControllerNoDedup(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger) 
  : CloudfsController(state, host_name, std::move(bucket_name), std::move(logger)) {

}

CloudfsControllerNoDedup::~CloudfsControllerNoDedup() {
}

int CloudfsController::stat_file(const std::string& path, struct stat* stbuf) {
  logger_->info("stat_file: " + path);

  auto main_path = state_->ssd_path + path;

  auto ret = lstat(main_path.c_str(), stbuf);
  if(ret != 0) {
    return logger_->error("stat_file: stat main_path failed");
  }

  if(S_ISREG(stbuf->st_mode)) {
    std::string buffer_path;
    ret = get_buffer_path(main_path, buffer_path);
    if(ret != 0) {
      return logger_->error("stat_file: get_buffer_path failed");
    }
    
    ret = get_size(buffer_path, stbuf->st_size);
    if(ret != 0) {
      return logger_->error("stat_file: get_size failed");
    }
  }

  logger_->info("stat_file: size " + std::to_string(stbuf->st_size));
  return 0;
}

int CloudfsController::create_file(const std::string& path, mode_t mode) {
  logger_->info("create_file: " + path + ", mode: " + std::to_string(mode));

  auto main_path = state_->ssd_path + path;

  auto ret = creat(main_path.c_str(), mode);
  if(ret == -1) {
    return logger_->error("create_file: creat main_path failed");
  }

  auto buffer_path = main_path_to_buffer_path(main_path);
  ret = set_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("create_file: set_buffer_path failed");
  }
  ret = set_is_on_cloud(main_path, false);
  if(ret != 0) {
    return logger_->error("create_file: set_is_on_cloud failed");
  }

  ret = set_truncated(main_path, false);
  if(ret != 0) {
    return logger_->error("create_file: set_truncated failed");
  }

  std::vector<Chunk> chunks;
  ret = set_chunkinfo(main_path, chunks); // no chunk initially

  // logger_->info("create_file: create main_path success");

  // create buffer file
  
  ret = creat(buffer_path.c_str(), 0777);
  if(ret == -1) {
    return logger_->error("create_file: creat buffer_path failed");
  }
  // put size in buffer file
  ret = set_size(buffer_path, 0);
  if(ret != 0) {
    return logger_->error("create_file: set_size failed");
  }

  // logger_->info("create_file: success");

  return 0;
}

int CloudfsControllerNoDedup::open_file(const std::string& path, int flags, uint64_t* fd) {
  logger_->info("open_file: " + path + ", flags: " + std::to_string(flags));

  auto main_path = state_->ssd_path + path;

  // flags remove O_CREAT, O_EXCL
  auto ret = open(main_path.c_str(), flags & ~(O_CREAT | O_EXCL));
  if(ret == -1) {
    return logger_->error("open_file: open main_path failed");
  }
  close(ret);

  std::string buffer_path;
  ret = get_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("open_file: get_buffer_path failed");
  }

  bool is_on_cloud;
  ret = get_is_on_cloud(main_path, is_on_cloud);
  if(ret != 0) {
    return logger_->error("open_file: get_is_on_cloud failed");
  }

  if(is_on_cloud) {
    // download file
    ret = buffer_controller_->download_file(generate_object_key(buffer_path), buffer_path);
    if(ret != 0) {
      return logger_->error("open_file: download_file failed");
    }

    logger_->info("open_file: download_file success");
  }

  // open buffer file
  ret = open(buffer_path.c_str(), flags & ~(O_CREAT | O_EXCL));
  if(ret == -1) {
    return logger_->error("open_file: open buffer_path failed");
  }

  *fd = ret;
  open_files_[*fd] = OpenFile(main_path, 0, 0, false);

  logger_->info("open_file: success");

  return 0;
}

int CloudfsControllerNoDedup::read_file(const std::string& path, uint64_t fd, char* buf, size_t size, off_t offset) {
  logger_->info("read_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(size) + ", offset: " + std::to_string(offset));

  auto ret = pread(fd, buf, size, offset);
  if(ret == -1) {
    return logger_->error("read_file: pread failed");
  }

  logger_->info("read_file: success, read " + std::to_string(ret) + " bytes");

  return ret;
}

int CloudfsControllerNoDedup::write_file(const std::string& path, uint64_t fd, const char* buf, size_t size, off_t offset) {
  logger_->info("write_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(size) + ", offset: " + std::to_string(offset));

  auto written = pwrite(fd, buf, size, offset);
  if(written == -1) {
    return logger_->error("write_file: pwrite failed");
  }

  open_files_[fd].is_dirty_ = true; // mark as dirty
  struct stat stbuf;
  auto ret = fstat(fd, &stbuf);
  if(ret == -1) {
    return logger_->error("write_file: fstat failed");
  }

  ret = set_size(fd, stbuf.st_size);
  if(ret != 0) {
    return logger_->error("write_file: set_size failed");
  }

  logger_->info("write_file: success, write " + std::to_string(written) + " bytes");

  return written;
}

int CloudfsControllerNoDedup::close_file(const std::string& path, uint64_t fd) {
  logger_->info("close_file: " + path + ", fd: " + std::to_string(fd));

  auto main_path = state_->ssd_path + path;
  std::string buffer_path;
  auto ret = get_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("close_file: get_buffer_path failed");
  }

  ret = close(fd);
  if(ret == -1) {
    return logger_->error("close_file: close buffer_path failed");
  }

  struct stat stbuf;
  ret = lstat(buffer_path.c_str(), &stbuf);
  if(ret == -1) {
    return logger_->error("close_file: stat buffer_path failed");
  }

  off_t old_size;
  ret = get_size(buffer_path, old_size);
  if(ret != 0) {
    return logger_->error("close_file: get_size failed");
  }

  ret = set_size(buffer_path, stbuf.st_size);
  if(ret != 0) {
    return logger_->error("close_file: set_size failed");
  }

  if(open_files_[fd].is_dirty_ || stbuf.st_size != old_size) {
    if(stbuf.st_size > (off_t)state_->threshold) {
      // upload file
      ret = buffer_controller_->upload_file(generate_object_key(buffer_path), buffer_path, stbuf.st_size);
      if(ret != 0) {
        return logger_->error("close_file: upload_file failed");
      }
      logger_->info("close_file: upload_file success");

      ret = set_is_on_cloud(main_path, true);
      if(ret != 0) {
        return logger_->error("close_file: set_is_on_cloud failed");
      }
      // clear buffer file
      ret = buffer_controller_->clear_file(buffer_path);
      if(ret != 0) {
        return logger_->error("close_file: clear_file failed");
      }
    } else {
      // keep file locally
      bool is_on_cloud;
      ret = get_is_on_cloud(main_path, is_on_cloud);
      if(ret != 0) {
        return logger_->error("close_file: get_is_on_cloud failed");
      }
      if(is_on_cloud) {
        // delete object on cloud
        buffer_controller_->delete_object(generate_object_key(buffer_path));
      }
      ret = set_is_on_cloud(main_path, false);
      if(ret != 0) {
        return logger_->error("close_file: set_is_on_cloud failed");
      }
    }
  } else{
    bool is_on_cloud;
    ret = get_is_on_cloud(main_path, is_on_cloud);
    if(ret != 0) {
      return logger_->error("close_file: get_is_on_cloud failed");
    }
    if(is_on_cloud) {
      // clear buffer file
      ret = buffer_controller_->clear_file(buffer_path);
      if(ret != 0) {
        return logger_->error("close_file: clear_file failed");
      }
    }
  }

  open_files_.erase(fd);

  logger_->info("close_file: success with size " + std::to_string(stbuf.st_size));

  return 0;
}

int CloudfsControllerNoDedup::unlink_file(const std::string& path) {
  logger_->info("unlink_file: " + path);

  auto main_path = state_->ssd_path + path;

  struct stat stbuf;
  auto ret = lstat(main_path.c_str(), &stbuf);
  if(ret == -1) {
    return logger_->error("unlink_file: stat main_path failed");
  }
  logger_->info("unlink_file: main_path link count " + std::to_string(stbuf.st_nlink));

  if(S_ISREG(stbuf.st_mode) && stbuf.st_nlink == 1) {
    // only one link, delete buffer file
    logger_->info("unlink_file: only one link, delete buffer file");

    std::string buffer_path;
    ret = get_buffer_path(main_path, buffer_path);
    if(ret != 0) {
      return logger_->error("unlink_file: get_buffer_path failed");
    }

    bool is_on_cloud;
    ret = get_is_on_cloud(main_path, is_on_cloud);
    if(ret != 0) {
      return logger_->error("unlink_file: get_is_on_cloud failed");
    }
    if(is_on_cloud) {
      // delete object on cloud
      buffer_controller_->delete_object(generate_object_key(buffer_path));
    }

    // unlink buffer file
    ret = unlink(buffer_path.c_str());
    if(ret == -1) {
      return logger_->error("unlink_file: unlink buffer_path failed");
    }
  }

  // unlink main file
  ret = unlink(main_path.c_str());
  if(ret == -1) {
    return logger_->error("unlink_file: unlink main_path failed");
  }

  logger_->info("unlink_file: success");

  return 0;
}

// assume truncate only after open and before close
int CloudfsControllerNoDedup::truncate_file(const std::string& path, off_t size) {
  logger_->info("truncate_file: " + path + ", size: " + std::to_string(size));

  auto main_path = state_->ssd_path + path;
  std::string buffer_path;
  auto ret = get_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("truncate_file: get_buffer_path failed");
  }

  ret = truncate(buffer_path.c_str(), size);
  if(ret == -1) {
    return logger_->error("truncate_file: truncate buffer_path failed");
  }

  logger_->info("truncate_file: success");

  return 0;
}

void CloudfsControllerNoDedup::destroy() {
}

const size_t CloudfsControllerDedup::RECHUNK_BUF_SIZE = 4 * 1024;

CloudfsControllerDedup::CloudfsControllerDedup(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, 
      std::shared_ptr<DebugLogger> logger, int window_size, int avg_seg_size, int min_seg_size, int max_seg_size):
  CloudfsController(state, host_name, std::move(bucket_name), logger),
  chunk_splitter_(window_size, avg_seg_size, min_seg_size, max_seg_size) { 
    logger_->debug("CloudfsControllerDedup: window_size " + std::to_string(window_size) + ", avg_seg_size " + std::to_string(avg_seg_size) + ", min_seg_size " + std::to_string(min_seg_size) + ", max_seg_size " + std::to_string(max_seg_size));
}

CloudfsControllerDedup::~CloudfsControllerDedup() {
}

int CloudfsControllerDedup::open_file(const std::string& path, int flags, uint64_t* fd) {
  logger_->info("open_file: " + path + ", flags: " + std::to_string(flags));

  auto main_path = state_->ssd_path + path;

  // flags remove O_CREAT, O_EXCL
  auto ret = open(main_path.c_str(), flags & ~(O_CREAT | O_EXCL | O_TRUNC));
  if(ret == -1) {
    return logger_->error("open_file: open main_path failed");
  }
  close(ret);

  std::string buffer_path;
  ret = get_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("open_file: get_buffer_path failed");
  }

  // open buffer file
  ret = open(buffer_path.c_str(), flags & ~(O_CREAT | O_EXCL));
  if(ret == -1) {
    return logger_->error("open_file: open buffer_path failed");
  }

  *fd = ret;

  std::vector<Chunk> chunks;
  ret = get_chunkinfo(main_path, chunks);
  if(ret != 0) {
    return logger_->error("open_file: get_chunkinfo failed");
  }

  auto op_fd = open(buffer_path.c_str(), O_RDWR);

  open_files_[*fd] = OpenFile(main_path, 0, 0, false, chunks, op_fd);

  // logger_->info("open_file: success");
  return 0;
}

int CloudfsControllerDedup::read_file(const std::string& path, uint64_t fd, char* buf, size_t read_size, off_t read_offset) {
  logger_->info("read_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(read_size) + ", offset: " + std::to_string(read_offset));

  auto main_path = open_files_[fd].main_path_;

  off_t file_size;
  auto ret = get_size(fd, file_size);
  if(ret != 0) {
    return logger_->error("read_file: get_size failed");
  }
  // logger_->info("read_file: file_size " + std::to_string(file_size));

  off_t buffer_offset;
  if(file_size > state_->threshold) {
    // logger_->info("read_file: prepare cloud data");
    auto ret = prepare_read_data(read_offset, read_size, fd);
    if(ret < 0) {
      logger_->error("read_file: prepare_read_data failed");
      return ret;
    }
    buffer_offset = open_files_[fd].start_;
  } else {
    // local
    buffer_offset = 0;
  }

  // logger_->info("read_file: ready to read, read size " + std::to_string(read_size) + ", read offset " + std::to_string(read_offset - buffer_offset));
  auto read_cnt = pread(fd, buf, read_size, read_offset - buffer_offset);
  if(read_cnt < 0) {
    return logger_->error("read_file: pread failed");
  }

  // logger_->info("read_file: success, read " + std::to_string(read_cnt) + " bytes");
  return read_cnt;
}

int CloudfsControllerDedup::write_file(const std::string& path, uint64_t fd, const char* buf, size_t size, off_t write_offset) {
  logger_->info("write_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(size) + ", offset: " + std::to_string(write_offset));
  // logger_->debug("write_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(size) + ", offset: " + std::to_string(write_offset));
  // logger_->debug("write_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(size) + ", offset: " + std::to_string(write_offset));

  auto main_path = open_files_[fd].main_path_;
  auto op_fd = open_files_[fd].op_fd_;

  off_t file_size;
  auto ret = get_size(fd, file_size);
  if(ret != 0) {
    return logger_->error("write_file: get_size failed");
  }
  // logger_->info("write_file: file_size " + std::to_string(file_size));

  bool is_truncated;
  ret = get_truncated(main_path, is_truncated);
  if(ret != 0) {
    return logger_->error("write_file: get_truncated failed");
  }

  if(is_truncated) {
    // reload chunks
    open_files_[fd].chunks_.clear();
    ret = get_chunkinfo(main_path, open_files_[fd].chunks_);
    if(ret != 0) {
      return logger_->error("write_file: get_chunkinfo failed");
    }
    ret = set_truncated(main_path, false);
    if(ret != 0) {
      return logger_->error("write_file: set_truncated failed");
    }
  }

  auto& chunks = open_files_[fd].chunks_;
  std::vector<Chunk> new_chunks;
  // logger_->info("write_file: original chunks count " + std::to_string(chunks.size()));

  int rechunk_start_idx, buffer_end_idx;
  off_t buffer_offset;
  size_t buffer_len;
  if(file_size > state_->threshold) {
    auto ret = prepare_write_data(write_offset, size, fd, rechunk_start_idx, buffer_end_idx);
    if(ret < 0) {
      logger_->error("write_file: prepare_write_data failed");
      return ret;
    }
    buffer_offset = open_files_[fd].start_;
    buffer_len = open_files_[fd].len_;
  } else {
    buffer_offset = 0;
    buffer_len = file_size;
    rechunk_start_idx = 0;
    buffer_end_idx = -1;
  }

  logger_->info("write_file: buffer_offset " + std::to_string(buffer_offset) + ", buffer_len " + std::to_string(buffer_len));
  auto written = pwrite(fd, buf, size, write_offset - buffer_offset);
  if(written < 0) {
    return logger_->error("write_file: pwrite failed");
  }

  ssize_t len_increase = write_offset + written - buffer_offset - buffer_len;
  logger_->info("write_file: len_increase " + std::to_string(len_increase));
  auto old_file_size = file_size;
  if(len_increase > 0) {
    file_size += len_increase;
    ret = set_size(fd, file_size);
    if(ret != 0) {
      return logger_->error("write_file: set_size failed");
    }
  }
  buffer_len = std::max(buffer_len, (size_t)(write_offset - buffer_offset + written));

  if(file_size <= state_->threshold) {
    logger_->info("write_file: success, write " + std::to_string(written) + " bytes");
    return written;
  } else if(old_file_size <= state_->threshold) {
    logger_->info("write_file: write cause a local file to cloud file");
    rechunk_start_idx = 0;
    buffer_end_idx = -1;
  }


  // rechunk buffer file contents

  logger_->info("write_file: rechunk_start_idx " + std::to_string(rechunk_start_idx) + ", buffer_end_idx " + std::to_string(buffer_end_idx));
  // logger_->info("write_file: buffer_offset " + std::to_string(buffer_offset) + ", buffer_len " + std::to_string(buffer_len));
  chunk_splitter_.init(buffer_offset);
  char rechunk_buf[RECHUNK_BUF_SIZE];
  ssize_t read_p = 0;
  while(read_p < (ssize_t)buffer_len) {
    auto read_cnt = pread(op_fd, rechunk_buf, RECHUNK_BUF_SIZE, read_p);
    if(read_cnt < 0) {
      return logger_->error("write_file: pread(rechunk buf) failed");
    }
    auto next_chunks = chunk_splitter_.get_chunks_next(rechunk_buf, read_cnt);
    for(auto& c: next_chunks) {
      auto is_first = chunk_table_.Use(c.key_);
      if(is_first) {
        logger_->debug("write_file: upload chunk(rechunk buf) start " + std::to_string(c.start_) + ", len " + std::to_string(c.len_) + ", key " + c.key_);
        ret = buffer_controller_->upload_chunk(c.key_, op_fd, c.start_ - buffer_offset, c.len_);
      }
      new_chunks.push_back(c);
    }
    read_p += read_cnt;
  }
  auto last_chunk = chunk_splitter_.get_chunk_last();
  if(last_chunk.len_ > 0) {
    auto is_first = chunk_table_.Use(last_chunk.key_);
    if(is_first) {
      logger_->debug("write_file: upload chunk(rechunk buf) start " + std::to_string(last_chunk.start_) + ", len " + std::to_string(last_chunk.len_) + ", key " + last_chunk.key_);
      ret = buffer_controller_->upload_chunk(last_chunk.key_, op_fd, last_chunk.start_ - buffer_offset, last_chunk.len_);
    }
    new_chunks.push_back(last_chunk);
  }
  logger_->info("write_file: get new chunks count " + std::to_string(new_chunks.size()));

  int release_end;
  if(buffer_end_idx != -1) {
    // write within the file size
    release_end = buffer_end_idx;
    // if(new_chunks.size() > 0 && new_chunks.back().start_ + new_chunks.back().len_ == chunks[buffer_end_idx].start_ + chunks[buffer_end_idx].len_) {
    //   logger_->info("write_file: same boundary, no need to rechunk the rest of the file");
    //   // no need to rechunk the rest of the file
    //   release_end = buffer_end_idx;
    // } else {
    //   // first copy the remaining part of the buffer file
    //   logger_->info("write_file: rechunk the rest of the file");
    //   size_t remain_len = buffer_len;
    //   ssize_t read_cnt;
    //   ssize_t write_cnt;
    //   if(new_chunks.size() > 0) {
    //     remain_len = buffer_len - new_chunks.back().start_ - new_chunks.back().len_;

    //     read_cnt = pread(op_fd, rechunk_buf, remain_len, new_chunks.back().start_ + new_chunks.back().len_ - buffer_offset);
    //     if(read_cnt != (ssize_t)remain_len) {
    //       return logger_->error("write_file: read remaining not expected length");
    //     }

    //     // clear and write remaining at the beginning of the buffer file
    //     buffer_controller_->clear_file(op_fd);
    //     write_cnt = pwrite(op_fd, rechunk_buf, remain_len, 0);
    //     if(write_cnt != (ssize_t)remain_len) {
    //       return logger_->error("write_file: write remaining not expected length");
    //     }
    //     buffer_offset = new_chunks.back().start_;
    //   }
      
    //   // logger_->info("write_file: remaining length " + std::to_string(remain_len) + ", buffer_offset " + std::to_string(buffer_offset));
    //   // rechunk the rest of the file
    //   release_end = chunks.size() - 1;
    //   auto cur_chunk_idx = buffer_end_idx + 1;
    //   while(cur_chunk_idx < (int)chunks.size()) {
    //     auto& chunk = chunks[cur_chunk_idx];
    //     auto ret = buffer_controller_->download_chunk(chunk.key_, op_fd, remain_len);
    //     if(ret != 0) {
    //       return logger_->error("write_file: download_chunk(rechunk rest) failed");
    //     }
    //     buffer_offset = chunk.start_ - remain_len;

    //     size_t used_len = 0; // do not include the remaining part
    //     read_p = remain_len; // read after the remaining part
    //     while(read_p < (ssize_t)chunk.len_) {
    //       auto read_cnt = pread(op_fd, rechunk_buf, RECHUNK_BUF_SIZE, read_p);
    //       if(read_cnt < 0) {
    //         return logger_->error("write_file: pread(rechunk rest) failed");
    //       }
    //       auto next_chunks = chunk_splitter_.get_chunks_next(rechunk_buf, read_cnt);
    //       for(auto& c: next_chunks) {
    //         auto is_first = chunk_table_.Use(c.key_);
    //         if(is_first) {
    //           logger_->debug("write_file: upload chunk(rechunk rest) start " + std::to_string(c.start_) + ", len " + std::to_string(c.len_) + ", key " + c.key_);
    //           ret = buffer_controller_->upload_chunk(c.key_, op_fd, c.start_ - buffer_offset, c.len_);
    //         }
    //         new_chunks.push_back(c);
    //         used_len += c.len_; // chunk here may include the remaining part
    //       }
    //       read_p += read_cnt;
    //     }

    //     if(new_chunks.back().start_ + new_chunks.back().len_ == chunks[cur_chunk_idx].start_ + chunks[cur_chunk_idx].len_) {
    //       // no need to rechunk more
    //       release_end = cur_chunk_idx;
    //       break;
    //     }
    //     cur_chunk_idx++;

    //     remain_len = chunk.len_ + remain_len - used_len; // remaining length of this chunk
    //     read_cnt = pread(op_fd, rechunk_buf, remain_len, used_len);
    //     if(read_cnt != (ssize_t)remain_len) {
    //       return logger_->error("write_file: read remaining not expected length");
    //     }

    //     // clear and write remaining at the beginning of the buffer file
    //     buffer_controller_->clear_file(op_fd);
    //     write_cnt = pwrite(op_fd, rechunk_buf, remain_len, 0);
    //     if(write_cnt != (ssize_t)remain_len) {
    //       return logger_->error("write_file: write remaining not expected length");
    //     }
    //   }

    //   auto last_chunk = chunk_splitter_.get_chunk_last();
    //   if(last_chunk.len_ > 0) {
    //     auto is_first = chunk_table_.Use(last_chunk.key_);
    //     if(is_first) {
    //       logger_->debug("write_file: upload chunk(rechunk rest) start " + std::to_string(last_chunk.start_) + ", len " + std::to_string(last_chunk.len_) + ", key " + last_chunk.key_);
    //       ret = buffer_controller_->upload_chunk(last_chunk.key_, op_fd, last_chunk.start_ - buffer_offset, last_chunk.len_);
    //     }
    //     new_chunks.push_back(last_chunk);
    //   }
    // }
  } else {
    // write causes file size increase
    logger_->info("write_file: no rechunk rest, write causes file size increase");
    // auto last_chunk = chunk_splitter_.get_chunk_last();
    // if(last_chunk.len_ > 0) {
    //   auto is_first = chunk_table_.Use(last_chunk.key_);
    //   if(is_first) {
    //     logger_->debug("write_file: upload chunk(rechunk rest) start " + std::to_string(last_chunk.start_) + ", len " + std::to_string(last_chunk.len_) + ", key " + last_chunk.key_);
    //     ret = buffer_controller_->upload_chunk(last_chunk.key_, op_fd, last_chunk.start_ - buffer_offset, last_chunk.len_);
    //   }
    //   new_chunks.push_back(last_chunk);
    // }

    release_end = chunks.size() - 1;
  }

  logger_->info("write_file: rechunk from " + std::to_string(rechunk_start_idx) + " to " + std::to_string(release_end));
  for(int i = rechunk_start_idx; i <= release_end; i++) {
    logger_->info("write_file: release chunk " + chunks[i].key_);
    auto is_last = chunk_table_.Release(chunks[i].key_);
    if(is_last) {
      ret = buffer_controller_->delete_object(chunks[i].key_);
    }
  }

  std::vector<Chunk> remain_chunks;
  if(release_end >= 0) {
    remain_chunks = std::vector<Chunk>(chunks.begin() + release_end + 1, chunks.end());
  } else {
    remain_chunks = {};
  }

  chunks.resize(rechunk_start_idx);
  chunks.insert(chunks.end(), new_chunks.begin(), new_chunks.end());
  chunks.insert(chunks.end(), remain_chunks.begin(), remain_chunks.end());

  // for(auto& c: chunks) {
  //   logger_->info("write_file: chunk start " + std::to_string(c.start_) + ", len " + std::to_string(c.len_) + ", key " + c.key_);
  // }

  // logger_->info("write_file: success, write " + std::to_string(written) + " bytes" + ", chunk count " + std::to_string(chunks.size()));
  ret = set_chunkinfo(main_path, chunks);
  if(ret != 0) {
    return logger_->error("write_file: set_chunkinfo failed");
  }

  logger_->info("write_file: success, write " + std::to_string(written) + " bytes");
  return written;
}

int CloudfsControllerDedup::close_file(const std::string& path, uint64_t fd) {
  logger_->info("close_file: " + path + ", fd: " + std::to_string(fd));

  close(open_files_[fd].op_fd_);

  auto ret = close(fd);
  if(ret == -1) {
    return logger_->error("close_file: close buffer_path failed");
  }

  auto main_path = open_files_[fd].main_path_;

  std::vector<Chunk> chunks;
  ret = get_chunkinfo(main_path, chunks);
  if(ret != 0) {
    return logger_->error("close_file: get_chunkinfo failed");
  }

  for(auto& chunk : chunks) {
    logger_->info("close_file: chunk start " + std::to_string(chunk.start_) + ", len " + std::to_string(chunk.len_) + ", key " + chunk.key_);
  }

  // chunk_table_.Print();

  return 0;
}

int CloudfsControllerDedup::unlink_file(const std::string& path) {
  logger_->info("unlink_file: " + path);

  auto main_path = state_->ssd_path + path;

  struct stat stbuf;
  auto ret = lstat(main_path.c_str(), &stbuf);
  if(ret == -1) {
    return logger_->error("unlink_file: stat main_path failed");
  }

  if(S_ISREG(stbuf.st_mode) && stbuf.st_nlink == 1) {
    // only one link, delete buffer file
    logger_->info("unlink_file: only one link, delete buffer file");

    std::string buffer_path;
    ret = get_buffer_path(main_path, buffer_path);
    if(ret != 0) {
      return logger_->error("unlink_file: get_buffer_path failed");
    }

    // unlink buffer file
    ret = unlink(buffer_path.c_str());
    if(ret == -1) {
      return logger_->error("unlink_file: unlink buffer_path failed");
    }

    std::vector<Chunk> chunks;
    ret = get_chunkinfo(main_path, chunks);
    if(ret != 0) {
      return logger_->error("unlink_file: get_chunkinfo failed");
    }

    for(auto& c: chunks) {
      // logger_->info("unlink_file: chunk start " + std::to_string(c.start_) + ", len " + std::to_string(c.len_) + ", key " + c.key_);
      auto is_last = chunk_table_.Release(c.key_);
      if(is_last) {
        ret = buffer_controller_->delete_object(c.key_);
      }
    }
  }

  // unlink main file
  ret = unlink(main_path.c_str());
  if(ret == -1) {
    return logger_->error("unlink_file: unlink main_path failed");
  }

  // logger_->info("unlink_file: success");
  return 0;
}

int CloudfsControllerDedup::truncate_file(const std::string& path, off_t truncate_size) {
  // logger_->info("truncate_file: " + path + ", size: " + std::to_string(truncate_size));
  logger_->debug("truncate_file: " + path + ", size: " + std::to_string(truncate_size));

  auto main_path = state_->ssd_path + path;
  std::string buffer_path;
  auto ret = get_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("truncate_file: get_buffer_path failed");
  }

  off_t file_size;
  ret = get_size(buffer_path, file_size);
  if(ret != 0) {
    return logger_->error("truncate_file: get_size failed");
  }

  if(truncate_size >= file_size) {
    // logger_->info("truncate_file: success");
    return 0;
  }

  if(file_size <= state_->threshold) {
    ret = truncate(buffer_path.c_str(), truncate_size);
    if(ret == -1) {
      return logger_->error("truncate_file: truncate buffer_path failed");
    }

    ret = set_size(buffer_path, truncate_size);
    if(ret != 0) {
      return logger_->error("truncate_file: set_size failed");
    }

    // logger_->info("truncate_file: success");
    return 0;
  }

  std::vector<Chunk> chunks;
  ret = get_chunkinfo(main_path, chunks);
  if(ret != 0) {
    return logger_->error("truncate_file: get_chunkinfo failed");
  }

  auto op_fd = open(buffer_path.c_str(), O_RDWR);

  if(truncate_size <= state_->threshold) {
    // logger_->info("truncate_file: truncate to local");
    // truncate will cause the file to be local
    auto truncate_point_idx = get_chunk_idx(chunks, truncate_size);
    // load 0 to truncate_point_idx chunk
    buffer_controller_->clear_file(buffer_path);
    auto cur_offset = 0;
    for(int i = 0; i <= truncate_point_idx; i++) {
      auto& chunk = chunks[i];
      auto ret = buffer_controller_->download_chunk(chunk.key_, op_fd, cur_offset);
      if(ret != 0) {
        return logger_->error("truncate_file: download_chunk failed");
      }
      cur_offset += chunk.len_;
    }

    ret = ftruncate(op_fd, truncate_size);
    if(ret == -1) {
      return logger_->error("truncate_file: truncate(local) buffer_path failed");
    }
    
    // delete all chunks
    for(auto& c: chunks) {
      auto is_last = chunk_table_.Release(c.key_);
      if(is_last) {
        ret = buffer_controller_->delete_object(c.key_);
      }
    }
    chunks.clear();
    ret = set_chunkinfo(main_path, chunks); // set empty chunkinfo
    if(ret != 0) {
      return logger_->error("truncate_file: set_chunkinfo failed");
    }

    ret = set_truncated(main_path, true);
    if(ret != 0) {
      return logger_->error("truncate_file: set_truncated failed");
    }

    ret = set_size(buffer_path, truncate_size);
    if(ret != 0) {
      return logger_->error("truncate_file: set_size failed");
    }

    // logger_->info("truncate_file: chunk count " + std::to_string(chunks.size()));
    // for(auto& c: chunks) {
    //   logger_->info("truncate_file: chunk start " + std::to_string(c.start_) + ", len " + std::to_string(c.len_) + ", key " + c.key_);
    // }

    // logger_->info("truncate_file: success");
    return 0;
  }
  // file is still on cloud after truncate
  // logger_->info("truncate_file: truncate to cloud");

  auto truncate_point_idx = get_chunk_idx(chunks, truncate_size);
  assert(truncate_point_idx != -1);
  if(chunks[truncate_point_idx].start_ + (off_t)chunks[truncate_point_idx].len_ == truncate_size) {
    // truncate at the end of a chunk
    truncate_point_idx++;
  }
  assert(truncate_point_idx < (int)chunks.size());

  // load truncate_point_idx chunk
  buffer_controller_->clear_file(op_fd);
  ret = buffer_controller_->download_chunk(chunks[truncate_point_idx].key_, op_fd, 0);
  if(ret != 0) {
    return logger_->error("truncate_file: download_chunk failed");
  }
  auto buffer_offset = chunks[truncate_point_idx].start_;

  // truncate buffer file
  ret = ftruncate(op_fd, truncate_size - buffer_offset);
  if(ret == -1) {
    return logger_->error("truncate_file: truncate(cloud buf) buffer_path failed");
  }

  // delete all chunks after truncate_point_idx(inclusive)
  for(int i = truncate_point_idx; i < (int)chunks.size(); i++) {
    auto is_last = chunk_table_.Release(chunks[i].key_);
    if(is_last) {
      ret = buffer_controller_->delete_object(chunks[i].key_);
    }
  }

  char buf[RECHUNK_BUF_SIZE];
  auto new_chunk_len = truncate_size - chunks[truncate_point_idx].start_;
  chunks.resize(truncate_point_idx); // remove all chunks after truncate_point_idx(inclusive)

  ssize_t read_p = 0;
  while(read_p < new_chunk_len) {
    auto read_cnt = pread(op_fd, buf, RECHUNK_BUF_SIZE, read_p);
    if(read_cnt < 0) {
      return logger_->error("truncate_file: pread failed");
    }
    auto next_chunks = chunk_splitter_.get_chunks_next(buf, read_cnt);
    for(auto& c: next_chunks) {
      auto is_first = chunk_table_.Use(c.key_);
      if(is_first) {
        // logger_->info("truncate_file: upload chunk start " + std::to_string(c.start_) + ", len " + std::to_string(c.len_) + ", key " + c.key_);
        ret = buffer_controller_->upload_chunk(c.key_, op_fd, c.start_ - buffer_offset, c.len_);
      }
      chunks.push_back(c);
    }
    read_p += read_cnt;
  }

  auto last_chunk = chunk_splitter_.get_chunk_last();
  if(last_chunk.len_ > 0) {
    auto is_first = chunk_table_.Use(last_chunk.key_);
    if(is_first) {
      // logger_->info("truncate_file: upload chunk start " + std::to_string(last_chunk.start_) + ", len " + std::to_string(last_chunk.len_) + ", key " + last_chunk.key_);
      ret = buffer_controller_->upload_chunk(last_chunk.key_, op_fd, last_chunk.start_ - buffer_offset, last_chunk.len_);
    }
    chunks.push_back(last_chunk);
  }

  ret = set_chunkinfo(main_path, chunks);
  if(ret != 0) {
    return logger_->error("truncate_file: set_chunkinfo failed");
  }

  ret = set_truncated(main_path, true);
  if(ret != 0) {
    return logger_->error("truncate_file: set_truncated failed");
  }

  ret = set_size(buffer_path, truncate_size);
  if(ret != 0) {
    return logger_->error("truncate_file: set_size failed");
  }

  // logger_->info("truncate_file: chunk count " + std::to_string(chunks.size()));
  // for(auto& c: chunks) {
  //   logger_->info("truncate_file: chunk start " + std::to_string(c.start_) + ", len " + std::to_string(c.len_) + ", key " + c.key_);
  // }

  // logger_->info("truncate_file: success");
  return 0;
}

void CloudfsControllerDedup::destroy() {
  chunk_table_.Persist();
}

int CloudfsControllerDedup::prepare_read_data(off_t offset, size_t r_size, uint64_t fd) {
  // clear buffer file
  auto op_fd = open_files_[fd].op_fd_;

  auto ret = buffer_controller_->clear_file(op_fd);
  if(ret != 0) {
    return logger_->error("prepare_read_data: clear_file failed");
  }
  
  auto& chunks = open_files_[fd].chunks_;
  auto buffer_len = 0;
  if(offset >= chunks.back().start_ + (off_t)chunks.back().len_) {
    // logger_->info("prepare_read_data: read after the end of file");
    open_files_[fd].start_ = chunks.back().start_ + chunks.back().len_;
    open_files_[fd].len_ = 0;
    return 0; // after the end of file
  }

  auto read_start_idx = get_chunk_idx(chunks, offset);
  assert(read_start_idx != -1);

  auto read_end_idx = get_chunk_idx(chunks, offset + r_size - 1);
  if(read_end_idx == -1) {
    read_end_idx = chunks.size() - 1;
  }

  for(int i = read_start_idx; i <= read_end_idx; i++) {
    auto& chunk = chunks[i];
    // logger_->info("prepare_read_data: download chunk start " + std::to_string(chunk.start_) + ", len " + std::to_string(chunk.len_) + ", key " + chunk.key_);
    // download chunk
    auto ret = buffer_controller_->download_chunk(chunk.key_, op_fd, buffer_len);
    // struct stat stbuf;
    // ret = lstat(buffer_path.c_str(), &stbuf);
    // if(ret == -1) {
    //   return logger_->error("prepare_read_data: stat buffer_path failed");
    // }
    // logger_->info("prepare_read_data: buffer size " + std::to_string(stbuf.st_size));

    if(ret != 0) {
      return logger_->error("prepare_read_data: download_chunk failed");
    }
    buffer_len += chunk.len_;
  }

  open_files_[fd].start_ = chunks[read_start_idx].start_;
  open_files_[fd].len_ = buffer_len;
  // logger_->info("prepare_read_data: buffer_offset " + std::to_string(open_files_[fd].start_) + ", buffer_len " + std::to_string(open_files_[fd].len_));

  return 0;
}

int CloudfsControllerDedup::prepare_write_data(off_t offset, size_t w_size, uint64_t fd, int& rechunk_start_idx, int& buffer_end_idx) {
  // clear buffer file
  auto op_fd = open_files_[fd].op_fd_;

  auto ret = buffer_controller_->clear_file(op_fd);
  if(ret != 0) {
    return logger_->error("prepare_write_data: clear_file failed");
  }
  
  auto& chunks = open_files_[fd].chunks_;
  if(offset >= chunks.back().start_ + (off_t)chunks.back().len_) {
    if(chunks.size() > 0) {
      // read the last chunk, to make convinent for rechunking
      auto& chunk = chunks.back();
      auto ret = buffer_controller_->download_chunk(chunk.key_, op_fd, 0);
      if(ret != 0) {
        return logger_->error("prepare_write_data: download_chunk failed");
      }
      open_files_[fd].start_ = chunk.start_;
      open_files_[fd].len_ = chunk.len_;
      rechunk_start_idx = chunks.size() - 1;
    } else {
      open_files_[fd].start_ = chunks.back().start_ + chunks.back().len_;
      open_files_[fd].len_ = 0;
      rechunk_start_idx = 0;
    }
    buffer_end_idx = -1;
    return 0; // after the end of file
  }

  auto write_start_idx = get_chunk_idx(chunks, offset);
  assert(write_start_idx != -1);
  if(write_start_idx > 0) {
    write_start_idx--;
  }


  // // read previous chunks until get max_seg_size(to compact the chunks, avoid many small chunks due to many small writes)
  // int prev_chunk_len_sum = 0;
  // while(write_start_idx > 0) {
  //   prev_chunk_len_sum += chunks[write_start_idx - 1].len_;
  //   if(prev_chunk_len_sum >= state_->max_seg_size) {
  //     // has collected enough data
  //     break;
  //   }
  //   write_start_idx--;
  // }

  auto write_end_idx = get_chunk_idx(chunks, offset + w_size - 1);
  if(write_end_idx == -1) {
    write_end_idx = chunks.size() - 1;
  }

  auto buffer_len = 0;
  for(int i = write_start_idx; i <= write_end_idx; i++) {
    auto& chunk = chunks[i];
    // download chunk
    auto ret = buffer_controller_->download_chunk(chunk.key_, op_fd, buffer_len);
    if(ret != 0) {
      return logger_->error("prepare_read_data: download_chunk failed");
    }
    buffer_len += chunk.len_;
  }

  open_files_[fd].start_ = chunks[write_start_idx].start_;
  open_files_[fd].len_ = buffer_len;

  rechunk_start_idx = write_start_idx;
  buffer_end_idx = write_end_idx;
  return 0;
}

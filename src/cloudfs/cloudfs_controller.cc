#include "cloudfs_controller.h"
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

  int num_chunks;
  fread(&num_chunks, sizeof(int), 1, file);

  chunks.clear();
  chunks.reserve(num_chunks);
  for(int i = 0; i < num_chunks; i++) {
    off_t start;
    size_t len;
    fread(&start, sizeof(off_t), 1, file);
    fread(&len, sizeof(size_t), 1, file);
    size_t key_len;
    fread(&key_len, sizeof(size_t), 1, file);
    std::vector<char> key(key_len);
    fread(key.data(), sizeof(char), key_len, file);
    chunks.emplace_back(start, len, key);
  }
  fclose(file);

  return 0;
}

int CloudfsController::set_chunkinfo(const std::string& main_path, std::vector<Chunk>& chunks) {
  FILE* file = fopen(main_path.c_str(), "w");
  if(file == NULL) {
    return -1;
  }

  int num_chunks = chunks.size();
  fwrite(&num_chunks, sizeof(int), 1, file);

  for(int i = 0; i < num_chunks; i++) {
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

CloudfsController::CloudfsController(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger) 
  : state_(state), bucket_name_(std::move(bucket_name)), logger_(std::move(logger)), buffer_controller_(std::make_shared<BufferController>(host_name, bucket_name_, logger_)) {

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
  std::vector<Chunk> chunks;
  ret = set_chunkinfo(main_path, chunks); // no chunk initially

  logger_->info("create_file: create main_path success");

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

  logger_->info("create_file: success");

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

const size_t CloudfsControllerDedup::BUFFER_SIZE_MAX = 64 * 1024;

CloudfsControllerDedup::CloudfsControllerDedup(struct cloudfs_state *state, const std::string& host_name, std::string bucket_name, 
      std::shared_ptr<DebugLogger> logger, int window_size, int avg_seg_size, int min_seg_size, int max_seg_size):
  CloudfsController(state, host_name, std::move(bucket_name), std::move(logger)), chunk_table_("chunk_table"),
  chunk_splitter_(window_size, avg_seg_size, min_seg_size, max_seg_size) { 
    
}

CloudfsControllerDedup::~CloudfsControllerDedup() {
}

int CloudfsControllerDedup::open_file(const std::string& path, int flags, uint64_t* fd) {
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

  // open buffer file
  ret = open(buffer_path.c_str(), flags & ~(O_CREAT | O_EXCL));
  if(ret == -1) {
    return logger_->error("open_file: open buffer_path failed");
  }

  *fd = ret;

  struct stat buf_st;
  ret = fstat(*fd, &buf_st);
  if(ret == -1) {
    return logger_->error("open_file: fstat buffer_path failed");
  }

  std::vector<Chunk> chunks;
  ret = get_chunkinfo(main_path, chunks);
  if(ret != 0) {
    return logger_->error("open_file: get_chunkinfo failed");
  }

  open_files_[*fd] = OpenFile(main_path, 0, buf_st.st_size, false, chunks);

  return 0;
}

int CloudfsControllerDedup::read_file(const std::string& path, uint64_t fd, char* buf, size_t size, off_t offset) {
  logger_->info("read_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(size) + ", offset: " + std::to_string(offset));

  auto main_path = open_files_[fd].main_path_;

  std::string buffer_path;
  auto ret = get_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("read_file: get_buffer_path failed");
  }

  auto ret = prepare_read_data(buffer_path, offset, size, fd);
  if(ret < 0) {
    logger_->error("read_file: prepare_read_data failed");
    return ret;
  }

  auto start = open_files_[fd].start_;
  auto read_cnt = pread(fd, buf, size, offset - start);
  if(read_cnt < 0) {
    return logger_->error("read_file: pread failed");
  }

  logger_->info("read_file: success, read " + std::to_string(total_read) + " bytes");
  return read_cnt;
}

int CloudfsControllerDedup::write_file(const std::string& path, uint64_t fd, const char* buf, size_t size, off_t offset) {
  logger_->info("write_file: " + path + ", fd: " + std::to_string(fd) + ", size: " + std::to_string(size) + ", offset: " + std::to_string(offset));
  
  auto main_path = open_files_[fd].main_path_;
  
  std::string buffer_path;
  auto ret = get_buffer_path(main_path, buffer_path);
  if(ret != 0) {
    return logger_->error("write_file: get_buffer_path failed");
  }

  size_t rechunk_start, buffer_end;
  auto ret = prepare_write_data(buffer_path, offset, size, fd, rechunk_start, buffer_end);
  if(ret < 0) {
    logger_->error("write_file: prepare_write_data failed");
    return ret;
  }

  auto start = open_files_[fd].start_;
  auto buffer_len = open_files_[fd].len_;

  auto written = pwrite(fd, cur, size, offset - start);
  if(written < 0) {
    return logger_->error("write_file: pwrite failed");
  }

  buffer_len = std::max(buffer_len, (size_t)(offset - start + written));
  open_files_[fd].len_ = buffer_len;

  auto& chunks = open_files_[fd].chunks_;
  if(buffer_end == -1) {
    chunks.resize(rechunk_start);
    chunk_splitter_.init(start);
    char split_buf[4096];
    off_t cur = 0;
    while(cur < buffer_len) {
      ret = pread(fd, split_buf, 4096, cur);
      if(ret < 0) {
        return logger_->error("write_file: pread failed");
      }
      auto new_chunks = chunk_splitter_.get_chunks_next(split_buf, ret);
      for(auto& chunk : new_chunks) {
        chunks.push_back(chunk);
      }
      cur += ret;
    }
  } else {
    std::vector<Chunk> new_chunks;
    chunk_splitter_.init(start);
    char split_buf[4096];
    off_t cur = 0;
    while(cur < buffer_len) {
      ret = pread(fd, split_buf, 4096, cur);
      if(ret < 0) {
        return logger_->error("write_file: pread failed");
      }
      auto next_chunks = chunk_splitter_.get_chunks_next(split_buf, ret);
      new_chunks.insert(new_chunks.end(), next_chunks.begin(), next_chunks.end());
    }
    if(new_chunks.size() > 0 && chunks[buffer_end].start_ + chunks[buffer_end].len_ == new_chunks.back().start_ + new_chunks.back().len_) {
      chunks.resize(rechunk_start);
      chunks.insert(chunks.end(), new_chunks.begin(), new_chunks.end());
    } else {
      // rechunk following chunks, start from buffer_end + 1
      auto cur_idx = buffer_end + 1;
      do {
        // clear buffer file
        ret = buffer_controller_->clear_file(buffer_path);
        if(ret != 0) {
          return logger_->error("write_file: clear_file failed");
        }

        // download chunk
        auto& chunk = chunks[cur_idx];
        ret = buffer_controller_->download_chunk(chunk.key_, buffer_path, 0);
        if(ret != 0) {
          return logger_->error("write_file: download_chunk failed");
        }

        cur = 0;
        while(cur < chunk.len_) {
          ret = pread(fd, split_buf, 4096, cur);
          if(ret < 0) {
            return logger_->error("write_file: pread failed");
          }
          auto next_chunks = chunk_splitter_.get_chunks_next(split_buf, ret);
          new_chunks.insert(new_chunks.end(), next_chunks.begin(), next_chunks.end());
          cur += ret;
        }

        cur_idx++;
      } while(cur_idx < chunks.size() && new_chunks.size() > 0 && chunks[cur_idx - 1].start_ + chunks[cur_idx - 1].len_ != new_chunks.back().start_ + new_chunks.back().len_);

      auto final_chunk = chunk_splitter_->get_chunk_last();
      if(final_chunk.len_ > 0) {
        new_chunks.push_back(final_chunk);
      }

      auto tail_chunks = std::vector<Chunk>(chunks.begin() + cur_idx, chunks.end());
      chunks.resize(rechunk_start);
      chunks.insert(chunks.end(), new_chunks.begin(), new_chunks.end());
      chunks.insert(chunks.end(), tail_chunks.begin(), tail_chunks.end());
    }
  }

  ret = set_chunkinfo(main_path, chunks);
  if(ret != 0) {
    return logger_->error("write_file: set_chunkinfo failed");
  }

  logger_->info("write_file: success, write " + std::to_string(total_written) + " bytes");
  return total_written;
}

int CloudfsControllerDedup::close_file(const std::string& path, uint64_t fd) {
  logger_->info("close_file: " + path + ", fd: " + std::to_string(fd));
  return 0;
}

int CloudfsControllerDedup::unlink_file(const std::string& path) {
  logger_->info("unlink_file: " + path);
  return 0;
}

int CloudfsControllerDedup::truncate_file(const std::string& path, off_t size) {
  logger_->info("truncate_file: " + path + ", size: " + std::to_string(size));
  return 0;
}

int CloudfsControllerDedup::prepare_read_data(const std::string& buffer_path, off_t offset, size_t r_size, uint64_t fd) {
  // clear buffer file
  auto ret = buffer_controller_->clear_file(buffer_path);
  if(ret != 0) {
    return logger_->error("prepare_read_data: clear_file failed");
  }
  
  auto& chunks = open_files_[fd].chunks_;
  auto start = chunks.back().start_ + chunks.back().len_;
  auto buffer_len = 0;
  if(offset >= chunks.back().start_ + chunks.back().len_) {
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
    // download chunk
    auto ret = buffer_controller_->download_chunk(chunk.key_, buffer_path, buffer_len);
    if(ret != 0) {
      return logger_->error("prepare_read_data: download_chunk failed");
    }
    buffer_len += chunk.len_;
  }

  openfiles_[fd].start_ = chunks[read_start_idx].start_;
  openfiles_[fd].len_ = buffer_len;
  
  return 0;
}

int CloudfsControllerDedup::prepare_write_data(const std::string& buffer_path, off_t offset, size_t w_size, uint64_t fd, size_t& rechunk_start, size_t& buffer_end) {
  // clear buffer file
  auto ret = buffer_controller_->clear_file(buffer_path);
  if(ret != 0) {
    return logger_->error("prepare_write_data: clear_file failed");
  }
  
  auto& chunks = open_files_[fd].chunks_;
  auto start = chunks.back().start_ + chunks.back().len_;
  auto buffer_len = 0;
  if(offset >= chunks.back().start_ + chunks.back().len_) {
    if(chunks.size() > 0) {
      // read the last chunk, to make convinent for rechunking
      auto& chunk = chunks.back();
      auto ret = buffer_controller_->download_chunk(chunk.key_, buffer_path, buffer_len);
      if(ret != 0) {
        return logger_->error("prepare_write_data: download_chunk failed");
      }
      open_files_[fd].start_ = chunk.start_;
      open_files_[fd].len_ = chunk.len_;
      rechunk_start = chunks.size() - 1;
    } else {
      rechunk_start = 0;
    }
    buffer_end = -1;
    return 0; // after the end of file
  }

  auto write_start_idx = get_chunk_idx(chunks, offset);
  assert(write_start_idx != -1);

  auto write_end_idx = get_chunk_idx(chunks, offset + w_size - 1);
  if(write_end_idx == -1) {
    write_end_idx = chunks.size() - 1;
  }

  for(int i = write_start_idx; i <= write_end_idx; i++) {
    auto& chunk = chunks[i];
    // download chunk
    auto ret = buffer_controller_->download_chunk(chunk.key_, buffer_path, buffer_len);
    if(ret != 0) {
      return logger_->error("prepare_read_data: download_chunk failed");
    }
    buffer_len += chunk.len_;
  }

  openfiles_[fd].start_ = chunks[write_start_idx].start_;
  openfiles_[fd].len_ = buffer_len;

  rechunk_start = write_start_idx;
  buffer_end = write_end_idx;
  return 0;
}

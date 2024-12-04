#include "buffer_file.h"

#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#include "cloudapi.h"
#include "cloudfs.h"
#include "util.h"

BufferFileController::BufferFileController(struct cloudfs_state *state,
                                   std::string bucket_name,
                                   std::shared_ptr<DebugLogger> logger)
    : bucket_name_(std::move(bucket_name)), logger_(std::move(logger)) {
  // init s3
  cloud_init(state->hostname);
  cloud_print_error(logger_->get_file());
  cloud_create_bucket(bucket_name_.c_str());
  cloud_print_error(logger_->get_file());

  // init cache
  cache_replacer_ = std::make_shared<LRUCacheReplacer>(state, logger_);
  cache_size_ = state->cache_size;
  cache_used_ = 0;
  cache_root_ = std::string(state->ssd_path) + "/.cache";

  logger_->info("BufferFileController: cache_size: " + std::to_string(cache_size_));

  // create cache root if not exists
  if (mkdir(cache_root_.c_str(), 0777) == -1 && errno != EEXIST) {
    logger_->error("BufferFileController: create cache root failed");
  }
  cache_root_ += "/";

  // load cache states across mounts
  DIR *dir = opendir(cache_root_.c_str());
  if (dir == NULL) {
    logger_->error("BufferFileController: open cache root failed, path: " +
                   cache_root_);
    return;
  }

  auto xattr_name_key_len = "user.cloudfs.key_len";
  auto xattr_name_key = "user.cloudfs.key";
  auto xattr_name_size = "user.cloudfs.size";
  auto xattr_name_dirty = "user.cloudfs.dirty";
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    auto full_path = cache_root_ + entry->d_name;
    size_t key_len;
    auto ret = lgetxattr(full_path.c_str(), xattr_name_key_len, &key_len,
                         sizeof(size_t));
    if (ret == -1) {
      continue;
    }
    std::vector<char> key(key_len);
    ret = lgetxattr(full_path.c_str(), xattr_name_key, key.data(), key_len);
    if (ret == -1) {
      continue;
    }
    auto key_str = std::string(key.begin(), key.end());

    size_t size;
    ret = lgetxattr(full_path.c_str(), xattr_name_size, &size, sizeof(size_t));
    if (ret == -1) {
      continue;
    }

    bool dirty;
    ret = lgetxattr(full_path.c_str(), xattr_name_dirty, &dirty, sizeof(bool));
    if (ret == -1) {
      continue;
    }

    cached_objects_[key_str] = CachedObject(size, dirty);
    cache_used_ += size;
  }
  closedir(dir);

  for (auto &object : cached_objects_) {
    logger_->debug("BufferFileController: load cached object: " + object.first +
                   ", size: " + std::to_string(object.second.size_) +
                   ", dirty: " + std::to_string(object.second.dirty_));
  }
  logger_->info("BufferFileController: cache state loaded, cache_used: " +
                std::to_string(cache_used_));
}

FILE *BufferFileController::infile_ = NULL;
FILE *BufferFileController::outfile_ = NULL;
int64_t BufferFileController::infd_ = 0;
int64_t BufferFileController::outfd_ = 0;
off_t BufferFileController::in_offset_ = 0;
off_t BufferFileController::out_offset_ = 0;
std::vector<std::string> BufferFileController::bucket_list_;
std::vector<std::pair<std::string, uint64_t>> BufferFileController::object_list_;

BufferFileController::~BufferFileController() { cloud_destroy(); }

int BufferFileController::download_chunk(const std::string &key, uint64_t fd,
                                     off_t offset, size_t size) {
  if ((int64_t)size > cache_size_) {
    // cannot fit in cache, download directly
    outfd_ = fd;
    out_offset_ = offset;
    cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer_fd);
    cloud_print_error(logger_->get_file());
    return 0;
  }

  auto cached_path = cache_root_ + "." + key;
  if (cached_objects_.find(key) == cached_objects_.end()) {
    // evict cache to make space
    auto ret = evict_to_size(size);
    if (ret != 0) {
      logger_->error(
          "BufferFileController::download_chunk: evict cache to make space failed");
      return ret;
    }

    // download to cache, may create file
    outfd_ = open(cached_path.c_str(), O_CREAT | O_WRONLY, 0777);
    if (outfd_ == -1) {
      return logger_->error(
          "BufferFileController::download_chunk: open cached file failed, path: " +
          cached_path);
    }
    out_offset_ = 0;
    cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer_fd);
    cloud_print_error(logger_->get_file());
    close(outfd_);

    // update cache state
    cached_objects_[key] = CachedObject(size, false);
    cache_used_ += size;
  }

  // open cached chunk file
  outfile_ = fopen(cached_path.c_str(), "r");
  if (outfile_ == NULL) {
    return logger_->error(
        "BufferFileController::download_chunk: open cached file failed, path: " +
        cached_path);
  }
  auto object_size = cached_objects_[key].size_;

  // read from cached file
  char buffer[MEM_BUFFER_LEN + 1];
  size_t read_cnt = 0;
  while (read_cnt < object_size) {
    auto read_size =
        std::min((int)(object_size - read_cnt), MEM_BUFFER_LEN + 1);
    auto ret = fread(buffer, 1, read_size, outfile_);
    if ((int)ret != read_size) {
      return logger_->error("BufferFileController::download_chunk: read from "
                            "cached file failed, path: " +
                            cached_path);
    }
    // copy to fd
    auto write_size = pwrite(fd, buffer, ret, offset + read_cnt);
    if (write_size != (ssize_t)ret) {
      return logger_->error(
          "BufferFileController::download_chunk: write to fd failed, fd: " +
          std::to_string(fd));
    }

    read_cnt += ret;
  }
  fclose(outfile_);

  cache_replacer_->access(key); // update cache replacer
  return 0;
}

int BufferFileController::upload_chunk(const std::string &key, uint64_t fd,
                                   off_t offset, size_t size) {
  if ((int64_t)size > cache_size_) {
    // cannot fit in cache, upload directly
    infd_ = fd;
    in_offset_ = offset;
    cloud_put_object(bucket_name_.c_str(), key.c_str(), size, put_buffer_fd);
    cloud_print_error(logger_->get_file());
    return 0;
  }

  auto cached_path = cache_root_ + "." + key;
  if (cached_objects_.find(key) != cached_objects_.end()) {
    // already in cache
    return 0;
  }

  // evict cache to make space
  auto ret = evict_to_size(size);
  if (ret != 0) {
    logger_->error(
        "BufferFileController::upload_chunk: evict cache to make space failed");
    return ret;
  }

  // create cache file
  FILE *cached_file = fopen(cached_path.c_str(), "w");
  if (cached_file == NULL) {
    return logger_->error("upload_chunk: create cached file failed, path: " +
                          cached_path);
  }
  ret = ftruncate(fileno(cached_file), 0);
  if (ret == -1) {
    return logger_->error(
        "BufferFileController::upload_chunk: truncate cached file failed, path: " +
        cached_path);
  }

  // read from fd and write to cached file
  char buffer[MEM_BUFFER_LEN + 1];
  size_t read_cnt = 0;
  while (read_cnt < size) {
    auto read_size = std::min((int)(size - read_cnt), MEM_BUFFER_LEN + 1);
    auto ret = pread(fd, buffer, read_size, offset + read_cnt);
    if ((int)ret != read_size) {
      return logger_->error(
          "BufferFileController::upload_chunk: read from fd failed, fd: " +
          std::to_string(fd));
    }
    auto write_size = fwrite(buffer, 1, read_size, cached_file);
    if ((int)write_size != read_size) {
      return logger_->error("BufferFileController::upload_chunk: write to cached "
                            "file failed, path: " +
                            cached_path);
    }

    read_cnt += ret;
  }
  fclose(cached_file);

  // update cache state
  cached_objects_[key] = CachedObject(size, true);
  cache_used_ += size;

  cache_replacer_->access(key); // update cache replacer
  return 0;
}

int BufferFileController::download_file(const std::string &key,
                                    const std::string &buffer_path) {
  outfile_ = fopen(buffer_path.c_str(), "w");
  if (outfile_ == NULL) {
    return -1;
  }

  cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer);
  fclose(outfile_);
  cloud_print_error(logger_->get_file());

  return 0;
}

int BufferFileController::upload_file(const std::string &key,
                                  const std::string &buffer_path, size_t size) {
  infile_ = fopen(buffer_path.c_str(), "r");
  if (infile_ == NULL) {
    return -1;
  }

  cloud_put_object(bucket_name_.c_str(), key.c_str(), size, put_buffer);
  fclose(infile_);
  cloud_print_error(logger_->get_file());

  return 0;
}

int BufferFileController::clear_file(const std::string &buffer_path) {
  auto ret = truncate(buffer_path.c_str(), 0);
  if (ret == -1) {
    return -1;
  }

  return 0;
}

int BufferFileController::clear_file(uint64_t fd) {
  auto ret = ftruncate(fd, 0);
  if (ret == -1) {
    return -1;
  }

  return 0;
}

int BufferFileController::delete_object(const std::string &key) {
  if (cached_objects_.find(key) != cached_objects_.end()) {
    // found in cache, delete cached file
    cache_used_ -= cached_objects_[key].size_;
    auto dirty = cached_objects_[key].dirty_;
    cached_objects_.erase(key);

    auto cached_path = cache_root_ + "." + key;
    remove(cached_path.c_str());

    cache_replacer_->remove(key);

    if (dirty) {
      // dirty means the object hasn't been uploaded to cloud, no need to delete
      // the object on cloud
      return 0;
    }
  }

  // delete object on cloud
  cloud_delete_object(bucket_name_.c_str(), key.c_str());
  cloud_print_error(logger_->get_file());
  return 0;
}

int BufferFileController::persist_cache_state() {
  cache_replacer_->persist(); // persist cache replacer

  // write state to xattr of cached files
  auto xattr_name_key_len = "user.cloudfs.key_len";
  auto xattr_name_key = "user.cloudfs.key";
  auto xattr_name_size = "user.cloudfs.size";
  auto xattr_name_dirty = "user.cloudfs.dirty";
  for (auto &objects : cached_objects_) {
    auto path = cache_root_ + "." + objects.first;
    auto key_len = objects.first.size();
    auto ret = lsetxattr(path.c_str(), xattr_name_key_len, &key_len,
                         sizeof(size_t), 0);
    if (ret == -1) {
      return logger_->error(
          "BufferFileController::persist_cache_state: set xattr failed, path: " +
          path);
    }
    ret = lsetxattr(path.c_str(), xattr_name_key, objects.first.c_str(),
                    objects.first.size(), 0);
    if (ret == -1) {
      return logger_->error(
          "BufferFileController::persist_cache_state: set xattr failed, path: " +
          path);
    }
    ret = lsetxattr(path.c_str(), xattr_name_size, &objects.second.size_,
                    sizeof(size_t), 0);
    if (ret == -1) {
      return logger_->error(
          "BufferFileController::persist_cache_state: set xattr failed, path: " +
          path);
    }

    if (objects.second.dirty_) {
      // object hasn't been uploaded, upload to cloud

      infd_ = open(path.c_str(), O_RDONLY);
      if (infd_ == -1) {
        return logger_->error(
            "BufferFileController::persist_cache_state: open cached file failed, \
            path: " + path);
      }
      in_offset_ = 0;
      cloud_put_object(bucket_name_.c_str(), objects.first.c_str(),
                       objects.second.size_, put_buffer_fd);
      cloud_print_error(logger_->get_file());
      close(infd_);

      objects.second.dirty_ = false;
    }

    ret = lsetxattr(path.c_str(), xattr_name_dirty, &objects.second.dirty_,
                    sizeof(bool), 0);
    if (ret == -1) {
      return logger_->error(
          "BufferFileController::persist_cache_state: set xattr failed, path: " +
          path);
    }
    logger_->debug(
        "BufferFileController::persist_cache_state: persist cached object: " +
        objects.first + ", size: " + std::to_string(objects.second.size_) +
        ", dirty: " + std::to_string(objects.second.dirty_));
  }

  return 0;
}

void BufferFileController::print_cache() { cache_replacer_->print_cache(); }

int BufferFileController::evict_to_size(size_t required_size) {
  while (cache_size_ - cache_used_ < (int64_t)required_size) {
    std::string to_evict;
    cache_replacer_->evict(to_evict);

    auto victim_path = cache_root_ + "." + to_evict;
    auto dirty = cached_objects_[to_evict].dirty_;

    if (dirty) {
      // write back to cloud
      infd_ = open(victim_path.c_str(), O_RDONLY);
      if (infd_ == -1) {
        return logger_->error("evict_to_size: open cached file failed, path: " +
                              victim_path);
      }
      in_offset_ = 0;
      cloud_put_object(bucket_name_.c_str(), to_evict.c_str(),
                       cached_objects_[to_evict].size_, put_buffer_fd);
      cloud_print_error(logger_->get_file());
      close(infd_);
    }

    // delete local file
    remove(victim_path.c_str());

    // update cache state
    cache_used_ -= cached_objects_[to_evict].size_;
    cached_objects_.erase(to_evict);
  }
  return 0;
}

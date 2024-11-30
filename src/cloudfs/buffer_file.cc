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

BufferController::BufferController(struct cloudfs_state *state,
                                   std::string bucket_name,
                                   std::shared_ptr<DebugLogger> logger)
    : bucket_name_(std::move(bucket_name)), logger_(std::move(logger)) {
  // logger_->info("BufferController: " + bucket_name_);

  cloud_init(state->hostname);
  cloud_print_error(logger_->get_file());

  cloud_create_bucket(bucket_name_.c_str());
  cloud_print_error(logger_->get_file());

  // cache_replacer_ = std::make_shared<LRUKCacheReplacer>(2, state, logger_);
  cache_replacer_ = std::make_shared<LRUCacheReplacer>(state, logger_);
  cache_size_ = state->cache_size;
  cache_used_ = 0;
  cache_root_ = std::string(state->ssd_path) + "/.cache";

  logger_->info("BufferController: cache_size: " + std::to_string(cache_size_));

  // create cache root if not exists
  if (mkdir(cache_root_.c_str(), 0777) == -1 && errno != EEXIST) {
    logger_->error("BufferController: create cache root failed");
  }

  cache_root_ += "/";

  // load cache state
  DIR *dir = opendir(cache_root_.c_str());
  if (dir == NULL) {
    logger_->error("BufferController: open cache root failed, path: " +
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

    cached_objects[key_str] = CachedObject(size, dirty);
    cache_used_ += size;
  }

  closedir(dir);

  for (auto &object : cached_objects) {
    logger_->debug("BufferController: load cached object: " + object.first +
                   ", size: " + std::to_string(object.second.size_) +
                   ", dirty: " + std::to_string(object.second.dirty_));
  }

  logger_->info("BufferController: cache state loaded, cache_used: " +
                std::to_string(cache_used_));

  // bucket_list.clear();
  // cloud_list_service(list_service);
  // cloud_print_error(logger_->get_file());

  // for(auto& bucket : bucket_list) {
  //     logger_->info("Bucket: " + bucket);
  // }
}

FILE *BufferController::infile = NULL;
FILE *BufferController::outfile = NULL;
int64_t BufferController::infd = 0;
int64_t BufferController::outfd = 0;
off_t BufferController::in_offset = 0;
off_t BufferController::out_offset = 0;
std::vector<std::string> BufferController::bucket_list;
std::vector<std::pair<std::string, uint64_t>> BufferController::object_list;

BufferController::~BufferController() { cloud_destroy(); }

int BufferController::download_chunk(const std::string &key, uint64_t fd,
                                     off_t offset, size_t size) {
  logger_->debug("BufferController::download_chunk: " + key + ", offset: " +
                 std::to_string(offset) + ", size: " + std::to_string(size));

  if ((int64_t)size > cache_size_) {
    // cannot fit in cache, download directly
    // logger_->debug("BufferController::download_chunk: download directly, size: " +
    //                std::to_string(size));
    outfd = fd;
    out_offset = offset;
    cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer_fd);
    cloud_print_error(logger_->get_file());
    return 0;
  }

  auto cached_path = cache_root_ + "." + key;
  if (cached_objects.find(key) == cached_objects.end()) {
    // logger_->debug("BufferController::download_chunk: download from cloud");

    // evict cache to make space
    auto ret = evict_to_size(size);
    if (ret != 0) {
      logger_->error("BufferController::download_chunk: evict cache to make space failed");
      return ret;
    }

    // download to cache, may create file
    outfd = open(cached_path.c_str(), O_CREAT | O_WRONLY, 0777);
    if (outfd == -1) {
      return logger_->error("BufferController::download_chunk: open cached file failed, path: " +
                            cached_path);
    }
    out_offset = 0;
    cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer_fd);
    cloud_print_error(logger_->get_file());
    close(outfd);

    // update cache state
    cached_objects[key] = CachedObject(size, false);
    cache_used_ += size;
  }

  outfile = fopen(cached_path.c_str(), "r");
  if (outfile == NULL) {
    return logger_->error("BufferController::download_chunk: open cached file failed, path: " +
                          cached_path);
  }
  auto object_size = cached_objects[key].size_;

  char buffer[4097];
  size_t read_cnt = 0;
  while (read_cnt < object_size) {
    auto read_size = std::min((int)(object_size - read_cnt), 4097);
    auto ret = fread(buffer, 1, read_size, outfile);
    if ((int)ret != read_size) {
      return logger_->error(
          "BufferController::download_chunk: read from cached file failed, path: " + cached_path);
    }
    // copy to fd
    auto write_size = pwrite(fd, buffer, ret, offset + read_cnt);
    if (write_size != (ssize_t)ret) {
      return logger_->error("BufferController::download_chunk: write to fd failed, fd: " +
                            std::to_string(fd));
    }

    read_cnt += ret;
  }
  fclose(outfile);

  cache_replacer_->Access(key); // update cache replacer
  // logger_->debug("BufferController::download_chunk: success, key: " + key +
  //                ", size: " + std::to_string(size) + ", available space: " +
  //                std::to_string(cache_size_ - cache_used_));
  return 0;
}

int BufferController::upload_chunk(const std::string &key, uint64_t fd,
                                   off_t offset, size_t size) {
  // logger_->debug("BufferController::upload_chunk: " + key + ", offset: " +
  //                std::to_string(offset) + ", size: " + std::to_string(size));

  if ((int64_t)size > cache_size_) {
    // cannot fit in cache, upload directly
    // logger_->debug("BufferController::upload_chunk: upload directly, size: " +
    //                std::to_string(size));
    infd = fd;
    in_offset = offset;
    cloud_put_object(bucket_name_.c_str(), key.c_str(), size, put_buffer_fd);
    cloud_print_error(logger_->get_file());
    return 0;
  }

  auto cached_path = cache_root_ + "." + key;
  if (cached_objects.find(key) != cached_objects.end()) {
    // logger_->debug("BufferController::upload_chunk: found in cache, size: " + std::to_string(cached_objects[key].size_));
    return 0;
  }

  // logger_->debug("BufferController::upload_chunk: not in cache, evict cache to make space");

  // evict cache to make space
  auto ret = evict_to_size(size);
  if (ret != 0) {
    logger_->error("BufferController::upload_chunk: evict cache to make space failed");
    return ret;
  }

  // create cache file
  FILE *cached_file = fopen(cached_path.c_str(), "w");
  if (cached_file == NULL) {
    return logger_->error("upload_chunk: create cached file failed, path: " +
                          cached_path);
  }

  // truncate to 0
  ret = ftruncate(fileno(cached_file), 0);
  if (ret == -1) {
    return logger_->error("BufferController::upload_chunk: truncate cached file failed, path: " +
                          cached_path);
  }

  // read from fd and write to cached file
  char buffer[4097];
  size_t read_cnt = 0;
  while (read_cnt < size) {
    auto read_size = std::min((int)(size - read_cnt), 4097);
    auto ret = pread(fd, buffer, read_size, offset + read_cnt);
    if ((int)ret != read_size) {
      return logger_->error("BufferController::upload_chunk: read from fd failed, fd: " +
                            std::to_string(fd));
    }
    auto write_size = fwrite(buffer, 1, read_size, cached_file);
    if ((int)write_size != read_size) {
      return logger_->error(
          "BufferController::upload_chunk: write to cached file failed, path: " + cached_path);
    }

    read_cnt += ret;
  }
  fclose(cached_file);

  // update cache state
  cached_objects[key] = CachedObject(size, true);
  cache_used_ += size;
  cache_replacer_->Access(key); // update cache replacer

  // logger_->debug(
  //     "BufferController::upload_chunk: success, key: " + key + ", size: " + std::to_string(size) +
  //     ", available space: " + std::to_string(cache_size_ - cache_used_));
  return 0;
}

int BufferController::download_file(const std::string &key,
                                    const std::string &buffer_path) {
  // logger_->info("download_file: " + key + " to " + buffer_path);

  outfile = fopen(buffer_path.c_str(), "w");
  if (outfile == NULL) {
    return -1;
  }

  cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer);
  fclose(outfile);
  cloud_print_error(logger_->get_file());

  return 0;
}

int BufferController::upload_file(const std::string &key,
                                  const std::string &buffer_path, size_t size) {
  // logger_->info("upload_file: " + buffer_path + " to " + key + ", size: " +
  // std::to_string(size));

  infile = fopen(buffer_path.c_str(), "r");
  if (infile == NULL) {
    return -1;
  }

  cloud_put_object(bucket_name_.c_str(), key.c_str(), size, put_buffer);
  fclose(infile);
  cloud_print_error(logger_->get_file());

  return 0;
}

int BufferController::clear_file(const std::string &buffer_path) {
  // logger_->info("clear_file: " + buffer_path);

  auto ret = truncate(buffer_path.c_str(), 0);
  if (ret == -1) {
    return -1;
  }

  return 0;
}

int BufferController::clear_file(uint64_t fd) {
  // logger_->info("clear_file: " + std::to_string(fd));

  auto ret = ftruncate(fd, 0);
  if (ret == -1) {
    return -1;
  }

  return 0;
}

int BufferController::delete_object(const std::string &key) {
  // logger_->debug("BufferController::delete_object: " + key);

  if (cached_objects.find(key) != cached_objects.end()) {
    // logger_->debug("BufferController::delete_object: found in cache, size: " +
    //                std::to_string(cached_objects[key].size_));
    cache_used_ -= cached_objects[key].size_;
    auto dirty = cached_objects[key].dirty_;
    cached_objects.erase(key);

    auto cached_path = cache_root_ + "." + key;
    remove(cached_path.c_str());

    cache_replacer_->Remove(key);

    if (dirty) {
      // dirty means the object hasn't been uploaded to cloud, no need to delete
      // it from cloud
      return 0;
    }
  }

  // logger_->debug("BufferController::delete_object: delete from cloud");

  // delete from cloud
  cloud_delete_object(bucket_name_.c_str(), key.c_str());
  cloud_print_error(logger_->get_file());

  return 0;
}

int BufferController::persist_cache_state() {
  cache_replacer_->Persist();

  // write state to xattr of cached files
  auto xattr_name_key_len = "user.cloudfs.key_len";
  auto xattr_name_key = "user.cloudfs.key";
  auto xattr_name_size = "user.cloudfs.size";
  auto xattr_name_dirty = "user.cloudfs.dirty";
  for (auto &objects : cached_objects) {
    auto path = cache_root_ + "." + objects.first;
    auto key_len = objects.first.size();
    auto ret = lsetxattr(path.c_str(), xattr_name_key_len, &key_len,
                         sizeof(size_t), 0);
    if (ret == -1) {
      return logger_->error("BufferController::persist_cache_state: set xattr failed, path: " +
                            path);
    }
    ret = lsetxattr(path.c_str(), xattr_name_key, objects.first.c_str(),
                    objects.first.size(), 0);
    if (ret == -1) {
      return logger_->error("BufferController::persist_cache_state: set xattr failed, path: " +
                            path);
    }
    ret = lsetxattr(path.c_str(), xattr_name_size, &objects.second.size_,
                    sizeof(size_t), 0);
    if (ret == -1) {
      return logger_->error("BufferController::persist_cache_state: set xattr failed, path: " +
                            path);
    }

    // if (objects.second.dirty_) {
    //   // flush to cloud
    //   logger_->debug("BufferController::persist_cache_state: flush to cloud, key: " + objects.first + ", size: " + std::to_string(objects.second.size_));

    //   infd = open(path.c_str(), O_RDONLY);
    //   if (infd == -1) {
    //     return logger_->error(
    //         "BufferController::persist_cache_state: open cached file failed, path: " + path);
    //   }
    //   in_offset = 0;
    //   cloud_put_object(bucket_name_.c_str(), objects.first.c_str(),
    //                    objects.second.size_, put_buffer_fd);
    //   cloud_print_error(logger_->get_file());
    //   close(infd);

    //   objects.second.dirty_ = false;
    // }

    ret = lsetxattr(path.c_str(), xattr_name_dirty, &objects.second.dirty_,
                    sizeof(bool), 0);
    if (ret == -1) {
      return logger_->error("BufferController::persist_cache_state: set xattr failed, path: " +
                            path);
    }

    logger_->debug(
        "BufferController::persist_cache_state: persist cached object: " + objects.first +
        ", size: " + std::to_string(objects.second.size_) +
        ", dirty: " + std::to_string(objects.second.dirty_));
  }

  return 0;
}

void BufferController::PrintCache() {
  cache_replacer_->PrintCache();
}

int BufferController::evict_to_size(size_t required_size) {
  logger_->debug("BufferController::evict_to_size: required_size: " +
                 std::to_string(required_size) + ", current available space: " +
                 std::to_string(cache_size_ - cache_used_));
  while (cache_size_ - cache_used_ < (int64_t)required_size) {
    std::string to_evict;
    cache_replacer_->Evict(to_evict);

    logger_->debug("BufferController::evict_to_size: evict key: " + to_evict +
                   ", size: " + std::to_string(cached_objects[to_evict].size_));

    auto victim_path = cache_root_ + "." + to_evict;
    auto dirty = cached_objects[to_evict].dirty_;
    logger_->debug("BufferController::evict_to_size: dirty: " + std::to_string(dirty));

    
    if (dirty) {
      // write back to cloud
      infd = open(victim_path.c_str(), O_RDONLY);
      if (infd == -1) {
        return logger_->error("evict_to_size: open cached file failed, path: " +
                              victim_path);
      }
      in_offset = 0;
      cloud_put_object(bucket_name_.c_str(), to_evict.c_str(),
                       cached_objects[to_evict].size_, put_buffer_fd);
      cloud_print_error(logger_->get_file());
      close(infd);
      // logger_->debug(
      //     "BufferController::evict_to_size: write back to cloud, key: " + to_evict +
      //     ", size: " + std::to_string(cached_objects[to_evict].size_));
    }

    // delete local file
    remove(victim_path.c_str());

    // logger_->debug("BufferController::evict_to_size: delete local chunk, key: " + to_evict +
    //                ", size: " + std::to_string(cached_objects[to_evict].size_));
    cache_used_ -= cached_objects[to_evict].size_;
    cached_objects.erase(to_evict);
    logger_->debug("BufferController::evict_to_size: update cache used, new used: " +
                   std::to_string(cache_used_));
  }

  // logger_->debug("BufferController::evict_to_size: success, required_size: " +
  //                std::to_string(required_size) + ", current available space: " +
  //                std::to_string(cache_size_ - cache_used_));

  return 0;
}

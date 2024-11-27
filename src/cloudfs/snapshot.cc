#include "snapshot.h"
#include <cerrno>
#include <queue>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <climits>
#include <algorithm>
#include <dirent.h>
#include <unordered_set>

#include "snapshot-api.h"

SnapshotController::SnapshotController(
    struct cloudfs_state *state, std::shared_ptr<DebugLogger> logger,
    std::shared_ptr<CloudfsController> cloudfs_controller)
    : state_(state), logger_(logger), cloudfs_controller_(cloudfs_controller) {
    snapshot_stub_path_ = std::string(state_->ssd_path) + "/.snapshot";

    
}

SnapshotController::~SnapshotController() {}

int SnapshotController::create_snapshot(unsigned long *timestamp) {
  logger_->debug("SnapshotController::create_snapshot: create snapshot, " + std::to_string(*timestamp));
  int count;
  if (get_snapshot_count(count) != 0) {
    return logger_->error(
        "SnapshotController::generate_snapshot: get snapshot count failed");
  }
  if (count >= CLOUDFS_MAX_NUM_SNAPSHOTS) {
    // max number of snapshots reached
    errno = EINVAL;
    return logger_->error("SnapshotController::generate_snapshot: max number "
                          "of snapshots reached");
  }

  int installed_count;
  if (get_installed_snapshot_count(installed_count) != 0) {
    return logger_->error("SnapshotController::generate_snapshot: get "
                          "installed snapshot count failed");
  }
  if (installed_count > 0) {
    // currently has installed snapshot
    errno = EINVAL;
    return logger_->error("SnapshotController::generate_snapshot: currently "
                          "has installed snapshot");
  }

  std::vector<unsigned long> snapshot_list;
  if (get_snapshot_list(snapshot_list) != 0) {
    return logger_->error(
        "SnapshotController::generate_snapshot: get snapshot list failed");
  }
  if (snapshot_list.size() > 0) {
    for (auto &ts : snapshot_list) {
      if (ts == *timestamp) {
        // timestamp already exists
        errno = EINVAL;
        return logger_->error(
            "SnapshotController::generate_snapshot: timestamp already exists");
      }
    }
  }
  logger_->debug("SnapshotController::create_snapshot: start creating snapshot " + std::to_string(*timestamp));

  // TODO: create snapshot
  auto tmp_path = std::string(state_->ssd_path) + "/.snapshot_tmp";
  FILE* tmp_file = fopen(tmp_path.c_str(), "w");
  if(tmp_file == NULL) {
    return logger_->error("SnapshotController::generate_snapshot: open tmp file failed");
  }

  // write a entry count to tmp file
  size_t entry_count = 0;
  fwrite(&entry_count, sizeof(size_t), 1, tmp_file);

    // snapshot chunk table
  cloudfs_controller_->get_chunk_table().Snapshot(tmp_file);
  logger_->debug("SnapshotController::create_snapshot: snapshot chunk table done");

  // read all files in ssd path and write to tmp file
  char buf[PATH_MAX + 1];
  std::queue<std::string> dir_queue;
  dir_queue.push(std::string(state_->ssd_path));
  while (!dir_queue.empty()) {
    auto dir = dir_queue.front();
    logger_->debug("SnapshotController::create_snapshot: process dir " + dir);
    dir_queue.pop();
    DIR *dirp = opendir(dir.c_str());
    if (dirp == NULL) {
      return logger_->error(
          "SnapshotController::generate_snapshot: opendir failed, " + dir);
    }
    auto entry = readdir(dirp);
    if (entry == NULL) {
      return logger_->error(
          "SnapshotController::generate_snapshot: readdir failed, " + dir);
    }
    do {
      if (strcmp(entry->d_name, "lost+found") == 0) {
        continue;
      }
      auto name = std::string(entry->d_name);
      if (name == "." || name == "..") {
        continue;
      }
      if (is_buffer_path(name)) {
        continue;
      }

      struct stat st;
      if (stat((dir + "/" + name).c_str(), &st) != 0) {
        return logger_->error(
            "SnapshotController::generate_snapshot: stat failed, " + dir + "/" +
            name);
      }

      // write stat struct to tmp file
      fwrite(&st, sizeof(struct stat), 1, tmp_file);
      if (S_ISDIR(st.st_mode)) {
        // write dir path to tmp file
        auto dir_path = dir + "/" + name;
        logger_->debug("SnapshotController::create_snapshot: write dir path " + dir_path);
        size_t dir_path_len = dir_path.size();
        fwrite(&dir_path_len, sizeof(size_t), 1, tmp_file);
        fwrite(dir_path.c_str(), sizeof(char), dir_path_len, tmp_file);

        // add to dir queue
        dir_queue.push(dir + "/" + name);
      } else {
        // write filepath to tmp file
        auto file_path = dir + "/" + name;
        logger_->debug("SnapshotController::create_snapshot: write file path " + file_path);
        size_t file_path_len = file_path.size();
        fwrite(&file_path_len, sizeof(size_t), 1, tmp_file);
        fwrite(file_path.c_str(), sizeof(char), file_path_len, tmp_file);

        // write chunk info to tmp file
        std::vector<Chunk> chunks;
        if (cloudfs_controller_->get_chunkinfo(file_path, chunks) != 0) {
          return logger_->error("SnapshotController::generate_snapshot: get chunk info failed, " + file_path);
        }
        size_t num_chunks = chunks.size();
        fwrite(&num_chunks, sizeof(size_t), 1, tmp_file);
        for(auto& chunk : chunks) {
          fwrite(&chunk.start_, sizeof(off_t), 1, tmp_file);
          fwrite(&chunk.len_, sizeof(size_t), 1, tmp_file);
          size_t key_len = chunk.key_.size();
          fwrite(&key_len, sizeof(size_t), 1, tmp_file);
          fwrite(chunk.key_.c_str(), sizeof(char), key_len, tmp_file);
        }

        // write buffer path to tmp file
        std::string buffer_path;
        if (cloudfs_controller_->get_buffer_path(file_path, buffer_path) != 0) {
          return logger_->error("SnapshotController::generate_snapshot: get buffer path failed, " + file_path);
        }
        size_t buffer_path_len = buffer_path.size();
        fwrite(&buffer_path_len, sizeof(size_t), 1, tmp_file);
        fwrite(buffer_path.c_str(), sizeof(char), buffer_path_len, tmp_file);

        // write file size to tmp file
        off_t file_size;
        if (cloudfs_controller_->get_size(buffer_path, file_size) != 0) {
          return logger_->error("SnapshotController::generate_snapshot: get file size failed, " + file_path);
        }
        fwrite(&file_size, sizeof(size_t), 1, tmp_file);

        // check if need to copy content to tmp file
        if(file_size <= state_->threshold) {
            // small file, copy content to tmp file
            FILE* file = fopen(buffer_path.c_str(), "r");
            if(file == NULL) {
              return logger_->error("SnapshotController::generate_snapshot: open buffer file failed, " + buffer_path);
            }
            size_t read_size = 0;
            while((off_t)read_size < file_size) {
              size_t to_read = std::min(file_size - read_size, sizeof(buf));
              auto n = fread(buf, sizeof(char), to_read, file);
              if(n != to_read) {
                return logger_->error("SnapshotController::generate_snapshot: read buffer file failed, " + buffer_path);
              }
              fwrite(buf, sizeof(char), n, tmp_file);
              read_size += n;
            }
            fclose(file);
        }
      }
      entry_count++;
    } while ((entry = readdir(dirp)) != NULL);
  }

  // write entry count to tmp file
  fseek(tmp_file, 0, SEEK_SET);
  fwrite(&entry_count, sizeof(size_t), 1, tmp_file);
  logger_->debug("SnapshotController::create_snapshot: write entry count: " + std::to_string(entry_count));

  fclose(tmp_file);

  struct stat st;
  if (stat(tmp_path.c_str(), &st) != 0) {
    return logger_->error("SnapshotController::generate_snapshot: stat tmp file failed, " + tmp_path);
  }
  auto object_key = "snapshot_" + std::to_string(*timestamp);

  auto buffer_controller = cloudfs_controller_->get_buffer_controller();
  buffer_controller->upload_file(object_key, tmp_path, st.st_size);
  logger_->debug("SnapshotController::create_snapshot: upload tmp file to buffer done");

  // delete tmp file
  remove(tmp_path.c_str());

  snapshot_list.push_back(*timestamp);
  auto ret = set_snapshot_count(snapshot_list.size());
  if(ret != 0) {
    return logger_->error("SnapshotController::create_snapshot: set snapshot count failed");
  }
  ret = set_snapshot_list(snapshot_list);
  if(ret != 0) {
    return logger_->error("SnapshotController::create_snapshot: set snapshot list failed");
  }

  logger_->debug("SnapshotController::create_snapshot: create snapshot done");

  return 0;
}

int SnapshotController::restore_snapshot(unsigned long *timestamp) { 
    logger_->debug("SnapshotController::restore_snapshot: restore snapshot, " + std::to_string(*timestamp));
    std::vector<unsigned long> snapshot_list;
    if (get_snapshot_list(snapshot_list) != 0) {
        return logger_->error(
            "SnapshotController::generate_snapshot: get snapshot list failed");
    }
    bool found = false;
    for(auto& ts : snapshot_list) {
        if(ts == *timestamp) {
            found = true;
            break;
        }
    }
    if(!found) {
        errno = EINVAL;
        return logger_->error("SnapshotController::restore_snapshot: snapshot not found");
    }

    // TODO: restore snapshot
    auto tmp_path = std::string(state_->ssd_path) + "/.snapshot_tmp";
    auto object_key = "snapshot_" + std::to_string(*timestamp);
    auto buffer_controller = cloudfs_controller_->get_buffer_controller();
    buffer_controller->download_file(object_key, tmp_path);
    logger_->debug("SnapshotController::restore_snapshot: download tmp file from buffer done");

    // read tmp file and restore to ssd path
    FILE* tmp_file = fopen(tmp_path.c_str(), "r");
    if(tmp_file == NULL) {
      return logger_->error("SnapshotController::restore_snapshot: open tmp file failed, " + tmp_path);
    }

    // read entry count
    size_t entry_count;
    fread(&entry_count, sizeof(size_t), 1, tmp_file);
    logger_->debug("SnapshotController::restore_snapshot: restore entry count " + std::to_string(entry_count));

    // restore chunk table
    cloudfs_controller_->get_chunk_table().Restore(tmp_file);
    logger_->debug("SnapshotController::restore_snapshot: restore chunk table done");

    char buf[PATH_MAX + 1];
    size_t processed= 0;
    while(processed < entry_count) {

        // read stat struct
        struct stat st;
        fread(&st, sizeof(struct stat), 1, tmp_file);

        if(S_ISDIR(st.st_mode)) {
            // read dir path
            size_t dir_path_len;
            fread(&dir_path_len, sizeof(size_t), 1, tmp_file);
            fread(buf, sizeof(char), dir_path_len, tmp_file);
            std::string dir_path(buf, dir_path_len);

            logger_->debug("SnapshotController::restore_snapshot: process dir " + dir_path); 

            // create dir if not exist
            if(mkdir(dir_path.c_str(), st.st_mode) != 0) {
              if(errno != EEXIST) {
                return logger_->error("SnapshotController::restore_snapshot: mkdir failed, " + dir_path);
              }
            }

            processed++;
            continue;
        }

        // read filepath
        size_t filepath_len;
        fread(&filepath_len, sizeof(size_t), 1, tmp_file);
        fread(buf, sizeof(char), filepath_len, tmp_file);
        std::string filepath(buf, filepath_len);
        logger_->debug("SnapshotController::restore_snapshot: process file " + filepath);

        // create file if not exist, first full priv
        auto file_fd = open(filepath.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0777);
        if(file_fd == -1) {
          if(errno != EEXIST) {
            return logger_->error("SnapshotController::restore_snapshot: create file failed, " + filepath);
          }
        }
        close(file_fd);

        // read chunk info
        size_t num_chunks;
        fread(&num_chunks, sizeof(size_t), 1, tmp_file);
        std::vector<Chunk> chunks;
        chunks.reserve(num_chunks);
        for(size_t i = 0; i < num_chunks; i++) {
            off_t start;
            fread(&start, sizeof(off_t), 1, tmp_file);
            size_t len;
            fread(&len, sizeof(size_t), 1, tmp_file);
            size_t key_len;
            fread(&key_len, sizeof(size_t), 1, tmp_file);
            std::vector<char> key(key_len);
            fread(key.data(), sizeof(char), key_len, tmp_file);
            std::string key_str(key.begin(), key.end());
            chunks.emplace_back(start, len, key_str);
        }
        // write chunks to file
        auto ret = cloudfs_controller_->set_chunkinfo(filepath, chunks);
        if(ret != 0) {
          return logger_->error("SnapshotController::restore_snapshot: set chunk info failed, " + filepath);
        }
        
        // read buffer path
        size_t buffer_path_len;
        fread(&buffer_path_len, sizeof(size_t), 1, tmp_file);
        fread(buf, sizeof(char), buffer_path_len, tmp_file);
        std::string buffer_path(buf, buffer_path_len);
        // set buffer path
        ret = cloudfs_controller_->set_buffer_path(filepath, buffer_path);
        if(ret != 0) {
          return logger_->error("SnapshotController::restore_snapshot: set buffer path failed, " + filepath);
        }

        // create buffer file if not exist
        auto buffer_fd = open(buffer_path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0777);
        if(buffer_fd == -1) {
          if(errno != EEXIST) {
            return logger_->error("SnapshotController::restore_snapshot: open buffer file failed, " + buffer_path);
          }
        }
        close(buffer_fd);

        // read file size
        size_t file_size;
        fread(&file_size, sizeof(size_t), 1, tmp_file);
        // set file size
        ret = cloudfs_controller_->set_size(buffer_path, file_size);
        if(ret != 0) {
          return logger_->error("SnapshotController::restore_snapshot: set file size failed, " + buffer_path);
        }

        if((int)file_size <= state_->threshold) {
            // small file, copy content to buffer file
            FILE* file = fopen(buffer_path.c_str(), "w");
            if(file == NULL) {
              return logger_->error("SnapshotController::restore_snapshot: open buffer file failed, " + buffer_path);
            }
            size_t read_size = 0;
            while(read_size < file_size) {
              size_t to_read = std::min(file_size - read_size, sizeof(buf));
              auto n = fread(buf, sizeof(char), to_read, tmp_file);
              if(n != to_read) {
                return logger_->error("SnapshotController::restore_snapshot: read tmp file failed, " + tmp_path);
              }
              fwrite(buf, sizeof(char), n, file);
              read_size += n;
            }
            fclose(file);
        }

        // utimensat file
        struct timespec tv[2];
        tv[0] = st.st_atim;
        tv[1] = st.st_mtim;
        if(utimensat(AT_FDCWD, filepath.c_str(), tv, AT_SYMLINK_NOFOLLOW) != 0) {
          return logger_->error("SnapshotController::restore_snapshot: utimensat file failed, " + filepath);
        }

        // chmod file
        if(chmod(filepath.c_str(), st.st_mode) != 0) {
          return logger_->error("SnapshotController::restore_snapshot: chmod file failed, " + filepath);
        }

        processed++;
    }
    fclose(tmp_file);
    logger_->debug("SnapshotController::restore_snapshot: restore file done");

    // delete tmp file
    remove(tmp_path.c_str());

    // delete snapshot that's newer than the restored snapshot
    std::sort(snapshot_list.rbegin(), snapshot_list.rend()); // from new to old
    for(size_t i = 0; i < snapshot_list.size(); i++) {
      if(snapshot_list[i] == *timestamp) {
        break;
      }
      uninstall_snapshot(&snapshot_list[i]); // uninstall if installed
      delete_snapshot(&snapshot_list[i]); // delete after uninstall
    }

    return 0;
}

int SnapshotController::list_snapshots(unsigned long *current_ts) { return 0; }

int SnapshotController::delete_snapshot(unsigned long *timestamp) { return 0; }

int SnapshotController::install_snapshot(unsigned long *timestamp) { return 0; }

int SnapshotController::uninstall_snapshot(unsigned long *timestamp) {
  return 0;
}

int SnapshotController::get_snapshot_count(int &count) {
  char buf[sizeof(int)];
  auto ret = lgetxattr(snapshot_stub_path_.c_str(), "user.cloudfs.snapshot_count",
                       buf, sizeof(buf));
  if (ret == -1) {
    if(errno == ENODATA) {
      logger_->debug("SnapshotController::get_snapshot_count: snapshot count not found, default to 0");
      count = 0;
      return 0;
    }
    return logger_->error(
        "SnapshotController::get_snapshot_count: get snapshot count failed");
  }
  count = *(int *)buf;
  return 0;
}

int SnapshotController::set_snapshot_count(int count) {
  return lsetxattr(snapshot_stub_path_.c_str(), "user.cloudfs.snapshot_count", &count, sizeof(count), 0);
}

int SnapshotController::get_snapshot_list(std::vector<unsigned long> &list) {
  auto xattr_name_header = "user.cloudfs.snapshot_";
  int count;
  if (get_snapshot_count(count) != 0) {
    return logger_->error(
        "SnapshotController::get_snapshot_list: get snapshot count failed");
  }
  for (int i = 0; i < count; i++) {
    char buf[sizeof(unsigned long)];
    auto xattr_name = xattr_name_header + std::to_string(i);
    auto ret =
        lgetxattr(snapshot_stub_path_.c_str(), xattr_name.c_str(), buf, sizeof(buf));
    if (ret == -1) {
      return logger_->error(
          "SnapshotController::get_snapshot_list: get snapshot list failed");
    }
    list.push_back(*(unsigned long *)buf);
  }
  return 0;
}

int SnapshotController::set_snapshot_list(std::vector<unsigned long>& list) {
  for(size_t i = 0; i < list.size(); i++) {
    auto xattr_name = "user.cloudfs.snapshot_" + std::to_string(i);
    lsetxattr(snapshot_stub_path_.c_str(), xattr_name.c_str(), &list[i], sizeof(unsigned long), 0);
  }
  return 0;
}

int SnapshotController::get_installed_snapshot_count(int &count) {
  char buf[sizeof(int)];
  auto ret =
      lgetxattr(snapshot_stub_path_.c_str(), "user.cloudfs.snapshot_installed_count",
                buf, sizeof(buf));
  if (ret == -1) {
    if(errno == ENODATA) {
      logger_->debug("SnapshotController::get_installed_snapshot_count: installed snapshot count not found, default to 0");
      count = 0;
      return 0;
    }
    return logger_->error("SnapshotController::get_installed_snapshot_count: "
                          "get installed snapshot count failed");
  }
  count = *(int *)buf;
  return 0;
}

int SnapshotController::set_installed_snapshot_count(int count) {
  return lsetxattr(snapshot_stub_path_.c_str(), "user.cloudfs.snapshot_installed_count", &count, sizeof(count), 0);
}

int SnapshotController::get_installed_snapshot_list(
    std::vector<unsigned long> &list) {
  auto xattr_name_header = "user.cloudfs.snapshot_installed_";
  int count;
  if (get_installed_snapshot_count(count) != 0) {
    return logger_->error("SnapshotController::get_installed_snapshot_list: "
                          "get installed snapshot count failed");
  }
  for (int i = 0; i < count; i++) {
    char buf[sizeof(unsigned long)];
    auto xattr_name = xattr_name_header + std::to_string(i);
    auto ret =
        lgetxattr(snapshot_stub_path_.c_str(), xattr_name.c_str(), buf, sizeof(buf));
    if (ret == -1) {
      return logger_->error("SnapshotController::get_installed_snapshot_list: "
                            "get installed snapshot list failed");
    }
    list.push_back(*(unsigned long *)buf);
  }
  return 0;
}

int SnapshotController::set_installed_snapshot_list(std::vector<unsigned long>& list) {
  for(size_t i = 0; i < list.size(); i++) {
    auto xattr_name = "user.cloudfs.snapshot_installed_" + std::to_string(i);
    lsetxattr(snapshot_stub_path_.c_str(), xattr_name.c_str(), &list[i], sizeof(unsigned long), 0);
  }
  return 0;
}



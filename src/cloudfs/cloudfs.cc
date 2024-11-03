#include <string>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>

#include "cloudapi.h"
#include "dedup.h"
#include "cloudfs.h"
#include "util.h"
#include "metadata.h"
#include "data.h"

#define UNUSED __attribute__((unused))

static struct cloudfs_state state_;

static const char* bukcet_name = "cloudfs";

static const std::string log_path = "/tmp/cloudfs.log";

static FILE *log_file;

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
  data_service_init(state_.hostname, bukcet_name);
  return NULL;
}

void cloudfs_destroy(void *data UNUSED) {
  data_service_destroy();
}

static int cloudfs_error(const std::string& error_str)
{
    auto retval = -errno;

    debug_print("[CloudFS Error] " +  error_str + ", errno = " + std::to_string(errno), log_file);

    /* FUSE always returns -errno to caller (yes, it is negative errno!) */
    return retval;
}

static void cloudfs_info(const std::string& info_str) {
  debug_print("[CloudFS Info] " + info_str, log_file);
}

int cloudfs_getattr(const char *path, struct stat *statbuf)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("getattr: " + ssd_path);

  struct stat st;
  auto ret = lstat(ssd_path.c_str(), &st); // use lstat to not follow the symbolic link
  if(ret != 0) {
    return cloudfs_error("getattr: ssd failed");
  }

  if(S_ISREG(st.st_mode)) {
    // regular file
    auto data_path = get_data_path(ssd_path);
    bool on_cloud;
    ret = is_on_cloud(ssd_path, on_cloud);
    if(ret != 0) {
      return cloudfs_error("getattr: is_on_cloud failed");
    }
    if(on_cloud) {
      auto data_path = get_data_path(ssd_path);
      // the file is on cloud, get the real size
      ret = get_size(ssd_path, st.st_size);
      if(ret != 0) {
        return cloudfs_error("getattr: get_size failed");
      }
    }
  }

  memcpy(statbuf, &st, sizeof(struct stat));

  cloudfs_info("getattr: size = " + std::to_string(statbuf->st_size));
  cloudfs_info("getattr: mode = " + std::to_string(statbuf->st_mode));
  // cloudfs_info("getattr: atime = " + std::to_string(statbuf->st_atime));
  // cloudfs_info("getattr: mtime = " + std::to_string(statbuf->st_mtime));

  return 0;
}

int cloudfs_getxattr(const char* path, const char* attr_name, char* buf, size_t size) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("getxattr: " + ssd_path + " attr_name = " + std::string(attr_name));
  
  auto ret = lgetxattr(ssd_path.c_str(), attr_name, buf, size);
  if(ret < 0) {
    return cloudfs_error("getxattr: failed");
  }

  cloudfs_info("getxattr: success");
  return ret;
}

int cloudfs_setxattr(const char* path, const char* attr_name, const char* attr_value, size_t size, int flags) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("setxattr: " + ssd_path + " attr_name = " + std::string(attr_name) + " attr_value = " + std::string(attr_value));
  
  auto ret = lsetxattr(ssd_path.c_str(), attr_name, attr_value, size, flags);
  if(ret < 0) {
    return cloudfs_error("setxattr: failed");
  }

  cloudfs_info("setxattr: success");
  return 0;
}

int cloudfs_mkdir(const char *path, mode_t mode)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("mkdir: " + ssd_path + " mode = " + std::to_string(mode));

  auto ret = mkdir(ssd_path.c_str(), mode);
  if(ret != 0) {
    return cloudfs_error("mkdir: ssd failed");
  }

  cloudfs_info("mkdir: success");
  return 0;
}

int cloudfs_mknod(const char *path, mode_t mode, dev_t dev)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("mknod: " + ssd_path + " mode = " + std::to_string(mode) + " dev = " + std::to_string(dev));

  auto ret = mknod(ssd_path.c_str(), mode, dev);
  if(ret != 0) {
    return cloudfs_error("mknod: ssd failed");
  }

  cloudfs_info("mknod: success");
  return 0;
}

int cloudfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("create: " + ssd_path + " mode = " + std::to_string(mode));

  auto fd = open(ssd_path.c_str(), fi->flags | O_CREAT, mode);
  if(fd < 0) {
    return cloudfs_error("create: ssd failed");
  }
  fi->fh = fd;


  auto data_path = get_data_path(ssd_path);
  // cloudfs_info("create: data_path = " + data_path);
  fd = open(data_path.c_str(), O_WRONLY | O_CREAT, 0777);
  if(fd < 0) {
    return cloudfs_error("create: create data file failed");
  }
  close(fd);
  
  auto ret = set_size(ssd_path, 0);
  if(ret != 0) {
    return cloudfs_error("create: set_size failed");
  }
  ret = set_on_cloud(ssd_path, false);
  if(ret != 0) {
    return cloudfs_error("create: set_on_cloud failed");
  }
  ret = set_dirty(ssd_path, false);
  if(ret != 0) {
    return cloudfs_error("create: set_dirty failed");
  }


  cloudfs_info("create: success");
  return 0;
}

int cloudfs_open(const char *path, struct fuse_file_info *fi)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("open: " + ssd_path + " flags = " + std::to_string(fi->flags));
  auto fd = open(ssd_path.c_str(), fi->flags);
  if(fd < 0) {
    return cloudfs_error("open: ssd failed");
  }
  fi->fh = fd;

  auto data_path = get_data_path(ssd_path);
  bool on_cloud;
  auto ret = is_on_cloud(ssd_path, on_cloud);
  if(ret != 0) {
    return cloudfs_error("open: is_on_cloud failed");
  }
  if(on_cloud) {
    cloudfs_info("open: download from cloud");
    download_data(data_path, bukcet_name, generate_object_key(ssd_path));
    close(fd); // if the file is on cloud, access actually goes to the data file
    fd = open(data_path.c_str(), fi->flags);
    if(fd < 0) {
      return cloudfs_error("open: download failed");
    }
    fi->fh = fd;
    cloudfs_info("open: download success");
  }
  // ret = set_dirty(ssd_path, false);
  // if(ret != 0) {
  //   return cloudfs_error("open: set_dirty failed");
  // }
  
  cloudfs_info("open: success");
  return 0;
}

int cloudfs_read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
  // cloudfs_info("read: " + std::string(path) + " count = " + std::to_string(count) + " offset = " + std::to_string(offset));
  auto fd = fi->fh;
  auto ret = pread(fd, buf, count, offset);
  if(ret < 0) {
    return cloudfs_error("read: ssd failed");
  }

  // cloudfs_info("read: success");
  return ret;
}

int cloud_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  // cloudfs_info("write: " + std::string(path) + " size = " + std::to_string(size) + " offset = " + std::to_string(offset));
  auto fd = fi->fh;
  auto written = pwrite(fd, buf, size, offset);
  if(written < 0) {
    return cloudfs_error("write: ssd failed");
  }

  auto ret = set_dirty(state_.ssd_path + std::string(path), true);
  if(ret != 0) {
    return cloudfs_error("write: set_dirty failed");
  }

  // cloudfs_info("write: success");
  return written;
}

int cloud_release(const char *path, struct fuse_file_info *fi) {
  cloudfs_info("release: " + std::string(path));

  auto fd = fi->fh;
  auto ret = close(fd);
  if(ret < 0) {
    // return cloudfs_error("release: ssd failed");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto data_path = get_data_path(ssd_path);
  bool on_cloud;
  ret = is_on_cloud(ssd_path, on_cloud);
  if(ret != 0) {
    return cloudfs_error("release: is_on_cloud failed");
  }

  auto upload_path = ssd_path;
  if(on_cloud) {
    upload_path = data_path;
  }

  struct stat stat_buf;
  lstat(upload_path.c_str(), &stat_buf);
  auto size = stat_buf.st_size;
  cloudfs_info("release: size = " + std::to_string(size));

  bool dirty;
  ret = is_dirty(ssd_path, dirty);
  if(ret != 0) {
    return cloudfs_error("release: is_dirty failed");
  }

  if(dirty) {
    cloudfs_info("release: is dirty");
    if(size > (off_t)state_.threshold) {
      // upload the file to cloud
      cloudfs_info("release: upload to cloud");
      upload_data(upload_path, bukcet_name, generate_object_key(ssd_path), size);
      clear_local(ssd_path);
      ret = set_on_cloud(ssd_path, true);
      if(ret != 0) {
        return cloudfs_error("release: set_on_cloud failed");
      }
      ret = set_size(ssd_path, size);
      if(ret != 0) {
        return cloudfs_error("release: set_size failed");
      }
      cloudfs_info("release: upload success");
    } else {
      // keep at ssd
      cloudfs_info("release: keep at ssd");
      bool on_cloud;
      ret = is_on_cloud(ssd_path, on_cloud);
      if(ret != 0) {
        return cloudfs_error("release: is_on_cloud failed");
      }
      if(on_cloud) {
        // delete the object on cloud
        cloudfs_info("release: delete from cloud");
        delete_object(bukcet_name, generate_object_key(ssd_path));
        ret = persist_data(upload_path, ssd_path);
        if(ret != 0) {
          return cloudfs_error("release: persist_data failed");
        }
      }
      ret = set_on_cloud(ssd_path, false);
      if(ret != 0) {
        return cloudfs_error("release: set_on_cloud failed");
      }
      ret = set_size(ssd_path, size);
      if(ret != 0) {
        return cloudfs_error("release: set_size failed");
      }
      cloudfs_info("release: keep at ssd success");
    }
    ret = set_dirty(ssd_path, false);
    if(ret != 0) {
      return cloudfs_error("release: set_dirty failed");
    }
  }
  clear_local(data_path);
  
  // cloudfs_info("release: success");
  return 0;
}

int cloud_opendir(const char *path, struct fuse_file_info *fi) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("opendir: " + ssd_path);

  auto dir = opendir(ssd_path.c_str());
  if(dir == NULL) {
    return cloudfs_error("opendir: ssd failed");
  }
  fi->fh = (uintptr_t)dir;

  cloudfs_info("opendir: success");
  return 0;
}

int cloud_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
  cloudfs_info("readdir: " + std::string(path));

  auto dir = (DIR *)fi->fh;
  auto entry = readdir(dir);
  if(entry == NULL) {
    return cloudfs_error("readdir: ssd failed");
  }
  do {
    // filter out lost+found directory
    if(strcmp(entry->d_name, "lost+found") == 0) {
      continue;
    }
    auto name = std::string(entry->d_name);
    // cloudfs_info("readdir: name = " + name);
    if(is_data_path(name)) {
      continue;
    }
    if(filler(buf, entry->d_name, NULL, 0) != 0) {
      return cloudfs_error("readdir: filler failed");
    }
  } while((entry = readdir(dir)) != NULL);

  cloudfs_info("readdir: success");
  return 0;
}

int cloud_access(const char *path, int mask) {
  cloudfs_info("access: " + std::string(path) + " mask = " + std::to_string(mask));

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = access(ssd_path.c_str(), mask);
  if(ret < 0) {
    return cloudfs_error("access: ssd failed");
  }

  cloudfs_info("access: success");
  return 0;
}

int cloud_utimens(const char *path, const struct timespec tv[2]) {
  auto ssd_path = state_.ssd_path + std::string(path);
  // cloudfs_info("utimens: " + ssd_path + " atime = " + std::to_string(tv[0].tv_sec) + " mtime = " + std::to_string(tv[1].tv_sec));

  auto ret = set_timestamps(ssd_path, tv);
  if(ret != 0) {
    return cloudfs_error("utimens: ssd failed");
  }

  // cloudfs_info("utimens: success");
  return 0;
}

int cloud_chmod(const char *path, mode_t mode) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("chmod: " + ssd_path + " mode = " + std::to_string(mode));

  auto ret = chmod(ssd_path.c_str(), mode);
  if(ret < 0) {
    return cloudfs_error("chmod: ssd failed");
  }

  cloudfs_info("chmod: success");
  return 0;
}

int cloud_link(const char* path, const char* newpath) {
  auto ssd_path = state_.ssd_path + std::string(path);
  auto new_ssd_path = state_.ssd_path + std::string(newpath);
  cloudfs_info("link: " + ssd_path + " -> " + new_ssd_path);

  auto ret = link(ssd_path.c_str(), new_ssd_path.c_str());
  if(ret < 0) {
    return cloudfs_error("link: ssd failed");
  }

  cloudfs_info("link: success");
  return 0;
}

int cloud_symlink(const char* target, const char* linkpath) {
  auto ssd_linkpath = state_.ssd_path + std::string(linkpath);
  cloudfs_info("symlink: " + std::string(target) + " -> " + ssd_linkpath);

  auto ret = symlink(target, ssd_linkpath.c_str());
  if(ret < 0) {
    return cloudfs_error("symlink: ssd failed");
  }

  cloudfs_info("symlink: success");
  return 0;
}

int cloud_readlink(const char* path, char* buf, size_t size) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("readlink: " + ssd_path);

  auto ret = readlink(ssd_path.c_str(), buf, size);
  if(ret < 0) {
    return cloudfs_error("readlink: ssd failed");
  }
  if(ret >= (int)size) {
    errno = ENAMETOOLONG;
    return cloudfs_error("readlink: buffer too small");
  }
  buf[ret] = '\0';
  cloudfs_info("readlink: " + std::string(buf));
  return 0;
}

int cloud_unlink(const char* path) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("unlink: " + ssd_path);

  struct stat stat_buf;
  lstat(ssd_path.c_str(), &stat_buf);
  if(S_ISREG(stat_buf.st_mode) && stat_buf.st_nlink == 1) {
    // This is the last link to the file
    // delete the file on cloud if it is on cloud
    auto data_path = get_data_path(ssd_path);
    bool on_cloud;
    auto ret = is_on_cloud(ssd_path, on_cloud);
    if(ret != 0) {
      return cloudfs_error("unlink: is_on_cloud failed");
    }
    if(on_cloud) {
      delete_object(bukcet_name, generate_object_key(ssd_path));
      ret = unlink(get_data_path(ssd_path).c_str()); // delete the data file
      if(ret < 0) {
        return cloudfs_error("unlink: delete data file failed");
      }
    }
  }

  auto ret = unlink(ssd_path.c_str());
  if(ret < 0) {
    return cloudfs_error("unlink: ssd failed");
  }
  ret = delete_data_file(get_data_path(ssd_path));
  if(ret != 0) {
    return cloudfs_error("unlink: delete data file failed");
  }

  cloudfs_info("unlink: success");
  return 0;
}

int cloud_rmdir(const char* path) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("rmdir: " + ssd_path);

  auto ret = rmdir(ssd_path.c_str());
  if(ret < 0) {
    return cloudfs_error("rmdir: ssd failed");
  }

  cloudfs_info("rmdir: success");
  return 0;
}

int cloud_truncate(const char* path, off_t size) {
  auto ssd_path = state_.ssd_path + std::string(path);
  // cloudfs_info("truncate: " + ssd_path + " size = " + std::to_string(size));
  auto data_path = get_data_path(ssd_path);
  bool on_cloud;
  auto ret = is_on_cloud(ssd_path, on_cloud);
  if(ret != 0) {
    return cloudfs_error("truncate: is_on_cloud failed");
  }

  auto truncate_path = ssd_path;
  if(on_cloud) {
    truncate_path = get_data_path(ssd_path);
  }

  ret = truncate(truncate_path.c_str(), size);
  if(ret < 0) {
    return cloudfs_error("truncate: ssd failed");
  }

  ret = set_dirty(ssd_path, true);

  // cloudfs_info("truncate: success");
  return 0;
}

static struct fuse_operations cloudfs_operations;

int cloudfs_start(struct cloudfs_state *state,
                  const char* fuse_runtime_name) {
  // This is where you add the VFS functions for your implementation of CloudFS.
  // You are NOT required to implement most of these operations, see the writeup
  //
  // Different operations take different types of parameters, see
  // /usr/include/fuse/fuse.h for the most accurate information
  //
  // In addition to this, the documentation for the latest version of FUSE
  // can be found at the following URL:
  // --- https://libfuse.github.io/doxygen/structfuse__operations.html
  cloudfs_operations.init = cloudfs_init;
  cloudfs_operations.destroy = cloudfs_destroy;
  cloudfs_operations.flag_utime_omit_ok = 1;
  cloudfs_operations.getattr = cloudfs_getattr;
  cloudfs_operations.getxattr = cloudfs_getxattr;
  cloudfs_operations.setxattr = cloudfs_setxattr;
  cloudfs_operations.mkdir = cloudfs_mkdir;
  cloudfs_operations.mknod = cloudfs_mknod;
  cloudfs_operations.create = cloudfs_create;
  cloudfs_operations.open = cloudfs_open;
  cloudfs_operations.read = cloudfs_read;
  cloudfs_operations.write = cloud_write;
  cloudfs_operations.release = cloud_release;
  cloudfs_operations.opendir = cloud_opendir;
  cloudfs_operations.readdir = cloud_readdir;
  cloudfs_operations.access = cloud_access;
  cloudfs_operations.utimens = cloud_utimens;
  cloudfs_operations.chmod = cloud_chmod;
  cloudfs_operations.link = cloud_link;
  cloudfs_operations.symlink = cloud_symlink;
  cloudfs_operations.readlink = cloud_readlink;
  cloudfs_operations.unlink = cloud_unlink;
  cloudfs_operations.rmdir = cloud_rmdir;
  cloudfs_operations.truncate = cloud_truncate;

  int argc = 0;
  char* argv[10];
  argv[argc] = (char *) malloc(strlen(fuse_runtime_name) + 1);
  strcpy(argv[argc++], fuse_runtime_name);
  argv[argc] = (char *) malloc(strlen(state->fuse_path) + 1);
  strcpy(argv[argc++], state->fuse_path);
  argv[argc] = (char *) malloc(strlen("-s") + 1);
  strcpy(argv[argc++], "-s"); // set the fuse mode to single thread
  // argv[argc] = (char *) malloc(sizeof("-f") * sizeof(char));
  // argv[argc++] = "-f"; // run fuse in foreground 

  state_  = *state;

  log_file = fopen("/tmp/cloudfs.log", "w");
  /* To ensure that a log of content is not buffered */
  setvbuf(log_file, NULL, _IOLBF, 0);
  cloudfs_info("Starting CloudFS");
  int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);
    
  return fuse_stat;
}

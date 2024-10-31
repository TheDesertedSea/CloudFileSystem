#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
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

static std::string bukcet_name = "cloudfs";

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

    debug_print("[CloudFS Error] " +  error_str, log_file);

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
  auto ret = stat(ssd_path.c_str(), &st);
  if(ret != 0) {
    return cloudfs_error("getattr: ssd failed");
  }

  if(S_ISREG(st.st_mode)) {
    Metadata metadata;
    ret = get_metadata(ssd_path, metadata);
    if(ret != 0) {
      return cloudfs_error("getattr: get_metadata failed");
    }
    if(metadata.is_on_cloud) {
      // the file is on cloud
      st.st_size = metadata.size;
    } else {
      // the file is on ssd
      st.st_size -= METADATA_SIZE;
    }
  }

  memcpy(statbuf, &st, sizeof(struct stat));
  return 0;
}

int cloudfs_getxattr(const char* path, const char* attr_name, char* buf, size_t size) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("getxattr: " + ssd_path);
  
  auto ret = getxattr(ssd_path.c_str(), attr_name, buf, size);
  if(ret < 0) {
    return cloudfs_error("getxattr: failed");
  }
  return ret;
}

int cloudfs_setxattr(const char* path, const char* attr_name, const char* attr_value, size_t size, int flags) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("setxattr: " + ssd_path);
  
  auto ret = setxattr(ssd_path.c_str(), attr_name, attr_value, size, flags);
  if(ret < 0) {
    return cloudfs_error("setxattr: failed");
  }
  return 0;
}

int cloudfs_mkdir(const char *path, mode_t mode)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("mkdir: " + ssd_path);

  auto ret = mkdir(ssd_path.c_str(), mode);
  if(ret != 0) {
    return cloudfs_error("mkdir: ssd failed");
  }

  return 0;
}

int cloudfs_mknod(const char *path, mode_t mode, dev_t dev)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("mknod: " + ssd_path);

  auto ret = mknod(ssd_path.c_str(), mode, dev);
  if(ret != 0) {
    return cloudfs_error("mknod: ssd failed");
  }

  return 0;
}

int cloudfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("create: " + ssd_path);

  auto fd = open(ssd_path.c_str(), fi->flags | O_CREAT, mode);
  if(fd < 0) {
    return cloudfs_error("create: ssd failed");
  }
  fi->fh = fd;
  Metadata metadata;
  metadata.size = 0;
  metadata.is_on_cloud = false;
  auto ret = set_metadata(ssd_path, metadata);
  if(ret != 0) {
    return cloudfs_error("create: set_metadata failed");
  }
  return 0;
}

int cloudfs_open(const char *path, struct fuse_file_info *fi)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("open: " + ssd_path);
  auto fd = open(ssd_path.c_str(), fi->flags);

  if(fd < 0) {
    return cloudfs_error("open: ssd failed");
  }
  fi->fh = fd;

  Metadata metadata;
  auto ret = get_metadata(ssd_path, metadata);
  if(ret != 0) {
    return cloudfs_error("open: get_metadata failed");
  }
  if(metadata.is_on_cloud) {
    cloudfs_info("open: download from cloud");
    download_data(ssd_path, bukcet_name, generate_object_key(ssd_path));
    struct stat st;
    lstat(ssd_path.c_str(), &st);
    st.st_size -= METADATA_SIZE;
    cloudfs_info("open: size = " + std::to_string(st.st_size));
    set_metadata(ssd_path, metadata);
    lstat(ssd_path.c_str(), &st);
    st.st_size -= METADATA_SIZE;
    cloudfs_info("open: size = " + std::to_string(st.st_size));
  }
  
  return 0;
}

int cloudfs_read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
  cloudfs_info("read: " + std::string(path) + " count = " + std::to_string(count) + " offset = " + std::to_string(offset));
  auto fd = fi->fh;
  auto ret = pread(fd, buf, count, offset + METADATA_SIZE);
  if(ret < 0) {
    return cloudfs_error("read: ssd failed");
  }
  return ret;
}

int cloud_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  cloudfs_info("write: " + std::string(path) + " size = " + std::to_string(size) + " offset = " + std::to_string(offset));
  auto fd = fi->fh;
  auto ret = pwrite(fd, buf, size, offset + METADATA_SIZE);
  if(ret < 0) {
    return cloudfs_error("write: ssd failed");
  }
  return ret;
}

int cloud_release(const char *path, struct fuse_file_info *fi) {
  cloudfs_info("release: " + std::string(path));

  auto fd = fi->fh;
  auto ret = close(fd);
  if(ret < 0) {
    return cloudfs_error("release: ssd failed");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  struct stat stat_buf;
  lstat(ssd_path.c_str(), &stat_buf);
  auto size = stat_buf.st_size - METADATA_SIZE;
  cloudfs_info("release: size = " + std::to_string(size));

  if(size > (size_t)state_.threshold) {
    // upload the file to cloud
    cloudfs_info("release: upload to cloud");
    upload_data(ssd_path, bukcet_name, generate_object_key(ssd_path), size);
    // clear the file
    auto ret = truncate(ssd_path.c_str(), METADATA_SIZE);
    if(ret < 0) {
      return cloudfs_error("release: truncate failed");
    }
    Metadata metadata;
    metadata.size = size;
    metadata.is_on_cloud = true;
    ret = set_metadata(ssd_path, metadata);
    if(ret != 0) {
      return cloudfs_error("release: set_metadata failed");
    }
  } else {
    // keep at ssd
    cloudfs_info("release: keep at ssd");
    Metadata metadata;
    auto ret = get_metadata(ssd_path, metadata);
    if(ret != 0) {
      return cloudfs_error("release: get_metadata failed");
    }
    if(metadata.is_on_cloud) {
      // delete the object on cloud
      delete_data(bukcet_name, generate_object_key(ssd_path));
    }
    metadata.size = size;
    metadata.is_on_cloud = false;
    ret = set_metadata(ssd_path, metadata);
    if(ret != 0) {
      return cloudfs_error("release: set_metadata failed");
    }
  }

  return 0;
}

int cloud_opendir(const char *path, struct fuse_file_info *fi) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("opendir: " + ssd_path);

  auto dir = opendir(ssd_path.c_str());
  if(dir == NULL) {
    return cloudfs_error("opendir: ssd failed");
  }
  fi->fh = (intptr_t)dir;
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
    if(filler(buf, entry->d_name, NULL, 0) != 0) {
      return cloudfs_error("readdir: filler failed");
    }
  } while((entry = readdir(dir)) != NULL);
  return 0;
}

int cloud_access(const char *path, int mask) {
  cloudfs_info("access: " + std::string(path));

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = access(ssd_path.c_str(), mask);
  if(ret < 0) {
    return cloudfs_error("access: ssd failed");
  }
  return 0;
}

int cloud_utimens(const char *path, const struct timespec tv[2]) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("utimens: " + ssd_path);

  auto ret = utimensat(AT_FDCWD, ssd_path.c_str(), tv, 0);
  if(ret < 0) {
    return cloudfs_error("utimens: ssd failed");
  }
  return 0;
}

int cloud_chmod(const char *path, mode_t mode) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("chmod: " + ssd_path);

  auto ret = chmod(ssd_path.c_str(), mode);
  if(ret < 0) {
    return cloudfs_error("chmod: ssd failed");
  }
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
  return 0;
}

int cloud_symlink(const char* path, const char* newpath) {
  auto ssd_path = state_.ssd_path + std::string(path);
  auto new_ssd_path = state_.ssd_path + std::string(newpath);
  cloudfs_info("symlink: " + ssd_path + " -> " + new_ssd_path);

  auto ret = symlink(ssd_path.c_str(), new_ssd_path.c_str());
  if(ret < 0) {
    return cloudfs_error("symlink: ssd failed");
  }
  return 0;
}

int cloud_readlink(const char* path, char* buf, size_t size) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("readlink: " + ssd_path);

  auto ret = readlink(ssd_path.c_str(), buf, size);
  if(ret < 0) {
    return cloudfs_error("readlink: ssd failed");
  }
  return 0;
}

int cloud_unlink(const char* path) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("unlink: " + ssd_path);

  struct stat stat_buf;
  lstat(ssd_path.c_str(), &stat_buf);
  if(stat_buf.st_nlink == 1) {
    // This is the last link to the file
    // delete the file on cloud if it is on cloud
    Metadata metadata;
    auto ret = get_metadata(ssd_path, metadata);
    if(ret != 0) {
      return cloudfs_error("unlink: get_metadata failed");
    }
    if(metadata.is_on_cloud) {
      delete_data(bukcet_name, generate_object_key(ssd_path));
    }
  }

  auto ret = unlink(ssd_path.c_str());
  if(ret < 0) {
    return cloudfs_error("unlink: ssd failed");
  }

  

  return 0;
}

int cloud_rmdir(const char* path) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("rmdir: " + ssd_path);

  auto ret = rmdir(ssd_path.c_str());
  if(ret < 0) {
    return cloudfs_error("rmdir: ssd failed");
  }
  return 0;
}

int cloud_truncate(const char* path, off_t size) {
  auto ssd_path = state_.ssd_path + std::string(path);
  cloudfs_info("truncate: " + ssd_path + " size = " + std::to_string(size));

  auto ret = truncate(ssd_path.c_str(), size + METADATA_SIZE); // do not truncate the metadata
  if(ret < 0) {
    return cloudfs_error("truncate: ssd failed");
  }
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

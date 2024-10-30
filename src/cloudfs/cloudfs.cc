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

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
  set_migrate_threshold(state_.threshold);
  cloud_init(state_.hostname);
  return NULL;
}

void cloudfs_destroy(void *data UNUSED) {
  cloud_destroy();
}

static int cloudfs_error(const std::string& error_str)
{
    auto retval = -errno;

    debug_print("[CloudFS Error] " +  error_str);

    /* FUSE always returns -errno to caller (yes, it is negative errno!) */
    return retval;
}

static void cloudfs_info(const std::string& info_str) {
  debug_print("[CloudFS Info] " + info_str);
}

int cloudfs_getattr(const char *path, struct stat *statbuf)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  auto proxy_path = get_proxy_path(ssd_path);
  if(!is_proxy_file(proxy_path)){
    auto ret = stat(ssd_path.c_str(), statbuf);
    if(ret != 0) {
      return cloudfs_error("getattr: ssd failed");
    }
    return 0;
  } 
  // File is on cloud, access the proxy file

  auto metadata = get_proxy_metadata(proxy_path);

  // Copy the metadata to the stat buffer
  memcpy(statbuf, &metadata.st, sizeof(struct stat));

  return 0;
}

int cloudfs_getxattr(const char* path, const char* attr_name, char* buf, size_t size) {
  auto ssd_path = state_.ssd_path + std::string(path);
  auto proxy_path = get_proxy_path(ssd_path);
  if(!is_proxy_file(proxy_path)){
    auto ret = getxattr(ssd_path.c_str(), attr_name, buf, size);
    if(ret == -1) {
      return cloudfs_error("getxattr: ssd failed");
    }
    return 0; 
  }
  // File is on cloud, access the proxy file

  auto metadata = get_proxy_metadata(proxy_path);

  for(auto& xattr : metadata.xattrs) {
    if(xattr.attr_name == attr_name) {
      if(xattr.attr_value.size() > size) {
        errno = ERANGE;
        return cloudfs_error("getxattr: Buffer too small");
      }
      memcpy(buf, xattr.attr_value.c_str(), xattr.attr_value.size());
      return xattr.attr_value.size();
    }
  }

  errno = ENODATA;
  return cloudfs_error("getxattr: Attribute not found");
}

int cloudfs_setxattr(const char* path, const char* attr_name, const char* attr_value, size_t size, int flags) {
  auto ssd_path = state_.ssd_path + std::string(path);
  auto proxy_path = get_proxy_path(ssd_path);
  if(!is_proxy_file(proxy_path)) {
    auto ret = setxattr(ssd_path.c_str(), attr_name, attr_value, size, flags);
    if(ret != 0) {
      return cloudfs_error("setxattr: ssd failed");
    }
    return 0;
  }
  // File is on cloud, access the proxy file

  auto metadata = get_proxy_metadata(proxy_path);

  for(auto& xattr : metadata.xattrs) {
    if(xattr.attr_name == attr_name) {
      if(flags == XATTR_CREATE) {
        errno = EEXIST;
        return cloudfs_error("setxattr: Attribute already exists");
      }

      xattr.attr_value = std::string(attr_value, size);
      set_proxy_metadata(proxy_path, metadata);
      return 0;
    }
  }

  if(flags == XATTR_REPLACE) {
    errno = ENODATA;
    return cloudfs_error("setxattr: Attribute not found");
  }

  ExtendedAttr new_xattr;
  new_xattr.attr_name = attr_name;
  new_xattr.attr_value = std::string(attr_value, size);
  metadata.xattrs.push_back(new_xattr);

  set_proxy_metadata(proxy_path, metadata);
  return 0;
}

int cloudfs_mkdir(const char *path, mode_t mode)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  // Check if there's a proxy file with the same name
  auto proxy_path = get_proxy_path(ssd_path);
  if(is_proxy_file(proxy_path)) {
    errno = EEXIST;
    return cloudfs_error("mkdir: already exists (proxy file)");
  }

  // Create the directory on the SSD
  
  auto ret = mkdir(ssd_path.c_str(), mode);
  if(ret != 0) {
    return cloudfs_error("mkdir: ssd failed");
  }

  return 0;
}

int cloudfs_mknod(const char *path, mode_t mode, dev_t dev)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  // Check if there's a proxy file with the same name
  auto proxy_path = get_proxy_path(ssd_path);
  if(is_proxy_file(proxy_path)) {
    errno = EEXIST;
    return cloudfs_error("mknod: already exists (proxy file)");
  }

  // Create the node on the SSD
  auto ret = mknod(ssd_path.c_str(), mode, dev);
  if(ret != 0) {
    return cloudfs_error("mknod: ssd failed");
  }

  return 0;
}

int cloudfs_open(const char *path, struct fuse_file_info *fi)
{
  auto ssd_path = state_.ssd_path + std::string(path);
  auto proxy_path = get_proxy_path(ssd_path);
  if(!is_proxy_file(proxy_path)) {
    auto ret = handler_open(ssd_path, fi->flags, &fi->fh, false, bukcet_name);
    if(ret != 0) {
      return cloudfs_error("open: SSD File failed");
    }
    return 0;
  }
  // File is on cloud, access the proxy file

  // access the file on the cloud
  auto ret = handler_open(proxy_path, fi->flags, &fi->fh, true, bukcet_name);
  if(ret != 0) {
    return cloudfs_error("open: Cloud File failed");
  }

  return 0;
}

int cloudfs_read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
  auto ret = handler_read(fi->fh, buf, count, offset);
  if(ret < 0) {
    return cloudfs_error("read: failed");
  }
  return 0;
}

static struct fuse_operations cloudfs_operations;

int cloudfs_start(struct cloudfs_state *state,
                  const char* fuse_runtime_name) {
  cloudfs_info("Starting CloudFS");
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
  cloudfs_operations.open = cloudfs_open;
  cloudfs_operations.read = cloudfs_read;

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

  int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);
    
  return fuse_stat;
}

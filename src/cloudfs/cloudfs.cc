/**
 * @file cloudfs.cc
 * @brief Fuse operations implementation for cloudfs
 *
 * CloudFS is a user-space file system for cloud storage services, stores small files locally and large files
 * in the cloud.
 * It uses rabin fingerprinting to split files into chunks and deduplicates chunks to save storage space.
 * It supports snapshots to save file system states and restore to previous states.
 *
 * Main functionalities:
 * 1. File operations: open, read, write, close, unlink, etc.
 * 2. Extended attributes: getxattr, setxattr, listxattr, removexattr
 * 3. Snapshots: snapshot, restore, delete, list, install, uninstall
 *
 * Modules:
 * 1. cloudfs: fuse operations implementation
 * 2. cloudfs_controller: controller for file operations, handling a subset of fuse operations and
 *    managing buffer files and cloud objects
 * 3. buffer_file: buffer file controller, handling buffer file operations.
 *    Buffer files are used to store small files locally or buffer cloud objects for large files
 *    It has a cache to store objects that are frequently accessed and uses a cache replacer to evict objects
 * 4. chunk_splitter: chunk splitter, splitting files into chunks using rabin fingerprinting and generating object keys
 *    using MD5 hash
 * 5. chunk_table: chunk table, managing chunk reference count
 * 6. snapshot: snapshot controller, handling snapshot operations
 * 7. cache_replacer: cache replacer, recording access history and choosing objects to evict
 * 8. util: utility functions, such as path checking, file tar/untar, debug logger, etc.
 *
 * @author Cundao Yu <cundaoy@andrew.cmu>
 */
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
#include <memory>

#include "cloudapi.h"
#include "cloudfs_controller.h"
#include "dedup.h"
#include "cloudfs.h"
#include "util.h"
#include "buffer_file.h"
#include "snapshot.h"
#include "snapshot-api.h"

#define UNUSED __attribute__((unused))

static struct cloudfs_state state_; // cloudfs state

static const char *bukcet_name = "cloudfs"; // bucket name

static const std::string log_path = "/tmp/cloudfs.log"; // log file path

static std::string snapshot_stub_path_; // absolute path to the ".snapshot" file

static std::shared_ptr<DebugLogger> logger_; // logger

static std::shared_ptr<CloudfsController> controller_;           // cloudfs controller
static std::unique_ptr<SnapshotController> snapshot_controller_; // snapshot controller

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 */
void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
  logger_ = std::make_shared<DebugLogger>(log_path);

  // create the cloudfs controller
  if (state_.no_dedup)
  {
    // no deduplication mode
    logger_->info("cloudfs_init: no dedup");
    controller_ = std::make_shared<CloudfsControllerNoDedup>(&state_, state_.hostname, bukcet_name, logger_);
  }
  else
  {
    // deduplication mode
    logger_->info("cloudfs_init: dedup");
    controller_ = std::make_shared<CloudfsControllerDedup>(&state_, state_.hostname, bukcet_name, logger_,
                                                           state_.rabin_window_size, state_.avg_seg_size, state_.min_seg_size, state_.max_seg_size);
  }

  // create the snapshot controller and initialize the snapshot stub file
  snapshot_controller_ = std::unique_ptr<SnapshotController>(new SnapshotController(&state_, logger_, controller_));
  snapshot_stub_path_ = std::string(state_.ssd_path) + "/.snapshot";
  return NULL;
}

/*
 * Destroys the FUSE file system (cloudfs) by unmounting the file system.
 *
 */
void cloudfs_destroy(void *data UNUSED)
{
  controller_->destroy();
  snapshot_controller_->persist();
}

/*
 * Get file attributes.
 * will be handled by the cloudfs controller
 */
int cloudfs_getattr(const char *path, struct stat *statbuf)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    auto ret = lstat(snapshot_stub_path_.c_str(), statbuf);
    if (ret < 0)
    {
      return logger_->error("getattr: failed");
    }
    return 0;
  }
  return controller_->stat_file(std::string(path), statbuf);
}

/*
 * Get extended attributes.
 */
int cloudfs_getxattr(const char *path, const char *attr_name, char *buf, size_t size)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    auto ret = lgetxattr(snapshot_stub_path_.c_str(), attr_name, buf, size);
    if (ret < 0)
    {
      return logger_->error("getxattr: failed");
    }
    return ret;
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = lgetxattr(ssd_path.c_str(), attr_name, buf, size);
  if (ret < 0)
  {
    return logger_->error("getxattr: failed");
  }
  return ret;
}

/*
 * Set extended attributes.
 */
int cloudfs_setxattr(const char *path, const char *attr_name, const char *attr_value, size_t size, int flags)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EACCES;
    return logger_->error("setxattr: .snapshot directory is read-only");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = lsetxattr(ssd_path.c_str(), attr_name, attr_value, size, flags);
  if (ret < 0)
  {
    return logger_->error("setxattr: failed");
  }
  return 0;
}

/*
 * Make a directory.
 */
int cloudfs_mkdir(const char *path, mode_t mode)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EEXIST;
    return logger_->error("mkdir: .snapshot directory cannot be created");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = mkdir(ssd_path.c_str(), mode);
  if (ret != 0)
  {
    return logger_->error("mkdir: failed");
  }
  return 0;
}

/*
 * Create a file node.
 */
int cloudfs_mknod(const char *path, mode_t mode, dev_t dev)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EEXIST;
    return logger_->error("mknod: .snapshot directory cannot be created");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = mknod(ssd_path.c_str(), mode, dev);
  if (ret != 0)
  {
    return logger_->error("mknod: failed");
  }
  return 0;
}

/*
 * Create a file.
 * will open the file after creation and return a file descriptor
 * will be handled by the cloudfs controller
 */
int cloudfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EEXIST;
    return logger_->error("create: .snapshot directory cannot be created");
  }

  // create the file
  auto ret = controller_->create_file(std::string(path), mode);
  if (ret != 0)
  {
    return logger_->error("create: failed");
  }

  // open the file
  return controller_->open_file(std::string(path), fi->flags, &fi->fh);
}

/*
 * Open a file.
 * will be handled by the cloudfs controller
 */
int cloudfs_open(const char *path, struct fuse_file_info *fi)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    // only allow read-only access to .snapshot directory
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
    {
      errno = EACCES;
      return logger_->error("open: .snapshot directory is read-only");
    }

    auto ret = open(snapshot_stub_path_.c_str(), fi->flags);
    if (ret < 0)
    {
      return logger_->error("open: failed");
    }
    fi->fh = ret;
    return 0;
  }

  return controller_->open_file(std::string(path), fi->flags, &fi->fh);
}

/*
 * Read data from an file or open file(need file descriptor) at the given offset.
 * will be handled by the cloudfs controller
 */
int cloudfs_read(const char *path, char *buf, size_t count, off_t offset, struct fuse_file_info *fi)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    auto ret = pread(fi->fh, buf, count, offset);
    if (ret < 0)
    {
      return logger_->error("read: failed");
    }
    return ret;
  }
  return controller_->read_file(std::string(path), fi->fh, buf, count, offset);
}

/*
 * Write data to an open file(need file descriptor) at the given offset.
 * will be handled by the cloudfs controller
 */
int cloud_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EACCES;
    return logger_->error("write: .snapshot directory is read-only");
  }
  return controller_->write_file(std::string(path), fi->fh, buf, size, offset);
}

/*
 * Release an open file.
 * will be handled by the cloudfs controller
 */
int cloud_release(const char *path, struct fuse_file_info *fi)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    auto ret = close(fi->fh);
    if (ret < 0)
    {
      return logger_->error("release: failed");
    }
    return 0;
  }
  return controller_->close_file(std::string(path), fi->fh);
}

/*
 * Open a directory.
 */
int cloud_opendir(const char *path, struct fuse_file_info *fi)
{

  auto ssd_path = state_.ssd_path + std::string(path);
  auto dir = opendir(ssd_path.c_str());
  if (dir == NULL)
  {
    return logger_->error("opendir: ssd failed");
  }
  fi->fh = (uintptr_t)dir;
  return 0;
}

/*
 * Read directory.
 */
int cloud_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
  auto dir = (DIR *)fi->fh;
  auto entry = readdir(dir);
  if (entry == NULL)
  {
    return logger_->error("readdir: ssd failed");
  }
  do
  {
    // filter out lost+found directory
    auto name = std::string(entry->d_name);
    if (name == "lost+found")
    {
      continue;
    }
    // filter out buffer files
    if (is_buffer_path(name))
    {
      continue;
    }
    // add the entry to buf
    if (filler(buf, entry->d_name, NULL, 0) != 0)
    {
      return logger_->error("readdir: filler failed");
    }
  } while ((entry = readdir(dir)) != NULL);

  if (strcmp(path, "/") == 0)
  {
    // add .snapshot to the directory
    if (filler(buf, ".snapshot", NULL, 0) != 0)
    {
      return logger_->error("readdir: filler failed");
    }
  }
  return 0;
}

/*
 * Access file.
 */
int cloud_access(const char *path, int mask)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    // only allow read-only access to .snapshot directory
    if ((mask & O_ACCMODE) != O_RDONLY)
    {
      errno = EACCES;
      return logger_->error("access: .snapshot directory is read-only");
    }
    auto ret = access(snapshot_stub_path_.c_str(), mask);
    if (ret < 0)
    {
      return logger_->error("access: failed");
    }
    return 0;
  }
  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = access(ssd_path.c_str(), mask);
  if (ret < 0)
  {
    return logger_->error("access: ssd failed");
  }
  return 0;
}

/*
 * Change the access and modification times of a file with nanosecond resolution.
 */
int cloud_utimens(const char *path, const struct timespec tv[2])
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EACCES;
    return logger_->error("utimens: .snapshot directory is read-only");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = utimensat(AT_FDCWD, ssd_path.c_str(), tv, AT_SYMLINK_NOFOLLOW);
  if (ret < 0)
  {
    return logger_->error("utimens: failed");
  }
  return 0;
}

/*
 * Change the mode of a file.
 */
int cloud_chmod(const char *path, mode_t mode)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    // cannot change the mode of .snapshot directory
    errno = EACCES;
    return logger_->error("chmod: .snapshot directory is read-only");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto ret = chmod(ssd_path.c_str(), mode);
  if (ret < 0)
  {
    return logger_->error("chmod: ssd failed");
  }
  return 0;
}

/*
 * Create a hard link to a file.
 */
int cloud_link(const char *path, const char *newpath)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EACCES;
    return logger_->error("link: .snapshot directory is read-only");
  }

  auto ssd_path = state_.ssd_path + std::string(path);
  auto new_ssd_path = state_.ssd_path + std::string(newpath);

  auto ret = link(ssd_path.c_str(), new_ssd_path.c_str());
  if (ret < 0)
  {
    return logger_->error("link: ssd failed");
  }
  return 0;
}

/*
 * Create a symbolic link.
 */
int cloud_symlink(const char *target, const char *linkpath)
{
  if (strcmp(linkpath, "/.snapshot") == 0)
  {
    errno = EACCES;
    return logger_->error("symlink: .snapshot directory is read-only");
  }

  auto ssd_linkpath = state_.ssd_path + std::string(linkpath);
  auto ret = symlink(target, ssd_linkpath.c_str());
  if (ret < 0)
  {
    return logger_->error("symlink: ssd failed");
  }
  return 0;
}

/*
 * Read the target of a symbolic link.
 */
int cloud_readlink(const char *path, char *buf, size_t size)
{
  auto ssd_path = state_.ssd_path + std::string(path);

  auto ret = readlink(ssd_path.c_str(), buf, size);
  if (ret < 0)
  {
    return logger_->error("readlink: ssd failed");
  }
  if (ret >= (int)size)
  {
    errno = ENAMETOOLONG;
    return logger_->error("readlink: ssd failed");
  }
  buf[ret] = '\0';
  return 0;
}

/*
 * Remove a hard link to a file.
 * will be handled by the cloudfs controller
 */
int cloud_unlink(const char *path)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EACCES;
    return logger_->error("unlink: .snapshot directory cannot be deleted");
  }
  return controller_->unlink_file(std::string(path));
}

/*
 * Remove a directory.
 */
int cloud_rmdir(const char *path)
{
  auto ssd_path = state_.ssd_path + std::string(path);

  auto ret = rmdir(ssd_path.c_str());
  if (ret < 0)
  {
    return logger_->error("rmdir: ssd failed");
  }
  return 0;
}

/*
 * Change the size of a file.
 */
int cloud_truncate(const char *path, off_t size)
{
  if (strcmp(path, "/.snapshot") == 0)
  {
    errno = EACCES;
    return logger_->error("truncate: .snapshot directory is read-only");
  }
  return controller_->truncate_file(std::string(path), size);
}

/*
 * Perform ioctl operations.
 * used for snapshot operations in cloudfs
 */
int cloud_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi, unsigned int flags, void *data)
{
  if (strcmp(path, "/.snapshot") != 0)
  {
    errno = EACCES;
    return logger_->error("ioctl: only .snapshot supports ioctl");
  }

  unsigned long *timestamp;
  unsigned long *snapshot_list;
  switch (cmd)
  {
  case CLOUDFS_SNAPSHOT:
    // create snapshot
    timestamp = (unsigned long *)data;
    return snapshot_controller_->create_snapshot(timestamp);
  case CLOUDFS_RESTORE:
    // restore snapshot
    timestamp = (unsigned long *)data;
    return snapshot_controller_->restore_snapshot(timestamp);
  case CLOUDFS_SNAPSHOT_LIST:
    // list snapshots
    snapshot_list = (unsigned long *)data;
    return snapshot_controller_->list_snapshots(snapshot_list);
  case CLOUDFS_DELETE:
    // delete snapshot
    timestamp = (unsigned long *)data;
    return snapshot_controller_->delete_snapshot(timestamp);
  case CLOUDFS_INSTALL_SNAPSHOT:
    // install snapshot
    timestamp = (unsigned long *)data;
    return snapshot_controller_->install_snapshot(timestamp);
  case CLOUDFS_UNINSTALL_SNAPSHOT:
    // uninstall snapshot
    timestamp = (unsigned long *)data;
    return snapshot_controller_->uninstall_snapshot(timestamp);
  default:
    errno = EINVAL;
    return logger_->error("ioctl: unknown command");
  }
  return 0;
}

static struct fuse_operations cloudfs_operations;

int cloudfs_start(struct cloudfs_state *state,
                  const char *fuse_runtime_name)
{
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
  cloudfs_operations.ioctl = cloud_ioctl;

  int argc = 0;
  char *argv[10];
  argv[argc] = (char *)malloc(strlen(fuse_runtime_name) + 1);
  strcpy(argv[argc++], fuse_runtime_name);
  argv[argc] = (char *)malloc(strlen(state->fuse_path) + 1);
  strcpy(argv[argc++], state->fuse_path);
  argv[argc] = (char *)malloc(strlen("-s") + 1);
  strcpy(argv[argc++], "-s"); // set the fuse mode to single thread
  // argv[argc] = (char *) malloc(sizeof("-f") * sizeof(char));
  // argv[argc++] = "-f"; // run fuse in foreground

  state_ = *state;

  int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);

  return fuse_stat;
}

#include "metadata.h"

#include <sys/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>

int get_size(const std::string& path, off_t& size) {
    char buf[SIZE_LEN];
    auto ret = lgetxattr(path.c_str(), SIZE_NAME, buf, SIZE_LEN);
    if(ret < 0) {
        
        return -1;
    }
    size = *(size_t*)buf;
    return 0;
}

int set_size(const std::string& path, off_t size) {
    auto ret = lsetxattr(path.c_str(), SIZE_NAME, &size, SIZE_LEN, 0);
    if(ret < 0) {
        return -1;
    }
    return 0;
}

int is_on_cloud(const std::string& path, bool& on_cloud) {
    char buf[ON_CLOUD_LEN];
    auto ret = lgetxattr(path.c_str(), ON_CLOUD_NAME, buf, ON_CLOUD_LEN);
    if(ret < 0) {
        return -1;
    }
    on_cloud = buf[0] == '1';
    return 0;
}

int set_on_cloud(const std::string& path, bool on_cloud) {
    char buf[ON_CLOUD_LEN];
    buf[0] = on_cloud ? '1' : '0';
    auto ret = lsetxattr(path.c_str(), ON_CLOUD_NAME, buf, ON_CLOUD_LEN, 0);
    if(ret < 0) {
        return -1;
    }
    return 0;
}

int get_timestamps(const std::string& path, timespec tv[]) {
    struct stat stat_buf;
    auto ret = lstat(path.c_str(), &stat_buf);
    if(ret < 0) {
        return -1;
    }
    tv[0] = stat_buf.st_atim;
    tv[1] = stat_buf.st_mtim;
    return 0;
}

int set_timestamps(const std::string& path, const timespec tv[]) {
    auto ret = utimensat(AT_FDCWD, path.c_str(), tv, AT_SYMLINK_NOFOLLOW);
    if(ret < 0) {
        return -1;
    }
    return 0;
}

int is_dirty(const std::string& path, bool& dirty) {
    char buf[DIRTY_LEN];
    auto ret = lgetxattr(path.c_str(), DIRTY_NAME, buf, DIRTY_LEN);
    if(ret < 0) {
        return -1;
    }
    dirty = buf[0] == '1';
    return 0;
}

int set_dirty(const std::string& path, bool dirty) {
    char buf[DIRTY_LEN];
    buf[0] = dirty ? '1' : '0';
    auto ret = lsetxattr(path.c_str(), DIRTY_NAME, buf, DIRTY_LEN, 0);
    if(ret < 0) {
        return -1;
    }
    return 0;
}

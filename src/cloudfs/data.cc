#include "data.h"
#include "cloudapi.h"
#include "metadata.h"
#include "util.h"

#include <cstdint>
#include <fuse.h>
#include <unistd.h>
#include <fcntl.h>
#include "cloudapi.h"

static std::vector<std::shared_ptr<OpenFile>> handler_table;

static int temp_fd_public = -1;

static int migrate_threshold = 0;

void set_migrate_threshold(int threshold) {
    migrate_threshold = threshold;
}

static int get_buffer(const char *buffer, int bufferLength) {
    lseek(temp_fd_public, 0, SEEK_SET);
    return write(temp_fd_public, buffer, bufferLength);
}

static int put_buffer(char *buffer, int bufferLength) {
    lseek(temp_fd_public, 0, SEEK_SET);
    return read(temp_fd_public, buffer, bufferLength);
}

bool OpenFile::IsOpened() {
    return is_opened_;
}

int OpenFile::GetErrorCode() {
    return error_code_;
}

bool OpenFile::IsOnCloud() {
    return is_on_cloud_;
}

std::string OpenFile::GetPath() {
    return path_;
}

int OpenFile::GetFlags() {
    return flags_;
}

OpenFile::~OpenFile() {}

SSDFile::SSDFile(std::string path, int flags) : OpenFile(path, flags, true){
    auto ret = open(path.c_str(), flags);
    if(ret == -1) {
        is_opened_ = false;
        error_code_ = errno;
        return;
    }

    fd_ = ret;
    is_opened_ = true;
}

SSDFile::~SSDFile() {
    close(fd_);
}

int SSDFile::Read(char *buf, size_t size, off_t offset) {
    lseek(fd_, offset, SEEK_SET);
    auto ret = read(fd_, buf, size);
    if(ret == -1) {
        error_code_ = errno;
    }
    return ret;
}

CloudFile::CloudFile(std::string path, int flags, std::string bucket_name)
        : OpenFile(path, flags, false), bucket_name_(bucket_name) {
    if(flags & O_RDONLY) {
        can_read_ = true;
        can_write_ = false;
    } else if(flags & O_WRONLY) {
        can_read_ = false;
        can_write_ = true;
    } else if(flags & O_RDWR) {
        can_read_ = true;
        can_write_ = true;
    }

    cloud_create_bucket(bucket_name.c_str());
    cloud_print_error();
    key_ = generate_object_key(path);

    temp_file_path_ = get_temp_file_path(path);
    temp_fd_ = open(temp_file_path_.c_str(), O_RDWR | O_CREAT, 0666);
    if(temp_fd_ == -1) {
        is_opened_ = false;
        error_code_ = errno;
        return;
    }

    is_opened_ = true;
    struct stat st;
    fstat(temp_fd_, &st);
    is_data_downloaded_ = st.st_size > 0;
    is_dirty_ = false;
}

void CloudFile::download_data() {
    if(is_data_downloaded_) {
        return;
    }

    temp_fd_public = temp_fd_;
    cloud_get_object(bucket_name_.c_str(), key_.c_str(), get_buffer);
    cloud_print_error();

    is_data_downloaded_ = true;
}

void CloudFile::upload_data() {
    if(!is_dirty_) {
        return;
    }

    struct stat st;
    fstat(temp_fd_, &st);
    temp_fd_public = temp_fd_;
    cloud_put_object(bucket_name_.c_str(), key_.c_str(), st.st_size, put_buffer);
    cloud_print_error();
    is_dirty_ = false;
}

int CloudFile::Read(char *buf, size_t size, off_t offset) {
    if(!can_read_) {
        error_code_ = EACCES;
        return -1;
    }

    download_data();
    lseek(temp_fd_, offset, SEEK_SET);
    auto ret = read(temp_fd_, buf, size);
    if(ret == -1) {
        error_code_ = errno;
    }

    return ret;
}

CloudFile::~CloudFile() {
    struct stat st;
    fstat(temp_fd_, &st);
    close(temp_fd_);
    if(st.st_size <= migrate_threshold * 1024) {
        // delete on cloud, save on ssd
        cloud_delete_object(bucket_name_.c_str(), key_.c_str());
        cloud_print_error();
        rename(temp_file_path_.c_str(), get_ssd_path(GetPath()).c_str());
    } else {
        // save on cloud, delete on ssd
        upload_data();
        remove(temp_file_path_.c_str());
    }
}

size_t get_next_handler_id() {
    for(size_t i = 0; i < handler_table.size(); i++) {
        if(handler_table[i] == nullptr) {
            return i;
        }
    }

    handler_table.push_back(nullptr);
    return handler_table.size() - 1;
}

int handler_open(std::string path, int flags, uint64_t* fh, bool is_on_cloud, std::string bucket_name) {
    std::shared_ptr<OpenFile> open_file = nullptr;
    if(is_on_cloud) {
        open_file = std::make_shared<CloudFile>(path, flags, bucket_name);
    } else {
        open_file = std::make_shared<SSDFile>(path, flags);
    }
    if(!open_file->IsOpened()) {
        errno = open_file->GetErrorCode();
        return -1;
    }

    auto handler_id = get_next_handler_id();
    handler_table[handler_id] = open_file;
    *fh = handler_id;
    return 0;
}

int handler_read(uint64_t handler_id, char *buf, size_t count, off_t offset) {
    if(handler_id >= handler_table.size() || handler_table[handler_id] == nullptr) {
        errno = EBADF;
        return -1;
    }

    auto open_file = handler_table[handler_id];
    int ret = open_file->Read(buf, count, offset);
    if(ret == -1) {
        errno = open_file->GetErrorCode();
        return -1;
    }

    return ret;
}
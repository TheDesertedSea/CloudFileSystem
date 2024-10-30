#pragma once

#include "metadata.h"
#include <string>
#include <exception>
#include <fcntl.h>
#include <sys/types.h>
#include <vector>
#include <memory>

class OpenFile {

    std::string path_;
    int flags_;
    bool is_on_cloud_;

protected:
    bool is_opened_;
    int error_code_;

public:

    OpenFile(std::string path, int flags, bool is_on_cloud) : path_(path), flags_(flags), is_on_cloud_(is_on_cloud), is_opened_(false) {}

    bool IsOpened();

    int GetErrorCode();

    bool IsOnCloud();

    std::string GetPath();

    int GetFlags();

    virtual int Read(char *buf, size_t count, off_t offset) = 0;

    virtual ~OpenFile();
};

class SSDFile : public OpenFile {
    int fd_;

public:
    SSDFile(std::string path, int flags);

    int Read(char *buf, size_t count, off_t offset) override;

    ~SSDFile() override;
};

class CloudFile : public OpenFile {
    
    std::string bucket_name_;
    std::string key_;
    bool can_write_;
    bool can_read_;

    std::string temp_file_path_;
    int temp_fd_;
    bool is_data_downloaded_;
    bool is_dirty_;

    void download_data();
    void upload_data();
    void update_metadata();
public:
    CloudFile(std::string path, int flags, std::string bucket_name);

    int Read(char *buf, size_t count, off_t offset) override;

    ~CloudFile() override;
};

size_t get_next_handler_id();

void set_migrate_threshold(int threshold);

int handler_open(std::string path, int flags, uint64_t* fh, bool is_on_cloud, std::string bucket_name);

int handler_read(uint64_t handler_id, char *buf, size_t count, off_t offset);
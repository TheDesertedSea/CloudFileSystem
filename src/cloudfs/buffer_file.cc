#include "buffer_file.h"

#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

#include "cloudapi.h"

BufferController::BufferController(const std::string& host_name, std::string bucket_name, std::shared_ptr<DebugLogger> logger)
    : bucket_name_(std::move(bucket_name)), logger_(std::move(logger)) {
    logger_->info("BufferController: " + bucket_name_);

    cloud_init(host_name.c_str());
    cloud_print_error(logger_->get_file());

    cloud_create_bucket(bucket_name_.c_str());
    cloud_print_error(logger_->get_file());

    bucket_list.clear();
    cloud_list_service(list_service);
    cloud_print_error(logger_->get_file());

    for(auto& bucket : bucket_list) {
        logger_->info("Bucket: " + bucket);
    }
}

FILE* BufferController::infile = NULL;
FILE* BufferController::outfile = NULL;
uint64_t BufferController::infd = 0;
uint64_t BufferController::outfd = 0;
off_t BufferController::in_offset = 0;
off_t BufferController::out_offset = 0;
std::vector<std::string> BufferController::bucket_list;
std::vector<std::pair<std::string, uint64_t>> BufferController::object_list;

BufferController::~BufferController() {
    cloud_destroy();
}

int BufferController::download_chunk(const std::string& key, uint64_t fd, off_t offset) {
    // logger_->info("download_chunk: " + key + " to " + buffer_path + ", offset: " + std::to_string(offset));

    outfd = fd;
    out_offset = offset;

    cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer_fd);
    cloud_print_error(logger_->get_file());

    return 0;
}

int BufferController::upload_chunk(const std::string& key, uint64_t fd, off_t offset, size_t size) {
    logger_->info("upload_chunk: " + key + ", offset: " + std::to_string(offset) + ", size: " + std::to_string(size));

    // FILE* test = fopen("/home/student/test.txt", "w+");
    // if(test == NULL) {
    //     logger_->error("upload_chunk: open test failed");
    // }
    // char buffer[4096];
    // auto ret = pread(fd, buffer, 4096, offset);
    // logger_->info("upload_chunk: try read from file first: " + std::to_string(ret));

    // infile = fopen("/home/student/buf", "w+");
    // char* buf = (char*)malloc(size);
    // auto ret = pread(fd, buf, size, offset);
    // logger_->info("upload_chunk: try read from file: " + std::to_string(ret));
    // fwrite(buf, 1, size, infile);
    // struct stat stat_buf;
    // lstat("/home/student/buf", &stat_buf);
    // logger_->info("upload_chunk: buf size: " + std::to_string(stat_buf.st_size));

    // fclose(infile);
    // free(buf);

    // upload_file(key, "/home/student/buf", size);

    infd = fd;
    in_offset = offset;

    cloud_put_object(bucket_name_.c_str(), key.c_str(), size, put_buffer_fd);
    cloud_print_error(logger_->get_file());

    // object_list.clear();
    // cloud_list_bucket(bucket_name_.c_str(), list_bucket_filler);
    // cloud_print_error(logger_->get_file());

    // for(auto& object : object_list) {
    //     logger_->info("Object: " + object.first + ", " + std::to_string(object.second));
    // }

    // logger_->info("upload_chunk: " + key + ", offset: " + std::to_string(offset) + ", size: " + std::to_string(size));
    
    // outfile = test;
    // cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer);
    // cloud_print_error(logger_->get_file());
    
    // ret = fread(buffer, 1, 4096, test);
    // logger_->info("upload_chunk: try read from cloud: " + std::to_string(ret));
    // fclose(test);

    return 0;
}

int BufferController::download_file(const std::string& key, const std::string& buffer_path) {
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

int BufferController::upload_file(const std::string& key, const std::string& buffer_path, size_t size) {
    // logger_->info("upload_file: " + buffer_path + " to " + key + ", size: " + std::to_string(size));

    infile = fopen(buffer_path.c_str(), "r");
    if (infile == NULL) {
        return -1;
    }

    cloud_put_object(bucket_name_.c_str(), key.c_str(), size, put_buffer);
    fclose(infile);
    cloud_print_error(logger_->get_file());

    return 0;
}

int BufferController::clear_file(const std::string& buffer_path) {
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

int BufferController::delete_object(const std::string& key) {
    // logger_->info("delete_object: " + key);

    cloud_delete_object(bucket_name_.c_str(), key.c_str());
    cloud_print_error(logger_->get_file());

    return 0;
}

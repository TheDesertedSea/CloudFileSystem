#include "buffer_file.h"

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
std::vector<std::string> BufferController::bucket_list;

BufferController::~BufferController() {
}

int BufferController::download_chunk(const std::string& key, const std::string& buffer_path, off_t offset) {
    // logger_->info("download_chunk: " + key + " to " + buffer_path + ", offset: " + std::to_string(offset));

    outfile = fopen(buffer_path.c_str(), "w");
    if (outfile == NULL) {
        return -1;
    }
    auto ret = fseek(outfile, offset, SEEK_SET);
    if (ret != 0) {
        return -1;
    }

    cloud_get_object(bucket_name_.c_str(), key.c_str(), get_buffer);
    fclose(outfile);
    cloud_print_error(logger_->get_file());

    return 0;
}

int BufferController::upload_chunk(const std::string& key, const std::string& buffer_path, off_t offset, size_t size) {
    // logger_->info("upload_chunk: " + buffer_path + " to " + key + ", offset: " + std::to_string(offset) + ", size: " + std::to_string(size));

    infile = fopen(buffer_path.c_str(), "r");
    if (infile == NULL) {
        return -1;
    }
    auto ret = fseek(infile, offset, SEEK_SET);
    if (ret != 0) {
        return -1;
    }

    cloud_put_object(bucket_name_.c_str(), key.c_str(), size, put_buffer);
    fclose(infile);
    cloud_print_error(logger_->get_file());

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

int BufferController::delete_object(const std::string& key) {
    // logger_->info("delete_object: " + key);

    cloud_delete_object(bucket_name_.c_str(), key.c_str());
    cloud_print_error(logger_->get_file());

    return 0;
}

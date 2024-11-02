#include "data.h"
#include "metadata.h"
#include "cloudapi.h"
#include "util.h"
#include <ctime>
#include <unistd.h>

static FILE *outfile;
int get_buffer(const char *buffer, int bufferLength) {
    return fwrite(buffer, 1, bufferLength, outfile);  
}

void download_data(const std::string& path, const std::string& bucket_name, const std::string& object_key) {
    // save the time
    timespec tv[2];
    auto ret = save_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to save time.");
        return;
    }

    outfile = fopen(path.c_str(), "r+");
    // skip the first METADATA_SIZE bytes
    fseek(outfile, METADATA_SIZE, SEEK_SET);
    cloud_get_object(bucket_name.c_str(), object_key.c_str(), get_buffer);
    fclose(outfile);
    cloud_print_error();

    // restore the time
    ret = restore_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to restore time.");
    }
}

static FILE *infile;
int put_buffer(char *buffer, int bufferLength) {
    return fread(buffer, 1, bufferLength, infile);
}

void data_service_init(const std::string& hostname, const std::string& bucket_name) {
    cloud_init(hostname.c_str());
    cloud_create_bucket(bucket_name.c_str());
    cloud_print_error();
}


void upload_data(const std::string& path, const std::string& bucket_name, const std::string& object_key, size_t size) {
    // save the time
    timespec tv[2];
    auto ret = save_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to save time.");
        return;
    }

    infile = fopen(path.c_str(), "r");
    if(infile == NULL)
    {
        debug_print("File not found.");
        return;
    }
    // skip the first METADATA_SIZE bytes
    fseek(infile, METADATA_SIZE, SEEK_SET);
    cloud_put_object(bucket_name.c_str(), object_key.c_str(), size, put_buffer);
    fclose(infile);
    cloud_print_error();

    // clear the file
    ret = truncate(path.c_str(), METADATA_SIZE);
    if(ret < 0) {
        debug_print("Failed to truncate the file.");
        return;
    }

    // restore the time
    ret = restore_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to restore time.");
    }
}

void delete_data(const std::string& bucket_name, const std::string& object_key) {
    cloud_delete_object(bucket_name.c_str(), object_key.c_str());
    cloud_print_error();
}

void data_service_destroy() {
    cloud_destroy();
}
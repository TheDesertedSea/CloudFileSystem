#include "data.h"
#include "metadata.h"
#include "cloudapi.h"
#include "util.h"

#include <time.h>
#include <stdio.h>
#include <unistd.h>

void data_service_init(const std::string& hostname, const std::string& bucket_name) {
    cloud_init(hostname.c_str());
    cloud_create_bucket(bucket_name.c_str());
    cloud_print_error();
}

void data_service_destroy() {
    cloud_destroy();
}

static FILE *outfile;
int get_buffer(const char *buffer, int bufferLength) {
    return fwrite(buffer, 1, bufferLength, outfile);  
}

int download_data(const std::string& path, const std::string& bucket_name, const std::string& object_key) {
    // save the time
    // timespec tv[2];
    // auto ret = get_timestamps(path.c_str(), tv);
    // if(ret != 0) {
    //     debug_print("download_data: Failed to get time.");
    //     return -1;
    // }

    // download the file
    outfile = fopen(path.c_str(), "w");
    if(outfile == NULL)
    {
        debug_print("download_data: File cannot be opened.");
        return -1;
    }
    cloud_get_object(bucket_name.c_str(), object_key.c_str(), get_buffer);
    fclose(outfile);
    cloud_print_error();

    // restore the time
    // ret = set_timestamps(path.c_str(), tv);
    // if(ret != 0) {
    //     debug_print("download_data: Failed to restore time.");
    //     return -1;
    // }

    return 0;
}

static FILE *infile;
int put_buffer(char *buffer, int bufferLength) {
    return fread(buffer, 1, bufferLength, infile);
}

int upload_data(const std::string& path, const std::string& bucket_name, const std::string& object_key, size_t size) {
    // save the time
    // timespec tv[2];
    // auto ret = get_timestamps(path.c_str(), tv);
    // if(ret != 0) {
    //     debug_print("upload_data: Failed to get time.");
    //     return -1;
    // }

    // upload the file
    infile = fopen(path.c_str(), "r");
    if(infile == NULL)
    {
        debug_print("upload_data: File cannot be opened.");
        return -1;
    }
    cloud_put_object(bucket_name.c_str(), object_key.c_str(), size, put_buffer);
    fclose(infile);
    cloud_print_error();

    // restore the time
    // ret = set_timestamps(path.c_str(), tv);
    // if(ret != 0) {
    //     debug_print("upload_data: Failed to restore time.");
    //     return -1;
    // }

    return 0;
}

int clear_local(const std::string& path) {
    return truncate(path.c_str(), 0);
}

int persist_data(const std::string& data_path, const std::string& path) {
    FILE* src = fopen(data_path.c_str(), "r");
    if(src == NULL) {
        debug_print("persist_data: Failed to open source file.");
        return -1;
    }
    FILE* dst = fopen(path.c_str(), "w");
    if(dst == NULL) {
        fclose(src);
        debug_print("persist_data: Failed to open destination file.");
        return -1;
    }
    char buffer[4096];
    size_t size;
    while((size = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, size, dst);
    }
    fclose(src);
    fclose(dst);
    return 0;
}

int delete_object(const std::string& bucket_name, const std::string& object_key) {
    cloud_delete_object(bucket_name.c_str(), object_key.c_str());
    cloud_print_error();
    return 0;
}

int delete_data_file(const std::string& path) {
    return unlink(path.c_str());
}


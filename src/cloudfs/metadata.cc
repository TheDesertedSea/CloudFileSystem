#include "metadata.h"

#include "util.h"
#include <cstdio>

int get_metadata(const std::string& path, Metadata& metadata) {
    // save the time
    timespec tv[2];
    auto ret = save_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to save time.");
        return -1;
    }


    FILE* file = fopen(path.c_str(), "r");
    if (file == NULL) {
        return -1;
    }
    // Read the first 8 bytes to get the size of the file
    fread(&metadata.size, sizeof(size_t), 1, file);
    // Read the next byte to get the is_on_cloud flag
    fread(&metadata.is_on_cloud, sizeof(bool), 1, file);
    fclose(file);

    // restore the time
    ret = restore_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to restore time.");
    }

    return 0;
}

int set_metadata(const std::string& path, const Metadata& metadata) {
    // save the time
    timespec tv[2];
    auto ret = save_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to save time.");
        return -1;
    }


    FILE* file = fopen(path.c_str(), "r+");
    if (file == NULL) {
        return -1;
    }
    // Write the size of the file
    fwrite(&metadata.size, sizeof(size_t), 1, file);
    // Write the is_on_cloud flag
    fwrite(&metadata.is_on_cloud, sizeof(bool), 1, file);
    fclose(file);

    // restore the time
    ret = restore_time(path, tv);
    if(ret != 0) {
        debug_print("Failed to restore time.");
    }
    return 0;
}


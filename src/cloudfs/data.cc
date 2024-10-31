#include "data.h"
#include "metadata.h"
#include "cloudapi.h"
#include "util.h"

static FILE *outfile;
int get_buffer(const char *buffer, int bufferLength) {
    return fwrite(buffer, 1, bufferLength, outfile);  
}

void download_data(const std::string& path, const std::string& bucket_name, const std::string& object_key) {
    outfile = fopen(path.c_str(), "wb");
    // skip the first METADATA_SIZE bytes
    fseek(outfile, METADATA_SIZE, SEEK_SET);
    cloud_get_object(bucket_name.c_str(), object_key.c_str(), get_buffer);
    fclose(outfile);
    cloud_print_error();
}

static FILE *infile;
int put_buffer(char *buffer, int bufferLength) {
    return fread(buffer, 1, bufferLength, infile);
}

void upload_data(const std::string& path, const std::string& bucket_name, const std::string& object_key, size_t size) {
    infile = fopen(path.c_str(), "rb");
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
}
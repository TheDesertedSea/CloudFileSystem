#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// Archive a single file, including extended attributes
void archive_file(const string& filepath, const string& root,
                  const string& output_path);

// archive_directory - archive an entire directory
void archive_directory(const char* outname, const string& dirname,
                       string root = "");

// extract - extract an archive to specified directory
void extract(const char* filename, const string& d);

#endif

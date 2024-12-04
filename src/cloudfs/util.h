/**
 * @file util.h
 * @brief Utility functions and classes
 * @author Cundao Yu <cundaoy@andrew.cmu.edu>
 */

#pragma once

#include <cstdio>
#include <stdio.h>
#include <string>
#include <sys/stat.h>


#define MEM_BUFFER_LEN 4096

/**
 * Print debug message to file
 * @param msg The message to print
 * @param file The file pointer to print to, default to stderr
 */
void debug_print(const std::string &msg, FILE *file = stderr);

/**
 * Generate object key from path
 * @param path The path to generate key from
 * @return The generated key
 */
std::string generate_object_key(const std::string &path);

/**
 * Convert main path to buffer path
 * @param main_path The main path to convert
 * @return The converted buffer path
 */
std::string main_path_to_buffer_path(const std::string &main_path);

/**
 * Check if the path is a buffer path(starts with '.')
 * @param path The path to check
 * @return True if the path is a buffer path, false otherwise
 */
bool is_buffer_path(const std::string &path);

/**
 * Tar a file to a tar file
 * @param tar_path The path to the tar file
 * @param file_path The path to the file to tar
 * @return 0 on success, -1 on failure
 */
int tar_file(const std::string &tar_path, const std::string &file_path);

/**
 * Untar a tar file to a directory
 * @param tar_path The path to the tar file
 * @param dir_path The path to the directory to untar to
 * @return 0 on success, -1 on failure
 */
int untar_file(const std::string &tar_path, const std::string &dir_path);

/**
 * Debug logger
 */
class DebugLogger {
  /*
   * File pointer to the log file
   */
  FILE *file_;

public:
  DebugLogger(const std::string &log_path);
  ~DebugLogger();

  /**
   * Print error message to the log file
   * @param error_str The error message to print
   * @return negative errno on failure, 0 on success
   */
  int error(const std::string &error_str);

  /**
   * Print info message to the log file
   * @param info_str The info message to print
   */
  void info(const std::string &info_str);

  /**
   * Print debug message to the log file
   * @param debug_str The debug message to print
   */
  void debug(const std::string &debug_str);

  /**
   * Get the file pointer to the log file
   * @return The file pointer to the log file
   */
  FILE *get_file();
};

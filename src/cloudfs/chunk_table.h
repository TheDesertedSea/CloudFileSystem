/**
 * @file chunk_table.h
 * @brief Chunk reference count table
 * @author Cundao Yu <cundaoy@andrew.cmu.edu>
 */

#pragma once

#include "util.h"
#include <memory>
#include <string>
#include <unordered_map>

#include "buffer_file.h"

/**
 * Chunk reference count table
 */
class ChunkTable {

  /**
   * Chunk reference count entry
   */
  struct RefCounts {
    int ref_count_;          // reference count of the chunk
    int snapshot_ref_count_; // indicates how many snapshots are using this
                             // chunk
    RefCounts() : ref_count_(0), snapshot_ref_count_(0) {}
    RefCounts(int ref_count, int snapshot_ref_count)
        : ref_count_(ref_count), snapshot_ref_count_(snapshot_ref_count) {}
  };

  std::string ssd_path_; // path of the SSD

  std::shared_ptr<DebugLogger> logger_;                 // logger
  std::shared_ptr<BufferFileController> buffer_controller_; // buffer controller

  static const std::string
      TABLE_FILE_NAME; // name of the chunk table persistence file

  std::unordered_map<std::string, RefCounts> chunk_table_; // chunk table

public:
  ChunkTable(const std::string &ssd_path, std::shared_ptr<DebugLogger> logger,
             std::shared_ptr<BufferFileController> buffer_controller);
  ~ChunkTable();

  /**
   * Use a chunk
   * @param key key of the chunk
   * @return true if this chunk is new, false if it already exists
   */
  bool use(const std::string &key);

  /**
   * Release a chunk
   * @param key key of the chunk
   * @return true if this chunk is no longer in use, false if it is still in use
   */
  bool release(const std::string &key);

  /**
   * Persist the chunk table to the disk
   */
  void persist();

  /**
   * Print the chunk table
   */
  void print();

  /**
   * Snapshot the chunk table into a snapshot file
   * @param snapshot_file file pointer to the snapshot file
   */
  void snapshot(FILE *snapshot_file);

  /**
   * Restore the chunk table from a snapshot file
   * @param snapshot_file file pointer to the snapshot file
   */
  void restore(FILE *snapshot_file);

  /**
   * A snapshot is deleted, decrease the snapshot ref count of the chunks
   * @param snapshot_file file pointer to the snapshot file
   */
  void snapshot_deleted(FILE *snapshot_file);

  /**
   * Not interested in the chunk table part of a snapshot, skip it
   * @param snapshot_file file pointer to the snapshot file
   */
  void skip_snapshot(FILE *snapshot_file);
};
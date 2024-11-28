#pragma once

#include "util.h"
#include <memory>
#include <string>
#include <unordered_map>

#include "buffer_file.h"

class ChunkTable {

  struct RefCounts {
    int ref_count_;
    int snapshot_ref_count_;
    RefCounts() : ref_count_(0), snapshot_ref_count_(0) {}
    RefCounts(int ref_count, int snapshot_ref_count) : ref_count_(ref_count), snapshot_ref_count_(snapshot_ref_count) {}
  };

  std::string ssd_path_;

  std::shared_ptr<DebugLogger> logger_;
  std::shared_ptr<BufferController> buffer_controller_;

  static const std::string TABLE_FILE_NAME;

  std::unordered_map<std::string, RefCounts> chunk_table_;

  public:
    ChunkTable(const std::string& ssd_path, std::shared_ptr<DebugLogger> logger, std::shared_ptr<BufferController> buffer_controller);
    ~ChunkTable();

    // Returns true if this chunk is new, false if it already exists
    bool Use(const std::string& key);

    // Returns true if this chunk is no longer in use, false if it is still in use
    bool Release(const std::string& key);

    void Persist();

    void Print();

    void Snapshot(FILE* snapshot_file);

    void Restore(FILE* snapshot_file);

    void DeleteSnapshot(FILE* snapshot_file);

    void SkipSnapshot(FILE* snapshot_file);
};
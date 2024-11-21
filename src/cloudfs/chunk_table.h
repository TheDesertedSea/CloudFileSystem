#pragma once

#include "util.h"
#include <memory>
#include <string>
#include <unordered_map>

class ChunkTable {


  std::string ssd_path_;

  std::shared_ptr<DebugLogger> logger_;


  static const std::string TABLE_FILE_NAME;

  std::unordered_map<std::string, int> chunk_table_;

  public:
    ChunkTable(const std::string& ssd_path, std::shared_ptr<DebugLogger> logger);
    ~ChunkTable();

    // Returns true if this chunk is new, false if it already exists
    bool Use(const std::string& key);

    // Returns true if this chunk is no longer in use, false if it is still in use
    bool Release(const std::string& key);

    void Persist();

    void Print();
};
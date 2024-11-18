#pragma once

#include <string>
#include <unordered_map>

class ChunkTable {

  std::unordered_map<std::string, int> chunk_table_;

  public:
    ChunkTable(const std::string& table_path);
    ~ChunkTable();

    // Returns true if this chunk is new, false if it already exists
    bool Use(const std::string& key);

    // Returns true if this chunk is no longer in use, false if it is still in use
    bool Release(const std::string& key);
};
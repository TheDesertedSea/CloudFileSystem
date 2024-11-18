#include "chunk_table.h"
#include <stdexcept>

ChunkTable::ChunkTable(const std::string& table_path) {
}

ChunkTable::~ChunkTable() {
}

bool ChunkTable::Use(const std::string& key) {
    chunk_table_[key]++;
    return chunk_table_[key] == 1;
}

bool ChunkTable::Release(const std::string& key) {
    if (chunk_table_.find(key) == chunk_table_.end()) {
        throw std::runtime_error("Chunk not found in table");
    }
    chunk_table_[key]--;
    return chunk_table_[key] == 0;
}


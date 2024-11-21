#include "chunk_table.h"
#include <stdexcept>
#include <vector>
#include <unistd.h>

const std::string ChunkTable::TABLE_FILE_NAME = ".chunk_table";

ChunkTable::ChunkTable(const std::string& ssd_path, std::shared_ptr<DebugLogger> logger): ssd_path_(ssd_path), logger_(std::move(logger)) {
    // logger_->info("ChunkTable: init " + ssd_path_);
    
    // load chunk table from file
    FILE* table_file = fopen((ssd_path_ + "/" + TABLE_FILE_NAME).c_str(), "r");
    if (table_file == NULL) {
        return;
    }
    size_t num_entries;
    fread(&num_entries, sizeof(size_t), 1, table_file);
    for (size_t i = 0; i < num_entries; i++) {
        size_t key_len;
        fread(&key_len, sizeof(size_t), 1, table_file);
        std::vector<char> key(key_len);
        fread(key.data(), sizeof(char), key_len, table_file);
        int count;
        fread(&count, sizeof(int), 1, table_file);
        chunk_table_[std::string(key.begin(), key.end())] = count;
    }

    // for(const auto& entry : chunk_table_) {
    //     logger_->info("ChunkTable: load key " + entry.first + ", count " + std::to_string(entry.second));
    // }
}

ChunkTable::~ChunkTable() {
}

bool ChunkTable::Use(const std::string& key) {
    chunk_table_[key]++;
    return chunk_table_[key] == 1;
}

bool ChunkTable::Release(const std::string& key) {
    if (chunk_table_.find(key) == chunk_table_.end()) {
        logger_->error("ChunkTable: chunk not found in table");
        throw std::runtime_error("Chunk not found in table");
    }
    chunk_table_[key]--;
    return chunk_table_[key] == 0;
}

void ChunkTable::Persist() {
    // logger_->info("ChunkTable: persist");

    // save chunk table to file
    FILE* table_file = fopen((ssd_path_ + "/" + TABLE_FILE_NAME).c_str(), "w+");
    ftruncate(fileno(table_file), 0);
    size_t num_entries = chunk_table_.size();
    fwrite(&num_entries, sizeof(size_t), 1, table_file);
    for (const auto& entry : chunk_table_) {
        // logger_->info("ChunkTable: persist key " + entry.first + ", count " + std::to_string(entry.second));
        size_t key_len = entry.first.size();
        fwrite(&key_len, sizeof(size_t), 1, table_file);
        fwrite(entry.first.c_str(), sizeof(char), key_len, table_file);
        fwrite(&entry.second, sizeof(int), 1, table_file);
    }
    fclose(table_file);
}


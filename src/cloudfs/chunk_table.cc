#include "chunk_table.h"
#include <cassert>
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
        int ref_count;
        fread(&ref_count, sizeof(int), 1, table_file);
        int snapshot_ref_count;
        fread(&snapshot_ref_count, sizeof(int), 1, table_file);
        chunk_table_[std::string(key.begin(), key.end())] = RefCounts(ref_count, snapshot_ref_count);
    }

    // for(const auto& entry : chunk_table_) {
    //     logger_->info("ChunkTable: load key " + entry.first + ", count " + std::to_string(entry.second));
    // }
}

ChunkTable::~ChunkTable() {
}

bool ChunkTable::Use(const std::string& key) {
    chunk_table_[key].ref_count_++;
    return chunk_table_[key].ref_count_ == 1 && chunk_table_[key].snapshot_ref_count_ == 0;
}

bool ChunkTable::Release(const std::string& key) {
    if (chunk_table_.find(key) == chunk_table_.end()) {
        logger_->error("ChunkTable: chunk not found in table");
        throw std::runtime_error("Chunk not found in table");
    }
    chunk_table_[key].ref_count_--;
    auto is_last = chunk_table_[key].ref_count_ == 0 && chunk_table_[key].snapshot_ref_count_ == 0;
    if (is_last) {
        chunk_table_.erase(key);
    }
    return is_last;
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
        fwrite(&entry.second.ref_count_, sizeof(int), 1, table_file);
        fwrite(&entry.second.snapshot_ref_count_, sizeof(int), 1, table_file);
    }
    fclose(table_file);
}

void ChunkTable::Print() {
    for (const auto& entry : chunk_table_) {
        // logger_->info("ChunkTable: key " + entry.first + ", count " + std::to_string(entry.second));
        logger_->debug("ChunkTable: key " + entry.first + ", ref_count " + std::to_string(entry.second.ref_count_) + ", snapshot_ref_count " + std::to_string(entry.second.snapshot_ref_count_));
    }
}

void ChunkTable::Snapshot(FILE* snapshot_file) {
    size_t num_entries = chunk_table_.size();
    fwrite(&num_entries, sizeof(size_t), 1, snapshot_file);
    for (auto& entry : chunk_table_) {
        // logger_->info("ChunkTable: persist key " + entry.first + ", count " + std::to_string(entry.second));
        size_t key_len = entry.first.size();
        fwrite(&key_len, sizeof(size_t), 1, snapshot_file);
        fwrite(entry.first.c_str(), sizeof(char), key_len, snapshot_file);
        fwrite(&entry.second.ref_count_, sizeof(int), 1, snapshot_file); // only ref_count is needed for snapshot

        // add snapshot ref count
        entry.second.snapshot_ref_count_++;
    }
}

void ChunkTable::Restore(FILE* snapshot_file) {
    size_t num_entries;
    fread(&num_entries, sizeof(size_t), 1, snapshot_file);
    for (size_t i = 0; i < num_entries; i++) {
        size_t key_len;
        fread(&key_len, sizeof(size_t), 1, snapshot_file);
        std::vector<char> key(key_len);
        fread(key.data(), sizeof(char), key_len, snapshot_file);
        std::string key_str(key.begin(), key.end());
        int ref_count;
        fread(&ref_count, sizeof(int), 1, snapshot_file);

        assert(chunk_table_.find(key_str) != chunk_table_.end()); // since snapshot_ref_count will be larger than 0, the key must exist
        chunk_table_[key_str].ref_count_ = ref_count;
    }
}

void ChunkTable::DeleteSnapshot(FILE* snapshot_file) {
    size_t num_entries;
    fread(&num_entries, sizeof(size_t), 1, snapshot_file);
    for (size_t i = 0; i < num_entries; i++) {
        size_t key_len;
        fread(&key_len, sizeof(size_t), 1, snapshot_file);
        std::vector<char> key(key_len);
        fread(key.data(), sizeof(char), key_len, snapshot_file);
        std::string key_str(key.begin(), key.end());

        assert(chunk_table_.find(key_str) != chunk_table_.end()); // since snapshot_ref_count will be larger than 0, the key must exist
        chunk_table_[key_str].snapshot_ref_count_--;
        if(chunk_table_[key_str].snapshot_ref_count_ == 0 && chunk_table_[key_str].ref_count_ == 0) {
            // remove the key if both ref_count and snapshot_ref_count are 0
            chunk_table_.erase(key_str);
        }
    }
}

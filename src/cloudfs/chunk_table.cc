#include "chunk_table.h"
#include <cassert>
#include <stdexcept>
#include <unistd.h>
#include <vector>


const std::string ChunkTable::TABLE_FILE_NAME = ".chunk_table";

ChunkTable::ChunkTable(const std::string &ssd_path,
                       std::shared_ptr<DebugLogger> logger,
                       std::shared_ptr<BufferController> buffer_controller)
    : ssd_path_(ssd_path), logger_(std::move(logger)),
      buffer_controller_(std::move(buffer_controller)) {
  // logger_->info("ChunkTable: init " + ssd_path_);

  // download chunk table from cloud
  auto table_path = ssd_path_ + "/" + TABLE_FILE_NAME;
  buffer_controller_->download_file(TABLE_FILE_NAME, table_path);
  struct stat st;
  stat(table_path.c_str(), &st);
  if(st.st_size == 0) {
    // logger_->debug("ChunkTable: no chunk table");
    remove(table_path.c_str());
    // no chunk table
    return;
  }

  // load chunk table from file
  FILE *table_file = fopen(table_path.c_str(), "r");
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
    chunk_table_[std::string(key.begin(), key.end())] =
        RefCounts(ref_count, snapshot_ref_count);
  }
  fclose(table_file);

  // remove local chunk table file
  remove(table_path.c_str());
  // for(const auto& entry : chunk_table_) {
  //     logger_->info("ChunkTable: load key " + entry.first + ", count " +
  //     std::to_string(entry.second));
  // }
}

ChunkTable::~ChunkTable() {}

bool ChunkTable::Use(const std::string &key) {
  chunk_table_[key].ref_count_++;
  return chunk_table_[key].ref_count_ == 1 &&
         chunk_table_[key].snapshot_ref_count_ == 0;
}

bool ChunkTable::Release(const std::string &key) {
  if (chunk_table_.find(key) == chunk_table_.end()) {
    logger_->error("ChunkTable: chunk not found in table");
    throw std::runtime_error("Chunk not found in table");
  }
  chunk_table_[key].ref_count_--;
  auto is_last = chunk_table_[key].ref_count_ == 0 &&
                 chunk_table_[key].snapshot_ref_count_ == 0;
  if (is_last) {
    chunk_table_.erase(key);
  }
  return is_last;
}

void ChunkTable::Persist() {
  // logger_->info("ChunkTable: persist");

  // save chunk table to file
  auto table_path = ssd_path_ + "/" + TABLE_FILE_NAME;
  FILE *table_file = fopen(table_path.c_str(), "w+");
  ftruncate(fileno(table_file), 0);
  size_t num_entries = chunk_table_.size();
  fwrite(&num_entries, sizeof(size_t), 1, table_file);
  for (const auto &entry : chunk_table_) {
    // logger_->info("ChunkTable: persist key " + entry.first + ", count " +
    // std::to_string(entry.second));
    size_t key_len = entry.first.size();
    fwrite(&key_len, sizeof(size_t), 1, table_file);
    fwrite(entry.first.c_str(), sizeof(char), key_len, table_file);
    fwrite(&entry.second.ref_count_, sizeof(int), 1, table_file);
    fwrite(&entry.second.snapshot_ref_count_, sizeof(int), 1, table_file);
  }
  fclose(table_file);

  struct stat st;
  stat(table_path.c_str(), &st);
  buffer_controller_->upload_file(TABLE_FILE_NAME, table_path, st.st_size);
  
  remove(table_path.c_str());
  // upload to cloud
}

void ChunkTable::Print() {
  for (const auto &entry : chunk_table_) {
    // logger_->info("ChunkTable: key " + entry.first + ", count " +
    // std::to_string(entry.second));
    logger_->debug("ChunkTable: key " + entry.first + ", ref_count " +
                   std::to_string(entry.second.ref_count_) +
                   ", snapshot_ref_count " +
                   std::to_string(entry.second.snapshot_ref_count_));
  }
}

void ChunkTable::Snapshot(FILE *snapshot_file) {
  size_t num_entries = chunk_table_.size();
  fwrite(&num_entries, sizeof(size_t), 1, snapshot_file);
  for (auto &entry : chunk_table_) {
    // logger_->info("ChunkTable: persist key " + entry.first + ", ref_count " +
    //              std::to_string(entry.second.ref_count_));
    size_t key_len = entry.first.size();
    fwrite(&key_len, sizeof(size_t), 1, snapshot_file);
    fwrite(entry.first.c_str(), sizeof(char), key_len, snapshot_file);
    fwrite(&entry.second.ref_count_, sizeof(int), 1,
           snapshot_file); // only ref_count is needed for snapshot

    // add snapshot ref count
    if(entry.second.ref_count_ > 0) {
      // only ref_count > 0 means the chunk is used by this snapshot
      entry.second.snapshot_ref_count_++;
    }
  }
}

void ChunkTable::Restore(FILE *snapshot_file) {
  // logger_->debug("ChunkTable: restore");
  size_t num_entries;
  fread(&num_entries, sizeof(size_t), 1, snapshot_file);
  // logger_->debug("ChunkTable: restore entry count " +
                 // std::to_string(num_entries));
  for (size_t i = 0; i < num_entries; i++) {
    size_t key_len;
    fread(&key_len, sizeof(size_t), 1, snapshot_file);
    std::vector<char> key(key_len);
    fread(key.data(), sizeof(char), key_len, snapshot_file);
    std::string key_str(key.begin(), key.end());
    int ref_count;
    fread(&ref_count, sizeof(int), 1, snapshot_file);

    // assert(chunk_table_.find(key_str) !=
    //        chunk_table_.end()); // since snapshot_ref_count will be larger than
    //                             // 0, the key must exist
    if(chunk_table_.find(key_str) == chunk_table_.end()) {
      if(ref_count > 0) {
        logger_->error("ChunkTable: restore key " + key_str + " not found, but ref_count > 0");
        continue;
      }
      chunk_table_[key_str] = RefCounts(0, 0);
    } else {
      chunk_table_[key_str].ref_count_ = ref_count;
    }

    // logger_->debug("ChunkTable: restore key " + key_str + ", ref_count " +
    //                std::to_string(ref_count));
  }
  // logger_->debug("ChunkTable: restore done");
}

void ChunkTable::DeleteSnapshot(FILE *snapshot_file) {
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

    // assert(chunk_table_.find(key_str) !=
    //        chunk_table_.end()); // since snapshot_ref_count will be larger than
    //                             // 0, the key must exist
    if(chunk_table_.find(key_str) == chunk_table_.end()) {
      if(ref_count > 0) {
        logger_->error("ChunkTable: delete snapshot key " + key_str + " not found, but ref_count > 0");
      }
      continue;
    }
    // logger_->debug("ChunkTable: delete snapshot key " + key_str + ", ref_count " +
    //                std::to_string(ref_count));

    if(ref_count > 0) {
      // only ref_count > 0 means the chunk is used by this snapshot
      chunk_table_[key_str].snapshot_ref_count_--;
    }
    if (chunk_table_[key_str].snapshot_ref_count_ == 0 &&
        chunk_table_[key_str].ref_count_ == 0) {
      // remove the key if both ref_count and snapshot_ref_count are 0
      chunk_table_.erase(key_str);
      buffer_controller_->delete_object(key_str);
    }
  }
}

void ChunkTable::SkipSnapshot(FILE* snapshot_file) {
  size_t num_entries;
  fread(&num_entries, sizeof(size_t), 1, snapshot_file);
  for(size_t i = 0; i < num_entries; i++) {
    size_t key_len;
    fread(&key_len, sizeof(size_t), 1, snapshot_file);
    fseek(snapshot_file, key_len, SEEK_CUR); // skip key
    fseek(snapshot_file, sizeof(int), SEEK_CUR); // skip ref_count
  }
}

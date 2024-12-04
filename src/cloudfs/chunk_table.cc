#include "chunk_table.h"

#include <cassert>
#include <stdexcept>
#include <unistd.h>
#include <vector>

const std::string ChunkTable::TABLE_FILE_NAME = ".chunk_table";

ChunkTable::ChunkTable(const std::string &ssd_path,
                       std::shared_ptr<DebugLogger> logger,
                       std::shared_ptr<BufferFileController> buffer_controller)
    : ssd_path_(ssd_path), logger_(std::move(logger)),
      buffer_controller_(std::move(buffer_controller)) {

  // download chunk table persistence file from cloud
  auto table_path = ssd_path_ + "/" + TABLE_FILE_NAME;
  buffer_controller_->download_file(TABLE_FILE_NAME, table_path);
  struct stat st;
  stat(table_path.c_str(), &st);
  if (st.st_size == 0) {
    // no chunk table persistence file
    remove(table_path.c_str());
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
  remove(table_path.c_str());
}

ChunkTable::~ChunkTable() {}

bool ChunkTable::use(const std::string &key) {
  chunk_table_[key].ref_count_++;

  // only when this chunk is the first one in this snapshot,
  // and no other snapshot is using this chunk, it is a new chunk
  return chunk_table_[key].ref_count_ == 1 &&
         chunk_table_[key].snapshot_ref_count_ == 0;
}

bool ChunkTable::release(const std::string &key) {
  if (chunk_table_.find(key) == chunk_table_.end()) {
    logger_->error("ChunkTable: chunk not found in table");
    throw std::runtime_error("Chunk not found in table");
  }
  chunk_table_[key].ref_count_--;

  // only when this chunk is the last one in this snapshot,
  // and no other snapshot is using this chunk, it is no longer in use
  auto is_last = chunk_table_[key].ref_count_ == 0 &&
                 chunk_table_[key].snapshot_ref_count_ == 0;
  if (is_last) {
    chunk_table_.erase(key);
  }
  return is_last;
}

void ChunkTable::persist() {
  if (chunk_table_.empty()) {
    // no chunk info, no need to persist
    return;
  }

  // save chunk table to file
  auto table_path = ssd_path_ + "/" + TABLE_FILE_NAME;
  FILE *table_file = fopen(table_path.c_str(), "w+");
  ftruncate(fileno(table_file), 0);
  size_t num_entries = chunk_table_.size();
  fwrite(&num_entries, sizeof(size_t), 1, table_file);
  for (const auto &entry : chunk_table_) {
    size_t key_len = entry.first.size();
    fwrite(&key_len, sizeof(size_t), 1, table_file);
    fwrite(entry.first.c_str(), sizeof(char), key_len, table_file);
    fwrite(&entry.second.ref_count_, sizeof(int), 1, table_file);
    fwrite(&entry.second.snapshot_ref_count_, sizeof(int), 1, table_file);
  }
  fclose(table_file);

  struct stat st;
  stat(table_path.c_str(), &st);

  // upload to cloud
  buffer_controller_->upload_file(TABLE_FILE_NAME, table_path, st.st_size);
  remove(table_path.c_str());
}

void ChunkTable::print() {
  for (const auto &entry : chunk_table_) {
    logger_->debug("ChunkTable: key " + entry.first + ", ref_count " +
                   std::to_string(entry.second.ref_count_) +
                   ", snapshot_ref_count " +
                   std::to_string(entry.second.snapshot_ref_count_));
  }
}

void ChunkTable::snapshot(FILE *snapshot_file) {
  // save chunk table of current snapshot
  size_t num_entries = chunk_table_.size();
  fwrite(&num_entries, sizeof(size_t), 1, snapshot_file);
  for (auto &entry : chunk_table_) {
    size_t key_len = entry.first.size();
    fwrite(&key_len, sizeof(size_t), 1, snapshot_file);
    fwrite(entry.first.c_str(), sizeof(char), key_len, snapshot_file);
    fwrite(&entry.second.ref_count_, sizeof(int), 1,
           snapshot_file); // only ref_count is needed for snapshot

    // add snapshot ref count
    if (entry.second.ref_count_ > 0) {
      // ref_count > 0 means the chunk is used by this snapshot
      entry.second.snapshot_ref_count_++; // increase snapshot ref count
    }
  }
}

void ChunkTable::restore(FILE *snapshot_file) {
  // restore chunk table from snapshot
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

    if (chunk_table_.find(key_str) == chunk_table_.end()) {
      if (ref_count > 0) {
        logger_->error("ChunkTable: restore key " + key_str +
                       " not found, but ref_count > 0");
        continue;
      }
      chunk_table_[key_str] = RefCounts(0, 0);
    } else {
      chunk_table_[key_str].ref_count_ = ref_count;
    }
  }
}

void ChunkTable::snapshot_deleted(FILE *snapshot_file) {
  // delete a snapshot, so snapshot ref count needs to be decreased

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

    if (chunk_table_.find(key_str) == chunk_table_.end()) {
      if (ref_count > 0) {
        logger_->error("ChunkTable: delete snapshot key " + key_str +
                       " not found, but ref_count > 0");
      }
      continue;
    }

    if (ref_count > 0) {
      // ref_count > 0 means the chunk is used by this snapshot
      chunk_table_[key_str]
          .snapshot_ref_count_--; // decrease snapshot ref count
    }
    if (chunk_table_[key_str].snapshot_ref_count_ == 0 &&
        chunk_table_[key_str].ref_count_ == 0) {
      // remove the key if both ref_count and snapshot_ref_count are 0
      chunk_table_.erase(key_str);
      buffer_controller_->delete_object(key_str);
    }
  }
}

void ChunkTable::skip_snapshot(FILE *snapshot_file) {
  // skip chunk table part of a snapshot
  size_t num_entries;
  fread(&num_entries, sizeof(size_t), 1, snapshot_file);
  for (size_t i = 0; i < num_entries; i++) {
    size_t key_len;
    fread(&key_len, sizeof(size_t), 1, snapshot_file);
    fseek(snapshot_file, key_len, SEEK_CUR);     // skip key
    fseek(snapshot_file, sizeof(int), SEEK_CUR); // skip ref_count
  }
}

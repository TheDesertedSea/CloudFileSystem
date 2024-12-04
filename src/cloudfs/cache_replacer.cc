#include "cache_replacer.h"

#include <stdexcept>
#include <unistd.h>

std::string CacheReplacer::PERSIST_FILE_PATH = ".cache_replacer";

CacheReplacer::CacheReplacer(struct cloudfs_state *state,
                             std::shared_ptr<DebugLogger> logger)
    : state_(state), logger_(logger) {}

CacheReplacer::~CacheReplacer() {}

LRUCacheReplacer::LRUCacheReplacer(struct cloudfs_state *state,
                                   std::shared_ptr<DebugLogger> logger)
    : CacheReplacer(state, logger) {
  head_ = nullptr;
  tail_ = nullptr;

  // load persisted cache entries info
  auto persist_path = state_->ssd_path + PERSIST_FILE_PATH;
  FILE *persist_file = fopen(persist_path.c_str(), "r");
  if (persist_file == NULL) {
    logger_->info(
        "CacheReplacer::LRUCacheReplacer: persist file not found, path: " +
        persist_path + ", skip loading");
    return;
  }

  // read cache_entries_
  size_t cache_entries_size;
  fread(&cache_entries_size, sizeof(size_t), 1, persist_file);
  logger_->info("CacheReplacer::LRUCacheReplacer: load cache_entries_ size: " +
                std::to_string(cache_entries_size));

  for (size_t i = 0; i < cache_entries_size; ++i) {
    size_t key_len;
    fread(&key_len, sizeof(size_t), 1, persist_file);
    std::vector<char> key(key_len);
    fread(key.data(), sizeof(char), key_len, persist_file);
    auto key_str = std::string(key.begin(), key.end());
    logger_->info("CacheReplacer::LRUCacheReplacer: load key: " + key_str);

    auto new_entry = new CacheEntry();
    new_entry->key_ = key_str;

    if (tail_ == nullptr) {
      head_ = new_entry;
      tail_ = new_entry;
    } else {
      tail_->next_ = new_entry;
      new_entry->prev_ = tail_;
      tail_ = new_entry;
    }
  }
  fclose(persist_file);
  unlink(persist_path.c_str());

  logger_->info("CacheReplacer::LRUCacheReplacer: persist file loaded, path: " +
                persist_path);
}

LRUCacheReplacer::~LRUCacheReplacer() {
  auto cur = head_;
  while (cur != nullptr) {
    auto next = cur->next_;
    delete cur;
    cur = next;
  }

  head_ = nullptr;
  tail_ = nullptr;

  cache_entries_.clear();
}

void LRUCacheReplacer::access(const std::string &key) {
  if (cache_entries_.find(key) != cache_entries_.end()) {
    // already in cache, move to head
    auto entry = cache_entries_[key];
    if (entry == head_) {
      return;
    }

    if (entry->prev_ != nullptr) {
      entry->prev_->next_ = entry->next_;
    }
    if (entry->next_ != nullptr) {
      entry->next_->prev_ = entry->prev_;
    }
    if (entry == tail_) {
      tail_ = entry->prev_;
    }

    entry->prev_ = nullptr;
    entry->next_ = head_;
    head_->prev_ = entry;
    head_ = entry;
    return;
  }

  // not in cache, add to head
  auto new_entry = new CacheEntry();
  new_entry->key_ = key;
  new_entry->next_ = head_;
  if (head_ != nullptr) {
    head_->prev_ = new_entry;
  }
  head_ = new_entry;
  if (tail_ == nullptr) {
    tail_ = head_;
  }

  cache_entries_[key] = new_entry;
}

void LRUCacheReplacer::evict(std::string &key) {
  if (tail_ == nullptr) {
    logger_->error("CacheReplacer::Evict: cache_entries_ is empty");
    throw std::runtime_error("CacheReplacer::Evict: cache_entries_ is empty");
  }

  // evict tail
  key = tail_->key_;
  if (tail_->prev_ != nullptr) {
    tail_->prev_->next_ = nullptr;
  }
  tail_ = tail_->prev_;
  if (tail_ == nullptr) {
    head_ = nullptr;
  }

  delete cache_entries_[key];
  cache_entries_.erase(key);
}

void LRUCacheReplacer::remove(const std::string &key) {
  if (cache_entries_.find(key) == cache_entries_.end()) {
    logger_->error("CacheReplacer::Remove: key not found in cache_entries_, "
                   "try to remove key: " +
                   key);
    throw std::runtime_error("CacheReplacer::Remove: key not found in "
                             "cache_entries_, try to remove key: " +
                             key);
  }

  // remove from cache
  auto entry = cache_entries_[key];
  if (entry->prev_ != nullptr) {
    entry->prev_->next_ = entry->next_;
  }
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry->prev_;
  }
  if (entry == head_) {
    head_ = entry->next_;
  }
  if (entry == tail_) {
    tail_ = entry->prev_;
  }

  delete entry;
  cache_entries_.erase(key);
}

void LRUCacheReplacer::persist() {
  logger_->debug("CacheReplacer::Persist: start to persist");

  // save cache entries info to file
  auto persist_path = state_->ssd_path + PERSIST_FILE_PATH;
  FILE *persist_file = fopen(persist_path.c_str(), "w");
  if (persist_file == NULL) {
    logger_->error("CacheReplacer::Persist: open persist file failed, path: " +
                   persist_path);
    return;
  }

  size_t cache_entries_size = cache_entries_.size();
  fwrite(&cache_entries_size, sizeof(size_t), 1, persist_file);

  auto cur = head_;
  while (cur != nullptr) {
    size_t key_len = cur->key_.size();
    fwrite(&key_len, sizeof(size_t), 1, persist_file);
    fwrite(cur->key_.c_str(), 1, key_len, persist_file);
    cur = cur->next_;
  }

  fclose(persist_file);

  logger_->debug("CacheReplacer::Persist: persist file saved, path: " +
                 persist_path);
}

void LRUCacheReplacer::print_cache() {
  auto cur = head_;
  while (cur != nullptr) {
    logger_->debug("CacheReplacer::PrintCache: key: " + cur->key_);
    cur = cur->next_;
  }
}

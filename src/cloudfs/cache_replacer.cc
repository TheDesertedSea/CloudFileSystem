#include "cache_replacer.h"

#include <algorithm>
#include <stdexcept>

std::string CacheReplacer::PERSIST_FILE_PATH = ".cache_replacer";

size_t LRUKCacheReplacer::CacheEntry::current_timestamp_ = 0;
int LRUKCacheReplacer::CacheEntry::k_ = 0;

CacheReplacer::CacheReplacer(struct cloudfs_state* state, std::shared_ptr<DebugLogger> logger) : state_(state), logger_(logger) {

}

CacheReplacer::~CacheReplacer() {

}

LRUKCacheReplacer::LRUKCacheReplacer(size_t k, struct cloudfs_state* state, std::shared_ptr<DebugLogger> logger) : CacheReplacer(state, logger) {
    auto persist_path = state_->ssd_path + PERSIST_FILE_PATH;
    FILE* persist_file = fopen(persist_path.c_str(), "r");
    if(persist_file == NULL) {
        logger_->info("CacheReplacer::LRUKCacheReplacer: persist file not found, path: " + persist_path + ", skip loading");
        CacheEntry::k_ = k;
        return;
    }

    // read k
    fread(&CacheEntry::k_, sizeof(size_t), 1, persist_file);
    logger_->info("CacheReplacer::LRUKCacheReplacer: load k: " + std::to_string(CacheEntry::k_));

    // read cache_entries_
    size_t cache_entries_size;
    fread(&cache_entries_size, sizeof(size_t), 1, persist_file);
    for(size_t i = 0; i < cache_entries_size; ++i) {
        size_t key_len;
        fread(&key_len, sizeof(size_t), 1, persist_file);
        std::vector<char> key(key_len);
        fread(key.data(), sizeof(char), key_len, persist_file);
        auto key_str = std::string(key.begin(), key.end());
        logger_->debug("CacheReplacer::LRUKCacheReplacer: load key: " + key_str);

        CacheEntry entry;
        size_t ts_count;
        fread(&ts_count, sizeof(size_t), 1, persist_file);
        for(size_t j = 0; j < ts_count; ++j) {
            size_t ts;
            fread(&ts, sizeof(size_t), 1, persist_file);
            entry.timestamps_.push_back(ts);
            logger_->debug("CacheReplacer::LRUKCacheReplacer: load timestamp: " + std::to_string(ts));
        }

        cache_entries_[key_str] = entry;
    }

    fclose(persist_file);

    remove(persist_path.c_str());

    LRUKCacheReplacer::CacheEntry::k_ = k;

    logger_->debug("CacheReplacer::LRUKCacheReplacer: persist file loaded, path: " + persist_path);
}

LRUKCacheReplacer::~LRUKCacheReplacer() {

}

void LRUKCacheReplacer::Access(const std::string& key) {
    if(cache_entries_.find(key) == cache_entries_.end()) {
        cache_entries_[key] = CacheEntry();
    }
    cache_entries_[key].RecordAccess();
    logger_->debug("CacheReplacer::LRUKCacheReplacer: access key: " + key);
}

void LRUKCacheReplacer::Evict(std::string& key) {
    if(cache_entries_.empty()) {
        logger_->error("CacheReplacer::Evict: cache_entries_ is empty");
        throw std::runtime_error("CacheReplacer::Evict: cache_entries_ is empty");
    }

    auto entry = std::max_element(cache_entries_.begin(), cache_entries_.end());
    key = entry->first;
    cache_entries_.erase(entry);
    logger_->debug("CacheReplacer::Evict: evict key: " + key);
}

void LRUKCacheReplacer::Remove(const std::string& key) {
    if(cache_entries_.find(key) == cache_entries_.end()) {
        logger_->error("CacheReplacer::Remove: key not found in cache_entries_, try to remove key: " + key);
        throw std::runtime_error("CacheReplacer::Remove: key not found in cache_entries_, try to remove key: " + key);
    }
    cache_entries_.erase(key);
}

void LRUKCacheReplacer::Persist() {
    logger_->debug("CacheReplacer::Persist: start to persist");

    auto persist_path = state_->ssd_path + PERSIST_FILE_PATH;
    FILE* persist_file = fopen(persist_path.c_str(), "w");
    if(persist_file == NULL) {
        logger_->error("CacheReplacer::Persist: open persist file failed, path: " + persist_path);
        return;
    }

    // write k
    fwrite(&CacheEntry::k_, sizeof(size_t), 1, persist_file);
    logger_->debug("CacheReplacer::Persist: persist k: " + std::to_string(CacheEntry::k_));

    // write cache_entries_
    size_t cache_entries_size = cache_entries_.size();
    fwrite(&cache_entries_size, sizeof(size_t), 1, persist_file);
    for(const auto& entry : cache_entries_) {
        size_t key_len = entry.first.size();
        fwrite(&key_len, sizeof(size_t), 1, persist_file);
        fwrite(entry.first.c_str(), 1, key_len, persist_file);
    
        logger_->debug("CacheReplacer::Persist: persist key: " + entry.first);

        size_t ts_count = entry.second.timestamps_.size();
        fwrite(&ts_count, sizeof(size_t), 1, persist_file);
        for(const auto& ts : entry.second.timestamps_) {
            fwrite(&ts, sizeof(size_t), 1, persist_file);
            logger_->debug("CacheReplacer::Persist: persist timestamp: " + std::to_string(ts));
        }
    }

    fclose(persist_file);

    logger_->debug("CacheReplacer::Persist: persist file saved, path: " + persist_path);
}

void LRUKCacheReplacer::PrintCache() {
    for(const auto& entry : cache_entries_) {
        logger_->debug("CacheReplacer::PrintCache: key: " + entry.first);
    }
}

LRUCacheReplacer::LRUCacheReplacer(struct cloudfs_state* state, std::shared_ptr<DebugLogger> logger) : CacheReplacer(state, logger) {
    head_ = nullptr;
    tail_ = nullptr;

    auto persist_path = state_->ssd_path + PERSIST_FILE_PATH;
    FILE* persist_file = fopen(persist_path.c_str(), "r");
    if(persist_file == NULL) {
        logger_->info("CacheReplacer::LRUCacheReplacer: persist file not found, path: " + persist_path + ", skip loading");
        return;
    }

    // read cache_entries_
    size_t cache_entries_size;
    fread(&cache_entries_size, sizeof(size_t), 1, persist_file);
    logger_->info("CacheReplacer::LRUCacheReplacer: load cache_entries_ size: " + std::to_string(cache_entries_size));

    for(size_t i = 0; i < cache_entries_size; ++i) {
        size_t key_len;
        fread(&key_len, sizeof(size_t), 1, persist_file);
        std::vector<char> key(key_len);
        fread(key.data(), sizeof(char), key_len, persist_file);
        auto key_str = std::string(key.begin(), key.end());
        logger_->info("CacheReplacer::LRUCacheReplacer: load key: " + key_str);

        auto new_entry = new CacheEntry();
        new_entry->key_ = key_str;

        if(tail_ == nullptr) {
            head_ = new_entry;
            tail_ = new_entry;
        } else {
            tail_->next_ = new_entry;
            new_entry->prev_ = tail_;
            tail_ = new_entry;
        }
    }

    fclose(persist_file);

    remove(persist_path.c_str());

    logger_->info("CacheReplacer::LRUCacheReplacer: persist file loaded, path: " + persist_path);
}

LRUCacheReplacer::~LRUCacheReplacer() {
    auto cur = head_;
    while(cur != nullptr) {
        auto next = cur->next_;
        delete cur;
        cur = next;
    }

    head_ = nullptr;
    tail_ = nullptr;

    cache_entries_.clear();
}

void LRUCacheReplacer::Access(const std::string& key) {
    logger_->debug("CacheReplacer::Access: key: " + key);
    if(cache_entries_.find(key) != cache_entries_.end()) {
        // move to head
        auto entry = cache_entries_[key];
        if(entry == head_) {
            return;
        }

        if(entry->prev_ != nullptr) {
            entry->prev_->next_ = entry->next_;
        }
        if(entry->next_ != nullptr) {
            entry->next_->prev_ = entry->prev_;
        }

        if(entry == tail_) {
            tail_ = entry->prev_;
        }

        entry->prev_ = nullptr;
        entry->next_ = head_;
        head_->prev_ = entry;
        head_ = entry;
        return;
    }

    auto new_entry = new CacheEntry();
    new_entry->key_ = key;
    new_entry->next_ = head_;
    if(head_ != nullptr) {
        head_->prev_ = new_entry;
    }
    head_ = new_entry;
    if(tail_ == nullptr) {
        tail_ = head_;
    }

    cache_entries_[key] = new_entry;
}

void LRUCacheReplacer::Evict(std::string& key) {
    logger_->debug("CacheReplacer::Evict: key: " + key);
    if(tail_ == nullptr) {
        logger_->error("CacheReplacer::Evict: cache_entries_ is empty");
        throw std::runtime_error("CacheReplacer::Evict: cache_entries_ is empty");
    }

    key = tail_->key_;

    if(tail_->prev_ != nullptr) {
        tail_->prev_->next_ = nullptr;
    }
    tail_ = tail_->prev_;
    if(tail_ == nullptr) {
        head_ = nullptr;
    }

    delete cache_entries_[key];
    cache_entries_.erase(key);
}

void LRUCacheReplacer::Remove(const std::string& key) {
    logger_->debug("CacheReplacer::Remove: key: " + key);
    if(cache_entries_.find(key) == cache_entries_.end()) {
        logger_->error("CacheReplacer::Remove: key not found in cache_entries_, try to remove key: " + key);
        throw std::runtime_error("CacheReplacer::Remove: key not found in cache_entries_, try to remove key: " + key);
    }

    auto entry = cache_entries_[key];
    if(entry->prev_ != nullptr) {
        entry->prev_->next_ = entry->next_;
    }
    if(entry->next_ != nullptr) {
        entry->next_->prev_ = entry->prev_;
    }
    if(entry == head_) {
        head_ = entry->next_;
    }
    if(entry == tail_) {
        tail_ = entry->prev_;
    }

    delete entry;
    cache_entries_.erase(key);
}

void LRUCacheReplacer::Persist() {
    logger_->debug("CacheReplacer::Persist: start to persist");

    auto persist_path = state_->ssd_path + PERSIST_FILE_PATH;
    FILE* persist_file = fopen(persist_path.c_str(), "w");
    if(persist_file == NULL) {
        logger_->error("CacheReplacer::Persist: open persist file failed, path: " + persist_path);
        return;
    }

    size_t cache_entries_size = cache_entries_.size();
    fwrite(&cache_entries_size, sizeof(size_t), 1, persist_file);

    auto cur = head_;
    while(cur != nullptr) {
        size_t key_len = cur->key_.size();
        fwrite(&key_len, sizeof(size_t), 1, persist_file);
        fwrite(cur->key_.c_str(), 1, key_len, persist_file);
        cur = cur->next_;
    }

    fclose(persist_file);

    logger_->debug("CacheReplacer::Persist: persist file saved, path: " + persist_path);
}

void LRUCacheReplacer::PrintCache() {
    auto cur = head_;
    while(cur != nullptr) {
        logger_->debug("CacheReplacer::PrintCache: key: " + cur->key_);
        cur = cur->next_;
    }
}

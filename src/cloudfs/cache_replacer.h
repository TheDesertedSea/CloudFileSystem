#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>

#include "cloudfs.h"
#include "util.h"

class CacheReplacer {
    
public:
    static std::string PERSIST_FILE_PATH;

    struct cloudfs_state* state_;
    std::shared_ptr<DebugLogger> logger_;

    CacheReplacer(struct cloudfs_state* state, std::shared_ptr<DebugLogger> logger);
    virtual ~CacheReplacer();

    virtual void Access(const std::string& key) = 0;

    virtual void Evict(std::string& key) = 0;

    virtual void Remove(const std::string& key) = 0;

    virtual void Persist() = 0;

    virtual void PrintCache() = 0;
};

class LRUKCacheReplacer : public CacheReplacer {

    struct CacheEntry {
        static size_t current_timestamp_;
        static int k_;

        std::deque<size_t> timestamps_;

        CacheEntry() {
            
        }

        size_t BackwardDistance() const {
            if((int)timestamps_.size() < k_) {
                return std::numeric_limits<size_t>::max();
            }

            return current_timestamp_ - timestamps_.back();
        }

        void RecordAccess() {
            current_timestamp_ = static_cast<size_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());

            timestamps_.push_front(current_timestamp_);
            if((int)timestamps_.size() > k_) {
                timestamps_.pop_back();
            }
        }

        bool operator<(const CacheEntry& other) const {
            auto my_distance = BackwardDistance();
            auto other_distance = other.BackwardDistance();
            if(my_distance == other_distance) {
                return timestamps_.back() > other.timestamps_.back();
            }
            return my_distance < other_distance;
        }
    };

    std::unordered_map<std::string, CacheEntry> cache_entries_;

public:
    LRUKCacheReplacer(size_t k, struct cloudfs_state* state, std::shared_ptr<DebugLogger> logger);
    ~LRUKCacheReplacer();

    void Access(const std::string& key) override;
    void Evict(std::string& key) override;
    void Remove(const std::string& key) override;
    void Persist() override;
    void PrintCache() override;
};

class LRUCacheReplacer : public CacheReplacer {
    struct CacheEntry {
        std::string key_;
        CacheEntry* prev_;
        CacheEntry* next_;

        CacheEntry() : prev_(nullptr), next_(nullptr) {}
    };

    CacheEntry* head_;
    CacheEntry* tail_;

    std::unordered_map<std::string, CacheEntry*> cache_entries_;

public:
    LRUCacheReplacer(struct cloudfs_state* state, std::shared_ptr<DebugLogger> logger);
    ~LRUCacheReplacer();

    void Access(const std::string& key) override;
    void Evict(std::string& key) override;
    void Remove(const std::string& key) override;
    void Persist() override;
    void PrintCache() override;
};

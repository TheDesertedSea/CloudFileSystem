/**
 * @file cache_replacer.h
 * @brief Cache replacer for LRU and LRUK
 * @author Cundao Yu <cundaoy@andrew.cmu.edu>
 */

#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cloudfs.h"
#include "util.h"

/**
 * Cache replacer interface
 */
class CacheReplacer {
public:
  static std::string PERSIST_FILE_PATH; // path to persist file

  struct cloudfs_state *state_;         // cloudfs state
  std::shared_ptr<DebugLogger> logger_; // logger

  CacheReplacer(struct cloudfs_state *state,
                std::shared_ptr<DebugLogger> logger);
  virtual ~CacheReplacer();

  /**
   * Access a key
   * @param key The key to access
   */
  virtual void access(const std::string &key) = 0;

  /**
   * Evict a key
   * @param key The key to evict
   */
  virtual void evict(std::string &key) = 0;

  /**
   * Remove a key
   * @param key The key to remove
   */
  virtual void remove(const std::string &key) = 0;

  /**
   * Persist cache entries info
   */
  virtual void persist() = 0;

  /**
   * Print cache entries info
   */
  virtual void print_cache() = 0;
};

/**
 * LRU cache replacer
 */
class LRUCacheReplacer : public CacheReplacer {
  /**
   * Cache entry, doubly linked list node
   */
  struct CacheEntry {
    std::string key_;  // key
    CacheEntry *prev_; // previous node
    CacheEntry *next_; // next node

    CacheEntry() : prev_(nullptr), next_(nullptr) {}
  };

  CacheEntry *head_; // head of the doubly linked list, least recently used
  CacheEntry *tail_; // tail of the doubly linked list, most recently used

  std::unordered_map<std::string, CacheEntry *>
      cache_entries_; // key -> CacheEntry*

public:
  LRUCacheReplacer(struct cloudfs_state *state,
                   std::shared_ptr<DebugLogger> logger);
  ~LRUCacheReplacer();

  void access(const std::string &key) override;
  void evict(std::string &key) override;
  void remove(const std::string &key) override;
  void persist() override;
  void print_cache() override;
};

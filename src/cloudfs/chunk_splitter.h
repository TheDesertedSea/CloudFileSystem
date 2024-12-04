/**
 * @file chunk_splitter.h
 * @brief Chunk splitter for deduplication
 * @author Cundao Yu <cundaoy@andrew.cmu.edu>
 */

#pragma once

#include "dedup.h"
#include <openssl/evp.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <vector>

/**
 * Chunk info
 */
struct Chunk {
  off_t start_;     // start offset of the chunk in complete file
  size_t len_;      // length of the chunk
  std::string key_; // key of the chunk

  Chunk() : start_(0), len_(0) {}
  Chunk(off_t start, size_t len, const std::vector<char> &key)
      : start_(start), len_(len) {
    for (auto &c : key) {
      key_ += char_to_hex(static_cast<unsigned char>(c));
    }
  }
  Chunk(off_t start, size_t len, const std::string &key)
      : start_(start), len_(len), key_(key) {}

private:
  /**
   * Convert a character to hex string
   * @param c The character to convert
   * @return The hex string
   */
  static std::string char_to_hex(unsigned char c) {
    char buf[3];
    sprintf(buf, "%02x", c);
    return std::string(buf);
  }
};

/**
 * Chunk splitter
 * Using Rabin fingerprint to split file into chunks
 * Using MD5 to generate the key of the chunk
 */
class ChunkSplitter {

  off_t chunk_start_; // start offset of the current chunk in complete file
  off_t chunk_len_;   // length of the current chunk

  rabinpoly_t *rp_; // Rabin polynomial

  const EVP_MD *md_; // MD5 context
  EVP_MD_CTX *mdctx_;

public:
  ChunkSplitter(int window_size, int avg_segment_size, int min_segment_size,
                int max_segment_size);
  ~ChunkSplitter();

  /**
   * Initialize the chunk splitter
   * @param start The start offset in complete file, chunk splitter will start
   * from this offset
   */
  void init(off_t start);

  /**
   * Consume a buffer and generate new chunks
   * @param buf The buffer to consume
   * @param len The length of the buffer
   * @return New generated chunks
   */
  std::vector<Chunk> get_chunks_next(const char *buf, size_t len);

  /**
   * Get the last chunk
   * @return The last chunk
   */
  Chunk get_chunk_last();
};
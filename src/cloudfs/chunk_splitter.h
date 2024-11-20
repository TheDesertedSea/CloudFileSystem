#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <openssl/evp.h>
#include <string.h>
#include "dedup.h"

struct Chunk{
  off_t start_;
  size_t len_;
  std::string key_;

  Chunk(): start_(0), len_(0) {}
  Chunk(off_t start, size_t len, const std::vector<char>& key) : start_(start), len_(len) {
    for(auto& c: key) {
      key_ += char_to_hex(static_cast<unsigned char>(c));
    }
  }
  Chunk(off_t start, size_t len, const std::string& key) : start_(start), len_(len), key_(key) {}

private:
  static std::string char_to_hex(unsigned char c) {
    char buf[3];
    sprintf(buf, "%02x", c);
    return std::string(buf);
  }
};

class ChunkSplitter {

  off_t chunk_start_;
  off_t chunk_len_;

  rabinpoly_t *rp_;

  const EVP_MD *md_;
  EVP_MD_CTX *mdctx_;

  public:
    ChunkSplitter(int window_size, int avg_segment_size, int min_segment_size, int max_segment_size);
    ~ChunkSplitter();

    // start: the start offset of the file (the start offset of the first chunk)
    void init(off_t start);
    std::vector<Chunk> get_chunks_next(const char *buf, size_t len);
    Chunk get_chunk_last();
};
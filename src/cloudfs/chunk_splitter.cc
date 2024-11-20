#include "chunk_splitter.h"
#include <stdexcept>

ChunkSplitter::ChunkSplitter(int window_size, int avg_segment_size, int min_segment_size, int max_segment_size) {
  rp_ = rabin_init(window_size, avg_segment_size, min_segment_size, max_segment_size);

  md_ = EVP_MD_fetch(NULL, "MD5", NULL);
  mdctx_ = EVP_MD_CTX_new();
}

ChunkSplitter::~ChunkSplitter() {
  EVP_MD_CTX_free(mdctx_);
  EVP_MD_free(const_cast<EVP_MD*>(md_));

  rabin_free(&rp_);
}

void ChunkSplitter::init(off_t start) {
  chunk_start_ = start;
  chunk_len_ = 0;
  EVP_DigestInit_ex(mdctx_, md_, NULL);
}

std::vector<Chunk> ChunkSplitter::get_chunks_next(const char *buf, size_t len) {
  std::vector<Chunk> chunks;
  int len_processed = 0;
  int new_segment = 0;
  while((len_processed = rabin_segment_next(rp_, buf, len, &new_segment)) > 0) {
      EVP_DigestUpdate(mdctx_, buf, len_processed);
      chunk_len_ += len_processed;

      if (new_segment) {
          unsigned char md_value[EVP_MAX_MD_SIZE];
          unsigned int md_len;
          EVP_DigestFinal_ex(mdctx_, md_value, &md_len);

          std::vector<char> key(md_len);
          for (unsigned int i = 0; i < md_len; i++) {
              key[i] = md_value[i];
          }

          chunks.emplace_back(chunk_start_, chunk_len_, key);

          chunk_start_ += chunk_len_;
          chunk_len_ = 0;

          EVP_DigestInit_ex(mdctx_, md_, NULL);
      }

      buf += len_processed;
      len -= len_processed;

      if (!len) {
          break;
      }
  }
  if (len_processed == -1) {
      throw std::runtime_error("Failed to process the segment");
  }

  return chunks;
}

Chunk ChunkSplitter::get_chunk_last() {
  if(chunk_len_ == 0) {
      return {};
  }

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;
  EVP_DigestFinal_ex(mdctx_, md_value, &md_len);

  std::vector<char> key(md_len);
  for (unsigned int i = 0; i < md_len; i++) {
      key[i] = md_value[i];
  }

  return {chunk_start_, static_cast<size_t>(chunk_len_), key};
}

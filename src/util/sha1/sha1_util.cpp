/*
 * This file is part of DXMT, Copyright (c) 2023 Feifan He
 *
 * Derived from a part of DXVK (originally under zlib License),
 * Copyright (c) 2017 Philip Rebohle
 * Copyright (c) 2019 Joshua Ashton
 *
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#include "sha1.h"
#include "sha1_util.hpp"

namespace dxmt {

std::string
Sha1Digest::string() const {
  static const char nibbles[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  std::string result;
  result.resize(2 * sizeof(data));

  for (uint32_t i = 0; i < sizeof(data); i++) {
    result.at(2 * i + 0) = nibbles[(data[i] >> 4) & 0xF];
    result.at(2 * i + 1) = nibbles[(data[i] >> 0) & 0xF];
  }

  return result;
}

Sha1HashState::Sha1HashState() {
  SHA1Init(&ctx);
}

Sha1HashState &
Sha1HashState::update(const void *data, size_t size) {
  SHA1Update(&ctx, reinterpret_cast<const uint8_t *>(data), size);
  return *this;
}

Sha1Digest
Sha1HashState::final() {
  Sha1Digest digest;
  SHA1Final(digest.data, &ctx);
  return digest;
}

} // namespace dxmt
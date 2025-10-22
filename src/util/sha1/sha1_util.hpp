/*
 * This file is part of DXMT, Copyright (c) 2023 Feifan He
 *
 * Derived from a part of DXVK (originally under zlib License),
 * Copyright (c) 2017 Philip Rebohle
 * Copyright (c) 2019 Joshua Ashton
 *
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include "./sha1.h"

namespace dxmt {

struct Sha1Digest {
  uint8_t data[20];

  bool
  operator==(const Sha1Digest &other) const {
    return !std::memcmp(data, other.data, sizeof(data));
  }

  bool
  operator!=(const Sha1Digest &other) const {
    return !this->operator==(other);
  }

  std::string string() const;
};

struct Sha1Data {
  const void *data; 
  size_t size;
};

class Sha1HashState {
public:
  Sha1HashState();

  Sha1HashState &update(const void *data, size_t size);

  template <typename T>
  Sha1HashState &
  update(const T &data) {
    return update(&data, sizeof(T));
  }

  Sha1Digest final();

  static Sha1Digest
  compute(const void *data, size_t size) {
    Sha1HashState h;
    h.update(data, size);
    return h.final();
  }

private:
  _SHA1_CTX ctx;
};

} // namespace dxmt

namespace std {
template <> struct hash<dxmt::Sha1Digest> {
  size_t
  operator()(const dxmt::Sha1Digest &v) const noexcept {
    std::hash<std::string_view> h;
    return h(std::string_view({reinterpret_cast<const char *>(v.data), sizeof(v)}));
  };
};

template <> struct equal_to<dxmt::Sha1Digest> {
  bool
  operator()(const dxmt::Sha1Digest &x, const dxmt::Sha1Digest &y) const {
    constexpr size_t binsize = sizeof(x);
    return std::string_view({reinterpret_cast<const char *>(x.data), binsize}) ==
           std::string_view({reinterpret_cast<const char *>(y.data), binsize});
  }
};
} // namespace std

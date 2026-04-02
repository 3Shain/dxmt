#pragma once

#include <cassert>
#include <cstdint>

namespace dxmt {

struct TextureViewDescriptor;

class ResourceSubsetState {
public:
  struct BufferSlice {
    uint64_t tag    : 2;
    uint64_t offset : 31;
    uint64_t length : 31;
  };

  struct TextureBitmaskSubset {
    uint64_t tag  : 2;
    uint64_t mask : 62;
  };

  struct TextureSubset {
    uint64_t tag          : 2;
    uint64_t planar_mask  : 30;
    uint64_t mip_start    : 4;
    uint64_t array_start  : 12;
    uint64_t mip_end    : 4;
    uint64_t array_end : 12;
  };

  inline bool
  overlapWith(const ResourceSubsetState &other) const {
    if (encoded_tag && other.encoded_tag && encoded_tag != other.encoded_tag)
      return false;

    if (encoded_tag == 0b01) {
      return (buffer.offset < other.buffer.offset + other.buffer.length) &&
             (other.buffer.offset < buffer.offset + buffer.length);
    } else if (encoded_tag == 0b11) {
      return texture_bitmask.mask & other.texture_bitmask.mask;
    } else if (encoded_tag == 0b10) {
      return (texture.planar_mask & other.texture.planar_mask) &&
             (texture.mip_start < other.texture.mip_end) &&
             (other.texture.mip_start < texture.mip_end) &&
             (texture.array_start < other.texture.array_end) &&
             (other.texture.array_start < texture.array_end);
    }
    return true;
  }

  ResourceSubsetState() {
    // whole resource
    encoded_tag = 0;
  }

  ResourceSubsetState(uint32_t buffer_offset, uint32_t buffer_length) {
    encoded_tag = 0b01;
    buffer.offset = buffer_offset;
    buffer.length = buffer_length;
  }

  /**
  NOTE: we assume the view format is in the same 'format family' of resource allocation format, so that we can conclude
  the total planar count from view format. This is not really the case for video format (e.g. NV12, YUY2), where the
  view format can be ordinary format like RG8 or R16
  So don't use this constructor for video textures/views (which is not implemented yet).
  */
  ResourceSubsetState(const TextureViewDescriptor *desc, uint32_t total_mip_count, uint32_t total_array_size, uint32_t ignore_planar_mask = 0);

private:
  union {
    BufferSlice buffer;
    TextureBitmaskSubset texture_bitmask;
    TextureSubset texture;
    struct {
      uint64_t encoded_tag : 2;
      uint64_t reserved    : 62;
    };
  };
};

}; // namespace dxmt
#pragma once
#include "Metal.hpp"
#include "dxmt_deptrack.hpp"
#include "dxmt_residency.hpp"
#include "dxmt_allocation.hpp"
#include "rc/util_rc_ptr.hpp"
#include "thread.hpp"
#include "util_flags.hpp"

namespace dxmt {

enum class TextureAllocationFlag : uint32_t {
  GpuReadonly = 0,
  NoTracking = 0,
  GpuPrivate = 1,
  CpuInvisible = 1,
  CpuWriteCombined = 2,
  OwnedByCommandList = 3,
  GpuManaged = 4,
  Shared = 5,
};

typedef unsigned TextureViewKey;

struct TextureViewDescriptor {
  WMTPixelFormat format    : 24;
  WMTTextureType type      : 8;
  uint32_t firstMiplevel   : 4 = 0;
  uint32_t miplevelCount   : 4 = 1;
  uint32_t firstArraySlice : 12 = 0;
  uint32_t arraySize       : 12 = 1;
};

class Texture;
class TextureAllocation;

class TextureView {
public:
  virtual ~TextureView() {};

  void incRef();
  void decRef();

  WMT::Reference<WMT::Texture> texture;
  uint64_t gpuResourceID;
  DXMT_RESOURCE_RESIDENCY_STATE residency{};
  TextureAllocation *allocation; // `TextureAllocation` holds strong reference to `TextureView`
  TextureViewKey key;

  TextureView(const TextureView &) = delete;
  TextureView(TextureView &&) = delete;
  TextureView &operator=(const TextureView &) = delete;
  TextureView &operator=(TextureView &&) = delete;
  TextureView(TextureAllocation *allocation);
  TextureView(TextureAllocation *allocation, TextureViewKey key, TextureViewDescriptor descriptor);

private:
  std::atomic<uint32_t> refcount_ = {0u};
};

class TextureViewRef : public Rc<TextureView> {
public:
  using Rc<TextureView>::Rc;

  WMT::Texture
  texture() const {
    if (!*this)
      return {};
    return ptr()->texture;
  }

  TextureViewRef &
  operator=(TextureView &ref) {
    return (*this = &ref);
  }
};

class TextureAllocation : public Allocation {
  friend class Texture;

  /**
   * notes on thread-safefy:
   * all states in `TextureAllocation` is either immutable or only accessed by `dxmt-encode-thread`
   */

public:

  WMT::Texture texture() const {
    return obj_;
  }

  Flags<TextureAllocationFlag>
  flags() const {
    return flags_;
  }

  Texture *descriptor;
  void *mappedMemory;
  uint64_t gpuResourceID;
  mach_port_t machPort;
  GenericAccessTracker fenceTracker;

private:
  TextureAllocation(
      Texture *descriptor, WMT::Reference<WMT::Buffer> &&buffer, void *mapped_buffer, const WMTTextureInfo &info,
      unsigned bytes_per_row, Flags<TextureAllocationFlag> flags
  );
  TextureAllocation(
      Texture *descriptor, WMT::Reference<WMT::Texture> &&texture, const WMTTextureInfo &textureDescriptor,
      Flags<TextureAllocationFlag> flags
  );
  ~TextureAllocation();

  TextureAllocation(const TextureAllocation &) = delete;
  TextureAllocation(TextureAllocation &&) = delete;

  WMT::Reference<WMT::Texture> obj_;
  WMT::Reference<WMT::Buffer> buffer_;
  uint32_t version_ = 0;
  Flags<TextureAllocationFlag> flags_;
  std::vector<TextureViewRef> cached_view_;
};

class Texture {

public:
  void incRef();
  void decRef();

  TextureViewKey createView(TextureViewDescriptor const &descriptor);

  constexpr TextureAllocation *
  current() {
    return current_.ptr();
  }

  WMTTextureType
  textureType() const {
    return info_.type;
  }

  WMTPixelFormat
  pixelFormat() const {
    return info_.pixel_format;
  }

  WMTTextureType
  textureType(TextureViewKey view) {
    std::shared_lock<dxmt::shared_mutex> lock(mutex_);
    return viewDescriptors_[view].type;
  }

  WMTPixelFormat
  pixelFormat(TextureViewKey view) {
    std::shared_lock<dxmt::shared_mutex> lock(mutex_);
    return viewDescriptors_[view].format;
  }

  WMTTextureUsage
  usage() const {
    return info_.usage;
  }

  unsigned
  sampleCount() const {
    return info_.sample_count;
  }

  unsigned
  width() const {
    return info_.width;
  }

  unsigned
  height() const {
    return info_.height;
  }

  unsigned
  depth() const {
    return info_.depth;
  }

  unsigned
  width(TextureViewKey view) {
    std::shared_lock<dxmt::shared_mutex> lock(mutex_);
    return std::max(info_.width >> viewDescriptors_[view].firstMiplevel, 1u);
  }

  unsigned
  height(TextureViewKey view) {
    std::shared_lock<dxmt::shared_mutex> lock(mutex_);
    return std::max(info_.height >> viewDescriptors_[view].firstMiplevel, 1u);
  }

  /**
  \warning for cube texture, arrayLength() returns 1, while arrayLength(0) returns 6"
  */
  unsigned
  arrayLength() const {
    return info_.array_length;
  }

  unsigned
  arrayLength(TextureViewKey view) {
    std::shared_lock<dxmt::shared_mutex> lock(mutex_);
    return viewDescriptors_[view].arraySize;
  }

  Rc<TextureAllocation> allocate(Flags<TextureAllocationFlag> flags);
  Rc<TextureAllocation> import(mach_port_t mach_port);

  TextureView &view(TextureViewKey key);
  TextureView &view(TextureViewKey key, TextureAllocation *allocation);

  TextureViewKey checkViewUseArray(TextureViewKey key, bool isArray);
  TextureViewKey checkViewUseFormat(TextureViewKey key, WMTPixelFormat format);

  Rc<TextureAllocation> rename(Rc<TextureAllocation> &&newAllocation);

  Texture(const WMTTextureInfo &info, WMT::Device device);

  Texture(unsigned bytes_per_image, unsigned bytes_per_row, const WMTTextureInfo &info, WMT::Device device);

private:
  void prepareAllocationViews(TextureAllocation* allocation);

  WMTTextureInfo info_;
  unsigned bytes_per_image_ = 0;
  unsigned bytes_per_row_ = 0;

  Rc<TextureAllocation> current_;
  uint32_t version_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};

  std::vector<TextureViewDescriptor> viewDescriptors_;
  dxmt::shared_mutex mutex_;
  WMT::Device device_;
};

class RenamableTexturePool {
public:
  void incRef();
  void decRef();
  Rc<TextureAllocation> getNext(uint64_t frame_);

  RenamableTexturePool(Texture *texture, size_t capacity,Flags<TextureAllocationFlag> allocation_flags);

private:
  Rc<Texture> texture_;
  std::vector<Rc<TextureAllocation>> allocations_;
  uint64_t last_frame_ = 0;
  Flags<TextureAllocationFlag> allocation_flags_;
  unsigned capacity_;
  unsigned current_index_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};
};

} // namespace dxmt
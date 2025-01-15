#pragma once

#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "dxmt_deptrack.hpp"
#include "dxmt_residency.hpp"
#include "objc_pointer.hpp"
#include "rc/util_rc_ptr.hpp"
#include "thread.hpp"
#include "util_flags.hpp"

namespace dxmt {

enum class BufferAllocationFlag : uint32_t {
  GpuReadonly = 0,
  NoTracking = 0,
  GpuPrivate = 1,
  CpuInvisible = 1,
  CpuWriteCombined = 2,
  OwnedByCommandList = 3,
  GpuManaged = 4,
};

typedef unsigned BufferViewKey;

struct BufferViewDescriptor {
  MTL::PixelFormat format;
};

class Buffer;

struct BufferView {
  Obj<MTL::Texture> texture;
  DXMT_RESOURCE_RESIDENCY_STATE residency {};

  BufferView(Obj<MTL::Texture> texture):texture(std::move(texture)) {}  
};

class BufferAllocation {
  friend class Buffer;

public:
  void incRef();
  void decRef();

  MTL::Buffer *
  buffer() {
    return obj_.ptr();
  }

  Flags<BufferAllocationFlag>
  flags() const {
    return flags_;
  }

  void* mappedMemory;
  uint64_t gpuAddress;
  DXMT_RESOURCE_RESIDENCY_STATE residencyState;
  EncoderDepKey depkey;

private:
  BufferAllocation(Obj<MTL::Buffer> &&buffer, Flags<BufferAllocationFlag> flags);

  Obj<MTL::Buffer> obj_;
  uint32_t version_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};
  Flags<BufferAllocationFlag> flags_;
  std::vector<std::unique_ptr<BufferView>> cached_view_;
};

class Buffer {
public:
  void incRef();
  void decRef();

  BufferViewKey createView(BufferViewDescriptor const &);

  constexpr BufferAllocation *
  current() {
    return current_.ptr();
  }

  Rc<BufferAllocation> allocate(Flags<BufferAllocationFlag> flags);

  Rc<BufferAllocation> rename(Rc<BufferAllocation> &&newAllocation);

  Buffer(uint64_t length, MTL::Device *device) : length_(length), device_(device) {}

  MTL::Texture *view(BufferViewKey key);
  MTL::Texture *view(BufferViewKey key, BufferAllocation* allocation);

  DXMT_RESOURCE_RESIDENCY_STATE &residency(BufferViewKey key);
  DXMT_RESOURCE_RESIDENCY_STATE &residency(BufferViewKey key, BufferAllocation *allocation);

  constexpr uint64_t
  length() {
    return length_;
  };

private:
  void prepareAllocationViews(BufferAllocation* allocation);

  uint64_t length_;

  Rc<BufferAllocation> current_;
  uint32_t version_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};

  std::vector<BufferViewDescriptor> viewDescriptors_;
  dxmt::mutex mutex_;
  MTL::Device *device_;
};

struct BufferSlice {
  uint32_t byteOffset = 0;
  uint32_t byteLength = 0;
  uint32_t firstElement = 0;
  uint32_t elementCount = 0;
};

} // namespace dxmt
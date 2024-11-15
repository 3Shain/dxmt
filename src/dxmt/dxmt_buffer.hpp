#pragma once

#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
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
  MTL::Texture *texture;
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

  // TODO: tracking state
  // TODO: residency state

  Flags<BufferAllocationFlag>
  flags() {
    return flags_;
  }

  void* mappedMemory;
  uint64_t gpuAddress;

private:
  BufferAllocation(Obj<MTL::Buffer> &&buffer, Flags<BufferAllocationFlag> flags);

  Obj<MTL::Buffer> obj_;
  uint32_t version_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};
  Flags<BufferAllocationFlag> flags_;
  std::vector<Obj<MTL::Texture>> cached_view_;
};

class Buffer {
public:
  void incRef();
  void decRef();

  BufferViewKey createView(BufferViewDescriptor const &);

  BufferAllocation *
  current() {
    return current_.ptr();
  }

  Rc<BufferAllocation> allocate(Flags<BufferAllocationFlag> flags);

  Rc<BufferAllocation> rename(Rc<BufferAllocation> &&newAllocation);

  Buffer(uint64_t length, MTL::Device *device) : length_(length), device_(device) {}

  MTL::Texture *view(BufferViewKey key);

private:
  void prepareAllocationViews();

  uint64_t length_;

  Rc<BufferAllocation> current_;
  uint32_t version_ = 0;

  std::vector<BufferViewDescriptor> viewDescriptors_;
  dxmt::mutex mutex_;
  MTL::Device *device_;
};

class BufferSlice {
public:
};

} // namespace dxmt
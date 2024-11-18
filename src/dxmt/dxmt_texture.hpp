#pragma once
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "objc_pointer.hpp"
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
};

typedef unsigned TextureViewKey;

struct TextureViewDescriptor {
  MTL::PixelFormat format;
  MTL::TextureType type;
  unsigned firstMiplevel = 0;
  unsigned miplevelCount = 1;
  unsigned firstArraySlice = 0;
  unsigned arraySize = 1;
};

struct TextureView {
  MTL::Texture *texture;
  // TextureViewSubrange range;
};

class TextureAllocation {
  friend class Texture;

public:
  void incRef();
  void decRef();

  MTL::Texture *texture() {
    return obj_.ptr();
  }

  Flags<TextureAllocationFlag>
  flags() {
    return flags_;
  }

  void *mappedMemory;
  uint64_t gpuResourceID;

private:
  TextureAllocation(
      Obj<MTL::Buffer> &&buffer, Obj<MTL::TextureDescriptor> &&textureDescriptor, unsigned bytes_per_row,Flags<TextureAllocationFlag> flags
  );
  TextureAllocation(Obj<MTL::Texture> &&texture, Flags<TextureAllocationFlag> flags);
  ~TextureAllocation();

  Obj<MTL::Texture> obj_;
  uint32_t version_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};
  Flags<TextureAllocationFlag> flags_;
  std::vector<Obj<MTL::Texture>> cached_view_;
};

class Texture {
public:
  void incRef();
  void decRef();

  TextureViewKey createView(TextureViewDescriptor const &descriptor);

  TextureAllocation *
  current() {
    return current_.ptr();
  }

  Rc<TextureAllocation> allocate(Flags<TextureAllocationFlag> flags);

  MTL::Texture *view(TextureViewKey key);

  Rc<TextureAllocation> rename(Rc<TextureAllocation> &&newAllocation);

  Texture(Obj<MTL::TextureDescriptor> &&descriptor, MTL::Device *device);

  Texture(
      unsigned bytes_per_image, unsigned bytes_per_row, Obj<MTL::TextureDescriptor> &&descriptor, MTL::Device *device
  );

private:
  void prepareAllocationViews();

  Obj<MTL::TextureDescriptor> descriptor_;
  unsigned bytes_per_image_ = 0;
  unsigned bytes_per_row_ = 0;

  Rc<TextureAllocation> current_;
  uint32_t version_ = 0;

  std::vector<TextureViewDescriptor> viewDescriptors_;
  dxmt::mutex mutex_;
  MTL::Device *device_;
};

} // namespace dxmt
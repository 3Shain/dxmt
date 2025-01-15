#pragma once
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "dxmt_deptrack.hpp"
#include "dxmt_residency.hpp"
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
  MTL::PixelFormat format: 32;
  MTL::TextureType type : 32;
  MTL::TextureUsage usage: 32;
  unsigned firstMiplevel = 0;
  unsigned miplevelCount = 1;
  unsigned firstArraySlice = 0;
  unsigned arraySize = 1;
};

struct TextureView {
  Obj<MTL::Texture> texture;
  DXMT_RESOURCE_RESIDENCY_STATE residency {};

  TextureView(Obj<MTL::Texture> texture):texture(std::move(texture)) {}  
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
  flags() const {
    return flags_;
  }

  void *mappedMemory;
  uint64_t gpuResourceID;
  DXMT_RESOURCE_RESIDENCY_STATE residencyState;
  EncoderDepKey depkey;

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
  std::vector<std::unique_ptr<TextureView>> cached_view_;
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

  MTL::TextureType textureType() {
    return descriptor_->textureType();
  }

  MTL::PixelFormat pixelFormat() {
    return descriptor_->pixelFormat();
  }

  unsigned sampleCount() {
    return descriptor_->sampleCount();
  }

  Rc<TextureAllocation> allocate(Flags<TextureAllocationFlag> flags);

  MTL::Texture *view(TextureViewKey key);
  MTL::Texture *view(TextureViewKey key, TextureAllocation* allocation);


  DXMT_RESOURCE_RESIDENCY_STATE &residency(TextureViewKey key);
  DXMT_RESOURCE_RESIDENCY_STATE &residency(TextureViewKey key, TextureAllocation* allocation);

  Rc<TextureAllocation> rename(Rc<TextureAllocation> &&newAllocation);

  Texture(Obj<MTL::TextureDescriptor> &&descriptor, MTL::Device *device);

  Texture(
      unsigned bytes_per_image, unsigned bytes_per_row, Obj<MTL::TextureDescriptor> &&descriptor, MTL::Device *device
  );

private:
  void prepareAllocationViews(TextureAllocation* allocation);

  Obj<MTL::TextureDescriptor> descriptor_;
  unsigned bytes_per_image_ = 0;
  unsigned bytes_per_row_ = 0;

  Rc<TextureAllocation> current_;
  uint32_t version_ = 0;
  std::atomic<uint32_t> refcount_ = {0u};

  std::vector<TextureViewDescriptor> viewDescriptors_;
  dxmt::mutex mutex_;
  MTL::Device *device_;
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
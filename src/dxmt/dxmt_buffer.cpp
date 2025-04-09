#include "dxmt_buffer.hpp"
#include "dxmt_format.hpp"
#include "thread.hpp"
#include "util_likely.hpp"
#include <cassert>
#include <mutex>

namespace dxmt {

std::atomic_uint64_t global_buffer_seq = {0};

BufferAllocation::BufferAllocation(WMT::Device device, const WMTBufferInfo &info, Flags<BufferAllocationFlag> flags) :
    info_(info),
    flags_(flags) {
  obj_ = device.newBuffer(&info_);
  gpuAddress = info_.gpu_address;
  mappedMemory = info_.memory.get();
  depkey = EncoderDepSet::generateNewKey(global_buffer_seq.fetch_add(1));
};

void
BufferAllocation::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
BufferAllocation::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

MTL::Texture *
Buffer::view(BufferViewKey key) {
  return view(key, current_.ptr());
};

MTL::Texture *
Buffer::view(BufferViewKey key, BufferAllocation* allocation) {
  if (unlikely(allocation->version_ != version_)) {
    prepareAllocationViews(allocation);
  }
  return (MTL::Texture *)allocation->cached_view_[key]->texture.handle;
};

DXMT_RESOURCE_RESIDENCY_STATE &
Buffer::residency(BufferViewKey key) {
  return residency(key, current_.ptr());
}

DXMT_RESOURCE_RESIDENCY_STATE &
Buffer::residency(BufferViewKey key, BufferAllocation *allocation) {
  if (unlikely(allocation->version_ != version_)) {
    prepareAllocationViews(allocation);
  }
  return allocation->cached_view_[key]->residency;
}

void
Buffer::prepareAllocationViews(BufferAllocation* allocation) {
  std::unique_lock<dxmt::mutex> lock(mutex_);
  for (unsigned version = allocation->version_; version < version_; version++) {
    auto format = viewDescriptors_[version].format;
    auto texel_size = MTLGetTexelSize(format);
    assert(texel_size);
    assert(!(length_ & (texel_size - 1)));
    WMTTextureInfo info;
    info.type = WMTTextureTypeTextureBuffer;
    info.width = length_ / (uint64_t)texel_size;
    info.height = 1;
    info.depth = 1;
    info.array_length = 1;
    info.mipmap_level_count = 1;
    info.sample_count = 1;
    info.pixel_format = format;
    info.options = allocation->info_.options;
    info.usage = WMTTextureUsageShaderRead; // FIXME

    allocation->cached_view_.push_back(std::make_unique<BufferView>(allocation->obj_.newTexture(&info, 0, length_)));
  }
  allocation->version_ = version_;
};

BufferViewKey
Buffer::createView(BufferViewDescriptor const &descriptor) {
  std::unique_lock<dxmt::mutex> lock(mutex_);
  unsigned i = 0;
  for (; i < version_; i++) {
    if (viewDescriptors_[i].format == descriptor.format) {
      return i;
    }
  }
  viewDescriptors_.push_back(descriptor);
  version_ = version_ + 1;
  return i;
}

Rc<BufferAllocation>
Buffer::allocate(Flags<BufferAllocationFlag> flags) {
  WMTResourceOptions options = WMTResourceStorageModeShared;
  if (flags.test(BufferAllocationFlag::GpuReadonly)) {
    options |= WMTResourceHazardTrackingModeUntracked;
  }
  if (flags.test(BufferAllocationFlag::CpuWriteCombined)) {
    options |= WMTResourceOptionCPUCacheModeWriteCombined;
  }
  if (flags.test(BufferAllocationFlag::CpuInvisible)) {
    options |= WMTResourceStorageModePrivate;
  }
  if (flags.test(BufferAllocationFlag::GpuManaged)) {
    options |= WMTResourceStorageModeManaged;
  }
  WMTBufferInfo info;
  info.memory.set(0);
  info.length = length_;
  info.options = options;
  return new BufferAllocation(device_, info, flags);
};

Rc<BufferAllocation>
Buffer::rename(Rc<BufferAllocation> &&newAllocation) {
  Rc<BufferAllocation> old = std::move(current_);
  current_ = std::move(newAllocation);
  return old;
}

void Buffer::incRef(){
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void Buffer::decRef(){
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

} // namespace dxmt
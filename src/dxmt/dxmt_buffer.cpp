#include "dxmt_buffer.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "dxmt_format.hpp"
#include "thread.hpp"
#include "util_likely.hpp"
#include <mutex>

namespace dxmt {

std::atomic_uint64_t global_buffer_seq = {0};

BufferAllocation::BufferAllocation(Obj<MTL::Buffer> &&buffer, Flags<BufferAllocationFlag> flags) :
    obj_(std::move(buffer)),
    flags_(flags) {
  if (flags_.test(BufferAllocationFlag::GpuPrivate))
    mappedMemory = nullptr;
  else
    mappedMemory = obj_->contents();
  gpuAddress = obj_->gpuAddress();
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
  return allocation->cached_view_[key]->texture.ptr();
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
    auto buffer = allocation->obj_.ptr();
    auto length = buffer->length();
    auto format = viewDescriptors_[version].format;
    auto texel_size = MTLGetTexelSize(format);
    assert(!(length & (texel_size - 1)));
    assert(texel_size);
    auto desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(MTL::TextureTypeTextureBuffer);
    desc->setWidth(length / texel_size);
    desc->setHeight(1);
    desc->setDepth(1);
    desc->setArrayLength(1);
    desc->setMipmapLevelCount(1);
    desc->setSampleCount(1);
    desc->setPixelFormat(format);
    desc->setResourceOptions(buffer->resourceOptions());

    allocation->cached_view_.push_back(std::make_unique<BufferView>(transfer(buffer->newTexture(desc, 0, length))));

    desc->release();
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
  MTL::ResourceOptions options = 0;
  if (flags.test(BufferAllocationFlag::GpuReadonly)) {
    options |= MTL::ResourceHazardTrackingModeUntracked;
  }
  if (flags.test(BufferAllocationFlag::CpuWriteCombined)) {
    options |= MTL::ResourceOptionCPUCacheModeWriteCombined;
  }
  if (flags.test(BufferAllocationFlag::CpuInvisible)) {
    options |= MTL::ResourceStorageModePrivate;
  }
  if (flags.test(BufferAllocationFlag::GpuManaged)) {
    options |= MTL::ResourceStorageModeManaged;
  }
  return new BufferAllocation(transfer(device_->newBuffer(length_, options)), flags);
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
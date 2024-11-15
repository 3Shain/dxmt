#include "dxmt_buffer.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "dxmt_format.hpp"
#include "thread.hpp"
#include "util_likely.hpp"
#include <mutex>

namespace dxmt {

BufferAllocation::BufferAllocation(Obj<MTL::Buffer> &&buffer, Flags<BufferAllocationFlag> flags) :
    obj_(std::move(buffer)),
    flags_(flags) {
  if (flags_.test(BufferAllocationFlag::GpuPrivate))
    mappedMemory = nullptr;
  else
    mappedMemory = obj_->contents();
  gpuAddress = obj_->gpuAddress();
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
  if (unlikely(current_->version_ != version_)) {
    prepareAllocationViews();
  }
  return current_->cached_view_[key];
};

void
Buffer::prepareAllocationViews() {
  std::unique_lock<dxmt::mutex> lock(mutex_);
  for (unsigned version = current_->version_; version < version_; version++) {
    auto buffer = current_->obj_.ptr();
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

    current_->cached_view_.push_back(transfer(buffer->newTexture(desc, 0, length)));

    desc->release();
  }
  current_->version_ = version_;
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

};

void Buffer::decRef(){

};

} // namespace dxmt
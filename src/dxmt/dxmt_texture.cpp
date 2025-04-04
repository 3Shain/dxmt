#include "dxmt_texture.hpp"
#include "Foundation/NSRange.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "Metal/MTLDevice.hpp"
#include "dxmt_residency.hpp"
#include "objc_pointer.hpp"
#include <atomic>

namespace dxmt {

std::atomic_uint64_t global_texture_seq = {0};

TextureAllocation::TextureAllocation(
    Obj<MTL::Buffer> &&buffer, Obj<MTL::TextureDescriptor> &&textureDescriptor, unsigned bytes_per_row,
    Flags<TextureAllocationFlag> flags
) :
    flags_(flags) {
  auto owned_buffer = buffer.takeOwnership();

  obj_ = transfer(owned_buffer->newTexture(textureDescriptor, 0, bytes_per_row));

  mappedMemory = owned_buffer->contents();
  gpuResourceID = obj_->gpuResourceID()._impl;
  depkey = EncoderDepSet::generateNewKey(global_texture_seq.fetch_add(1));
};

TextureAllocation::TextureAllocation(Obj<MTL::Texture> &&texture, Flags<TextureAllocationFlag> flags) :
    obj_(std::move(texture)),
    flags_(flags) {
  mappedMemory = nullptr;
  gpuResourceID = obj_->gpuResourceID()._impl;
  depkey = EncoderDepSet::generateNewKey(global_texture_seq.fetch_add(1));
};

TextureAllocation::~TextureAllocation() {
  if (obj_->buffer())
    obj_->buffer()->release();
};

void
TextureAllocation::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
TextureAllocation::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

void
Texture::prepareAllocationViews(TextureAllocation* allocaiton) {
  std::unique_lock<dxmt::mutex> lock(mutex_);
  if (allocaiton->version_ < 1) {
    allocaiton->cached_view_.push_back(std::make_unique<TextureView>(allocaiton->obj_.ptr()));
    allocaiton->version_ = 1;
  }
  for (unsigned version = allocaiton->version_; version < version_; version++) {
    auto texture = allocaiton->obj_.ptr();
    auto &view = viewDescriptors_[version];

    if ((view.usage & MTL::TextureUsageRenderTarget) == 0) {
      switch (view.format) {
      case MTL::PixelFormatDepth24Unorm_Stencil8:
      case MTL::PixelFormatDepth32Float_Stencil8:
      case MTL::PixelFormatDepth16Unorm:
      case MTL::PixelFormatDepth32Float:
      case MTL::PixelFormatStencil8:
        allocaiton->cached_view_.push_back(std::make_unique<TextureView>(transfer(texture->newTextureView(
            view.format, view.type, {view.firstMiplevel, view.miplevelCount}, {view.firstArraySlice, view.arraySize},
            {MTL::TextureSwizzleRed, MTL::TextureSwizzleZero, MTL::TextureSwizzleZero, MTL::TextureSwizzleOne}
        ))));
        continue;
      case MTL::PixelFormatX32_Stencil8:
      case MTL::PixelFormatX24_Stencil8:
        allocaiton->cached_view_.push_back(std::make_unique<TextureView>(transfer(texture->newTextureView(
            view.format, view.type, {view.firstMiplevel, view.miplevelCount}, {view.firstArraySlice, view.arraySize},
            {MTL::TextureSwizzleZero, MTL::TextureSwizzleRed, MTL::TextureSwizzleZero, MTL::TextureSwizzleOne}
        ))));
        continue;
      default:
        break;
      }
    }
    allocaiton->cached_view_.push_back(std::make_unique<TextureView>(transfer(texture->newTextureView(
        view.format, view.type, {view.firstMiplevel, view.miplevelCount}, {view.firstArraySlice, view.arraySize}
    ))));
  }
  allocaiton->version_ = version_;
}

TextureViewKey
Texture::createView(TextureViewDescriptor const &descriptor) {
  std::unique_lock<dxmt::mutex> lock(mutex_);
  unsigned i = 0;
  for (; i < version_; i++) {
    if (viewDescriptors_[i].format != descriptor.format)
      continue;
    if (viewDescriptors_[i].type != descriptor.type)
      continue;
    if ((viewDescriptors_[i].usage & descriptor.usage) != descriptor.usage)
      continue;
    if (viewDescriptors_[i].firstMiplevel != descriptor.firstMiplevel)
      continue;
    if (viewDescriptors_[i].miplevelCount != descriptor.miplevelCount)
      continue;
    if (viewDescriptors_[i].firstArraySlice != descriptor.firstArraySlice)
      continue;
    if (viewDescriptors_[i].arraySize != descriptor.arraySize)
      continue;
    return i;
  }
  viewDescriptors_.push_back(descriptor);
  version_ = version_ + 1;
  return i;
}

Texture::Texture(Obj<MTL::TextureDescriptor> &&descriptor, MTL::Device *device) :
    descriptor_(std::move(descriptor)),
    device_(device) {

  MTL::TextureUsage default_view_usage = descriptor_->usage();
  switch (descriptor_->pixelFormat()) {
  case MTL::PixelFormatDepth24Unorm_Stencil8:
  case MTL::PixelFormatDepth32Float_Stencil8:
  case MTL::PixelFormatDepth16Unorm:
  case MTL::PixelFormatDepth32Float:
  case MTL::PixelFormatStencil8:
  case MTL::PixelFormatX32_Stencil8:
  case MTL::PixelFormatX24_Stencil8:
    /**
    we need to remove read/write usage for default depth stencil view
    because the swizzle will be reconfigured to match the D3D11 spec
     */
    default_view_usage = default_view_usage & ~(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
    break;
  default:
    break;
  }

  uint32_t arraySize = (uint32_t)descriptor_->arrayLength();
  switch (descriptor_->textureType()) {
  case MTL::TextureTypeCubeArray:
  case MTL::TextureTypeCube:
    arraySize = arraySize * 6;
    break;
  default:
    break;
  }

  viewDescriptors_.push_back({
      .format = descriptor_->pixelFormat(),
      .type = descriptor_->textureType(),
      .usage = default_view_usage,
      .firstMiplevel = 0,
      .miplevelCount = (uint32_t)descriptor_->mipmapLevelCount(),
      .firstArraySlice = 0,
      .arraySize = arraySize,
  });
  version_ = 1;
}

Texture::Texture(
    unsigned bytes_per_image, unsigned bytes_per_row, Obj<MTL::TextureDescriptor> &&descriptor, MTL::Device *device
) :
    descriptor_(std::move(descriptor)),
    bytes_per_image_(bytes_per_image),
    bytes_per_row_(bytes_per_row),
    device_(device) {

  assert(descriptor_->textureType() == MTL::TextureType2D);
  assert(descriptor_->mipmapLevelCount() == 1);
  assert(descriptor_->arrayLength() == 1);

  viewDescriptors_.push_back({
      .format = descriptor_->pixelFormat(),
      .type = descriptor_->textureType(),
      .usage = MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite,
      .firstMiplevel = 0,
      .miplevelCount = 1,
      .firstArraySlice = 0,
      .arraySize = 1,
  });
  version_ = 1;
}

Rc<TextureAllocation>
Texture::allocate(Flags<TextureAllocationFlag> flags) {
  MTL::ResourceOptions options = 0;
  auto descriptor = transfer(descriptor_->copy());
  if (flags.test(TextureAllocationFlag::GpuReadonly)) {
    options |= MTL::ResourceHazardTrackingModeUntracked;
  }
  if (flags.test(TextureAllocationFlag::CpuWriteCombined)) {
    options |= MTL::ResourceOptionCPUCacheModeWriteCombined;
  }
  if (flags.test(TextureAllocationFlag::CpuInvisible)) {
    options |= MTL::ResourceStorageModePrivate;
  }
  if (flags.test(TextureAllocationFlag::GpuManaged)) {
    options |= MTL::ResourceStorageModeManaged;
  }
  descriptor->setResourceOptions(options);
  if (bytes_per_image_) {
    auto buffer = transfer(device_->newBuffer(bytes_per_image_, options));
    return new TextureAllocation(std::move(buffer), std::move(descriptor), bytes_per_row_, flags);
  }
  auto texture = transfer(device_->newTexture(descriptor));
  return new TextureAllocation(std::move(texture), flags);
}


MTL::Texture *
Texture::view(TextureViewKey key) {
  return view(key, current_.ptr());
}

MTL::Texture *
Texture::view(TextureViewKey key, TextureAllocation* allocation) {
  if (unlikely(allocation->version_ != version_)) {
    prepareAllocationViews(allocation);
  }
  return allocation->cached_view_[key]->texture.ptr();
}


TextureViewKey Texture::checkViewUseArray(TextureViewKey key, bool isArray) {
  auto &view = viewDescriptors_[key];
  static constexpr uint32_t ARRAY_TYPE_MASK = 0b0101001010;
  if (unlikely(bool((1 << uint32_t(view.type)) & ARRAY_TYPE_MASK) != isArray)) {
    // TODO: this process can be cached
    auto new_view_desc = view;
    switch (view.type) {
    case MTL::TextureType1D:
      new_view_desc.type = MTL::TextureType1DArray;
      new_view_desc.arraySize = 1;
      break;
    case MTL::TextureType1DArray:
      new_view_desc.type = MTL::TextureType1D;
      new_view_desc.arraySize = 1;
      break;
    case MTL::TextureType2D:
      new_view_desc.type = MTL::TextureType2DArray;
      new_view_desc.arraySize = 1;
      break;
    case MTL::TextureType2DArray:
      new_view_desc.type = MTL::TextureType2D;
      new_view_desc.arraySize = 1;
      break;
    case MTL::TextureType2DMultisample:
      new_view_desc.type = MTL::TextureType2DMultisampleArray;
      new_view_desc.arraySize = 1;
      break;
    case MTL::TextureType2DMultisampleArray:
      new_view_desc.type = MTL::TextureType2DMultisample;
      new_view_desc.arraySize = 1;
      break;
    case MTL::TextureTypeCube:
      new_view_desc.type = MTL::TextureTypeCubeArray;
      new_view_desc.arraySize = 6;
      break;
    case MTL::TextureTypeCubeArray:
      new_view_desc.type = MTL::TextureTypeCube;
      new_view_desc.arraySize = 6;
      break;
    default:
      return key; // should be unreachable
    }
    return createView(new_view_desc);
  }
  return key;
}

TextureViewKey Texture::checkViewUseFormat(TextureViewKey key, MTL::PixelFormat format) {
  auto &view = viewDescriptors_[key];
  if (unlikely(view.format != format)) {
    auto new_view_desc = view;
    new_view_desc.format = format;
    return createView(new_view_desc);
  }
  return key;
}

DXMT_RESOURCE_RESIDENCY_STATE &
Texture::residency(TextureViewKey key) {
  return residency(key, current_.ptr());
}

DXMT_RESOURCE_RESIDENCY_STATE &
Texture::residency(TextureViewKey key, TextureAllocation *allocation) {
  if (unlikely(allocation->version_ != version_)) {
    prepareAllocationViews(allocation);
  }
  return allocation->cached_view_[key]->residency;
}

Rc<TextureAllocation>
Texture::rename(Rc<TextureAllocation> &&newAllocation) {
  Rc<TextureAllocation> old = std::move(current_);
  current_ = std::move(newAllocation);
  return old;
}

void Texture::incRef(){
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void Texture::decRef(){
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

RenamableTexturePool::RenamableTexturePool(
    Texture *texture, size_t capacity, Flags<TextureAllocationFlag> allocation_flags
) :
    texture_(texture),
    allocations_(capacity, nullptr),
    allocation_flags_(allocation_flags),
    capacity_(capacity) {}

void
RenamableTexturePool::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
RenamableTexturePool::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

Rc<TextureAllocation>
RenamableTexturePool::getNext(uint64_t frame) {
  if (frame > last_frame_) {
    last_frame_ = frame;
    current_index_ = 0;
  }
  auto current_index = current_index_++ % capacity_;
  if (!allocations_[current_index].ptr())
    allocations_[current_index] = texture_->allocate(allocation_flags_);
  return allocations_[current_index];
}

} // namespace dxmt
/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dxmt_texture.hpp"
#include "dxmt_residency.hpp"
#include "wsi_platform.hpp"
#include <atomic>
#include <cassert>

namespace dxmt {

std::atomic_uint64_t global_texture_seq = {0};

void
TextureView::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
TextureView::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

TextureView::TextureView(TextureAllocation *allocation) :
    texture(allocation->texture()),
    gpuResourceID(allocation->gpuResourceID),
    allocation(allocation),
    key(allocation->descriptor->fullView) {}

TextureView::TextureView(TextureAllocation *allocation, unsigned index, TextureViewDescriptor descriptor) :
    gpuResourceID(0),
    allocation(allocation),
    key(descriptor, index, allocation->descriptor->miplevelCount()) {
  auto parent = allocation->texture();
  texture = parent.newTextureView(
      descriptor.format, descriptor.type, descriptor.firstMiplevel, descriptor.miplevelCount,
      descriptor.firstArraySlice, descriptor.arraySize,
      {WMTTextureSwizzleRed, WMTTextureSwizzleGreen, WMTTextureSwizzleBlue, WMTTextureSwizzleAlpha}, gpuResourceID
  );
}

TextureAllocation::TextureAllocation(
    Texture *descriptor, WMT::Reference<WMT::Buffer> &&buffer, void *mapped_buffer, const WMTTextureInfo &info,
    unsigned bytes_per_row, Flags<TextureAllocationFlag> flags
) :
    descriptor(descriptor),
    mappedMemory(mapped_buffer),
    buffer_(std::move(buffer)),
    flags_(flags) {
  auto info_copy = info;
  obj_ = buffer_.newTexture(info_copy, 0, bytes_per_row);

  gpuResourceID = info_copy.gpu_resource_id;
  machPort = 0;
  fenceTrackers.resize(
      flags.test(TextureAllocationFlag::ShaderReadonly) ? 1 : descriptor->arrayLength() * descriptor->miplevelCount()
  );
};

TextureAllocation::TextureAllocation(
    Texture *descriptor, WMT::Reference<WMT::Texture> &&texture, const WMTTextureInfo &textureDescriptor,
    Flags<TextureAllocationFlag> flags
) :
    descriptor(descriptor),
    obj_(std::move(texture)),
    flags_(flags) {
  mappedMemory = nullptr;
  gpuResourceID = textureDescriptor.gpu_resource_id;
  machPort = textureDescriptor.mach_port;
  fenceTrackers.resize(
      flags.test(TextureAllocationFlag::ShaderReadonly) ? 1 : descriptor->arrayLength() * descriptor->miplevelCount()
  );
};

void
TextureAllocation::free(){
#ifdef __i386__
  wsi::aligned_free(mappedMemory);
#endif
  delete this;
};

void
Texture::prepareAllocationViews(TextureAllocation *allocation) {
  if (allocation->version_ < 1) {
    allocation->cached_view_.push_back(new TextureView(allocation));
    allocation->version_ = 1;
  }
  std::shared_lock<dxmt::shared_mutex> lock(mutex_);
  for (unsigned version = allocation->version_; version < version_; version++) {
    allocation->cached_view_.push_back(new TextureView(allocation, version, viewDescriptors_[version]));
  }
  allocation->version_ = version_;
}

TextureViewKey
Texture::createView(TextureViewDescriptor const &descriptor) {
  std::unique_lock<dxmt::shared_mutex> lock(mutex_);
  unsigned i = 0;
  for (; i < version_; i++) {
    if (viewDescriptors_[i].format != descriptor.format)
      continue;
    if (viewDescriptors_[i].type != descriptor.type)
      continue;
    if (viewDescriptors_[i].firstMiplevel != descriptor.firstMiplevel)
      continue;
    if (viewDescriptors_[i].miplevelCount != descriptor.miplevelCount)
      continue;
    if (viewDescriptors_[i].firstArraySlice != descriptor.firstArraySlice)
      continue;
    if (viewDescriptors_[i].arraySize != descriptor.arraySize)
      continue;
    return TextureViewKey(descriptor, i, info_.mipmap_level_count);
  }
  viewDescriptors_.push_back(descriptor);
  version_ = version_ + 1;
  return TextureViewKey(descriptor, i, info_.mipmap_level_count);
}

Texture::Texture(const WMTTextureInfo &descriptor, WMT::Device device) :
    info_(descriptor),
    device_(device) {

  viewDescriptors_.push_back({
      .format = info_.pixel_format,
      .type = info_.type,
      .firstMiplevel = 0,
      .miplevelCount = info_.mipmap_level_count,
      .firstArraySlice = 0,
      .arraySize = arrayLength(),
  });
  version_ = 1;
  fullView = TextureViewKey(viewDescriptors_[0], 0, info_.mipmap_level_count);
}

Texture::Texture(
    unsigned bytes_per_image, unsigned bytes_per_row, const WMTTextureInfo &descriptor, WMT::Device device
) :
    info_(descriptor),
    bytes_per_image_(bytes_per_image),
    bytes_per_row_(bytes_per_row),
    device_(device) {

  assert(info_.type == WMTTextureType2D);
  assert(info_.mipmap_level_count == 1);
  assert(info_.array_length == 1);

  viewDescriptors_.push_back({
      .format = info_.pixel_format,
      .type = info_.type,
      .firstMiplevel = 0,
      .miplevelCount = 1,
      .firstArraySlice = 0,
      .arraySize = 1,
  });
  version_ = 1;
  fullView = TextureViewKey(viewDescriptors_[0], 0, info_.mipmap_level_count);
}

Rc<TextureAllocation>
Texture::allocate(Flags<TextureAllocationFlag> flags) {
  WMTResourceOptions options = WMTResourceHazardTrackingModeUntracked;
  WMTTextureInfo info = info_; // copy
  info.mach_port = 0;
  if (flags.test(TextureAllocationFlag::CpuWriteCombined)) {
    options |= WMTResourceOptionCPUCacheModeWriteCombined;
  }
  if (flags.test(TextureAllocationFlag::CpuInvisible)) {
    options |= WMTResourceStorageModePrivate;
  }
  if (flags.test(TextureAllocationFlag::GpuManaged)) {
    options |= WMTResourceStorageModeManaged;
  }
  info.options = options;
  if (bytes_per_image_) {
    WMTBufferInfo buffer_info;
    buffer_info.length = bytes_per_image_;
    buffer_info.options = options;
    buffer_info.memory.set(nullptr);
#ifdef __i386__
    buffer_info.memory.set(wsi::aligned_malloc(bytes_per_image_, DXMT_PAGE_SIZE));
#endif
    auto buffer = device_.newBuffer(buffer_info);
    return new TextureAllocation(this, std::move(buffer), buffer_info.memory.get(), info, bytes_per_row_, flags);
  }
  auto texture = flags.test(TextureAllocationFlag::Shared) ? device_.newSharedTexture(info) : device_.newTexture(info);
  return new TextureAllocation(this, std::move(texture), info, flags);
}

Rc<TextureAllocation>
Texture::import(mach_port_t mach_port) {
  Flags<TextureAllocationFlag> flags;
  WMTTextureInfo info;
  info.mach_port = mach_port;
  auto texture = device_.newSharedTexture(info);
  // now allocation's info is populated
  // and we may check if it is consistent with texture's info (it should be)
  if (texture) {
    // doing some unnecessary checks for the sake of completeness
    if (info.options & WMTResourceStorageModeManaged) // should be always false
      flags.set(TextureAllocationFlag::GpuManaged);
    if (info.options & WMTResourceStorageModePrivate) // should be always true
      flags.set(TextureAllocationFlag::GpuPrivate);
    if (info.options & WMTResourceHazardTrackingModeUntracked)
      flags.set(TextureAllocationFlag::NoTracking);
    if ((info.usage & (WMTTextureUsageShaderWrite | WMTTextureUsageRenderTarget)) == 0)
      flags.set(TextureAllocationFlag::ShaderReadonly);
    flags.set(TextureAllocationFlag::Shared);
    return new TextureAllocation(this, std::move(texture), info, flags);
  }
  assert(texture && "failed to import shared texture");
  return nullptr;
}

TextureView &
Texture::view(TextureViewKey key) {
  return view(key, current_.ptr());
}

TextureView &
Texture::view(TextureViewKey key, TextureAllocation* allocation) {
  if (unlikely(allocation->version_ != version_)) {
    prepareAllocationViews(allocation);
  }
  return *allocation->cached_view_[key.index];
}

TextureViewKey Texture::checkViewUseArray(TextureViewKey key, bool isArray) {
  std::shared_lock<dxmt::shared_mutex> shared_lock(mutex_);
  auto view = viewDescriptors_[key.index];
  shared_lock = {};
  static constexpr uint32_t ARRAY_TYPE_MASK = 0b0101001010;
  if (unlikely(bool((1 << uint32_t(view.type)) & ARRAY_TYPE_MASK) != isArray)) {
    // TODO: this process can be cached
    auto new_view_desc = view;
    switch (view.type) {
    case WMTTextureType1D:
      new_view_desc.type = WMTTextureType1DArray;
      new_view_desc.arraySize = 1;
      break;
    case WMTTextureType1DArray:
      new_view_desc.type = WMTTextureType1D;
      new_view_desc.arraySize = 1;
      break;
    case WMTTextureType2D:
      new_view_desc.type = WMTTextureType2DArray;
      new_view_desc.arraySize = 1;
      break;
    case WMTTextureType2DArray:
      new_view_desc.type = WMTTextureType2D;
      new_view_desc.arraySize = 1;
      break;
    case WMTTextureType2DMultisample:
      new_view_desc.type = WMTTextureType2DMultisampleArray;
      new_view_desc.arraySize = 1;
      break;
    case WMTTextureType2DMultisampleArray:
      new_view_desc.type = WMTTextureType2DMultisample;
      new_view_desc.arraySize = 1;
      break;
    case WMTTextureTypeCube:
      new_view_desc.type = WMTTextureTypeCubeArray;
      new_view_desc.arraySize = 6;
      break;
    case WMTTextureTypeCubeArray:
      new_view_desc.type = WMTTextureTypeCube;
      new_view_desc.arraySize = 6;
      break;
    default:
      return key; // should be unreachable
    }
    return createView(new_view_desc);
  }
  return key;
}

TextureViewKey Texture::checkViewUseFormat(TextureViewKey key, WMTPixelFormat format) {
  std::shared_lock<dxmt::shared_mutex> shared_lock(mutex_);
  auto view = viewDescriptors_[key.index];
  shared_lock = {};
  if (unlikely(view.format != format)) {
    auto new_view_desc = view;
    new_view_desc.format = format;
    return createView(new_view_desc);
  }
  return key;
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
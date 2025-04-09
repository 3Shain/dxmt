#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#define WINEMETAL_API
#include "../winemetal_thunks.h"

typedef int NTSTATUS;
#define STATUS_SUCCESS 0

static NTSTATUS
_NSObject_retain(NSObject **obj) {
  [*obj retain];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSObject_release(NSObject **obj) {
  [*obj release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSArray_object(void *obj) {
  struct unixcall_generic_obj_uint64_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(NSArray *)params->handle objectAtIndex:params->arg];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSArray_count(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(NSArray *)params->handle count];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCopyAllDevices(void *obj) {
  struct unixcall_generic_obj_ret *params = obj;
  params->ret = (obj_handle_t)MTLCopyAllDevices();
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_recommendedMaxWorkingSetSize(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLDevice>)params->handle recommendedMaxWorkingSetSize];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_currentAllocatedSize(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLDevice>)params->handle currentAllocatedSize];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_name(void *obj) {
  struct unixcall_generic_obj_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(id<MTLDevice>)params->handle name];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSString_getCString(void *obj) {
  struct unixcall_nsstring_getcstring *params = obj;
  params->ret = (uint32_t)[(NSString *)params->str getCString:(char *)params->buffer_ptr
                                                    maxLength:params->max_length
                                                     encoding:params->encoding];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_newCommandQueue(void *obj) {
  struct unixcall_generic_obj_uint64_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(id<MTLDevice>)params->handle newCommandQueueWithMaxCommandBufferCount:params->arg];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSAutoreleasePool_alloc_init(void *obj) {
  struct unixcall_generic_obj_ret *params = obj;
  params->ret = (obj_handle_t)[[NSAutoreleasePool alloc] init];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandQueue_commandBuffer(void *obj) {
  struct unixcall_generic_obj_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(id<MTLCommandQueue>)params->handle commandBuffer];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandBuffer_commit(void *obj) {
  struct unixcall_generic_obj_noret *params = obj;
  [(id<MTLCommandBuffer>)params->handle commit];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandBuffer_waitUntilCompleted(void *obj) {
  struct unixcall_generic_obj_noret *params = obj;
  [(id<MTLCommandBuffer>)params->handle waitUntilCompleted];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandBuffer_status(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLCommandBuffer>)params->handle status];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_newSharedEvent(void *obj) {
  struct unixcall_generic_obj_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(id<MTLDevice>)params->handle newSharedEvent];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLSharedEvent_signaledValue(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLSharedEvent>)params->handle signaledValue];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandBuffer_encodeSignalEvent(void *obj) {
  struct unixcall_generic_obj_obj_uint64_noret *params = obj;
  [(id<MTLCommandBuffer>)params->handle encodeSignalEvent:(id<MTLSharedEvent>)params->arg0 value:params->arg1];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_newBuffer(void *obj) {
  struct unixcall_mtldevice_newbuffer *params = obj;
  id<MTLDevice> device = (id<MTLDevice>)params->device;
  struct WMTBufferInfo *info = params->info;
  id<MTLBuffer> buffer;
  if (info->memory.ptr) {
    buffer = [device newBufferWithBytesNoCopy:info->memory.ptr
                                       length:info->length
                                      options:(enum MTLResourceOptions)info->options
                                  deallocator:NULL];
  } else {
    buffer = [device newBufferWithLength:info->length options:(enum MTLResourceOptions)info->options];
    info->memory.ptr = [buffer storageMode] == MTLStorageModePrivate ? NULL : [buffer contents];
  }
  params->ret = (obj_handle_t)buffer;
  info->gpu_address = [buffer gpuAddress];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_newSamplerState(void *obj) {
  struct unixcall_mtldevice_newsamplerstate *params = obj;
  id<MTLDevice> device = (id<MTLDevice>)params->device;
  struct WMTSamplerInfo *info = params->info;

  MTLSamplerDescriptor *sampler_desc = [[MTLSamplerDescriptor alloc] init];
  sampler_desc.borderColor = (MTLSamplerBorderColor)info->border_color;
  sampler_desc.rAddressMode = (MTLSamplerAddressMode)info->r_address_mode;
  sampler_desc.sAddressMode = (MTLSamplerAddressMode)info->s_address_mode;
  sampler_desc.tAddressMode = (MTLSamplerAddressMode)info->t_address_mode;
  sampler_desc.magFilter = (MTLSamplerMinMagFilter)info->mag_filter;
  sampler_desc.minFilter = (MTLSamplerMinMagFilter)info->min_filter;
  sampler_desc.mipFilter = (MTLSamplerMipFilter)info->mip_filter;
  sampler_desc.compareFunction = (MTLCompareFunction)info->compare_function;
  sampler_desc.lodMaxClamp = info->lod_max_clamp;
  sampler_desc.lodMinClamp = info->lod_min_clamp;
  sampler_desc.maxAnisotropy = info->max_anisotroy;
  sampler_desc.lodAverage = info->lod_average;
  sampler_desc.normalizedCoordinates = info->normalized_coords;
  sampler_desc.supportArgumentBuffers = info->support_argument_buffers;

  id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sampler_desc];
  info->gpu_resource_id = info->support_argument_buffers ? [sampler gpuResourceID]._impl : 0;
  params->ret = (obj_handle_t)sampler;
  [sampler_desc release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_newDepthStencilState(void *obj) {
  struct unixcall_mtldevice_newdepthstencilstate *params = obj;
  id<MTLDevice> device = (id<MTLDevice>)params->device;
  struct WMTDepthStencilInfo *info = params->info;

  MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
  desc.depthCompareFunction = (MTLCompareFunction)info->depth_compare_function;
  desc.depthWriteEnabled = info->depth_write_enabled;

  if (info->front_stencil.enabled) {
    desc.frontFaceStencil.depthStencilPassOperation = (MTLStencilOperation)info->front_stencil.depth_stencil_pass_op;
    desc.frontFaceStencil.depthFailureOperation = (MTLStencilOperation)info->front_stencil.depth_fail_op;
    desc.frontFaceStencil.stencilFailureOperation = (MTLStencilOperation)info->front_stencil.stencil_fail_op;
    desc.frontFaceStencil.stencilCompareFunction = (MTLCompareFunction)info->front_stencil.stencil_compare_function;
    desc.frontFaceStencil.writeMask = info->front_stencil.write_mask;
    desc.frontFaceStencil.readMask = info->front_stencil.read_mask;
  }

  if (info->back_stencil.enabled) {
    desc.backFaceStencil.depthStencilPassOperation = (MTLStencilOperation)info->back_stencil.depth_stencil_pass_op;
    desc.backFaceStencil.depthFailureOperation = (MTLStencilOperation)info->back_stencil.depth_fail_op;
    desc.backFaceStencil.stencilFailureOperation = (MTLStencilOperation)info->back_stencil.stencil_fail_op;
    desc.backFaceStencil.stencilCompareFunction = (MTLCompareFunction)info->back_stencil.stencil_compare_function;
    desc.backFaceStencil.writeMask = info->back_stencil.write_mask;
    desc.backFaceStencil.readMask = info->back_stencil.read_mask;
  }

  params->ret = (obj_handle_t)[device newDepthStencilStateWithDescriptor:desc];
  [desc release];
  return STATUS_SUCCESS;
}

void
fill_texture_descriptor(MTLTextureDescriptor *desc, struct WMTTextureInfo *info) {
  desc.textureType = (MTLTextureType)info->type;
  desc.pixelFormat = (MTLPixelFormat)info->pixel_format;
  desc.width = info->width;
  desc.height = info->height;
  desc.depth = info->depth;
  desc.arrayLength = info->array_length;
  desc.mipmapLevelCount = info->mipmap_level_count;
  desc.sampleCount = info->sample_count;
  desc.usage = (MTLTextureUsage)info->usage;
  desc.resourceOptions = (MTLResourceOptions)info->options;
};

static NTSTATUS
_MTLDevice_newTexture(void *obj) {
  struct unixcall_mtldevice_newtexture *params = obj;
  id<MTLDevice> device = (id<MTLDevice>)params->device;
  struct WMTTextureInfo *info = params->info;
  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  fill_texture_descriptor(desc, info);

  id<MTLTexture> ret = [device newTextureWithDescriptor:desc];
  params->ret = (obj_handle_t)ret;
  info->gpu_resource_id = [ret gpuResourceID]._impl;

  [desc release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLBuffer_newTexture(void *obj) {
  struct unixcall_mtlbuffer_newtexture *params = obj;
  id<MTLBuffer> buffer = (id<MTLBuffer>)params->buffer;
  struct WMTTextureInfo *info = params->info;
  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  fill_texture_descriptor(desc, info);

  id<MTLTexture> ret = [buffer newTextureWithDescriptor:desc offset:params->offset bytesPerRow:params->bytes_per_row];
  assert(ret);
  params->ret = (obj_handle_t)ret;
  info->gpu_resource_id = [ret gpuResourceID]._impl;

  [desc release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_newTextureView(void *obj) {
  struct unixcall_mtltexture_newtextureview *params = obj;
  id<MTLTexture> texture = (id<MTLTexture>)params->texture;

  id<MTLTexture> ret = [texture
      newTextureViewWithPixelFormat:(MTLPixelFormat)params->format
                        textureType:(MTLTextureType)params->texture_type
                             levels:NSMakeRange(params->level_start, params->level_count)
                             slices:NSMakeRange(params->slice_start, params->slice_count)
                            swizzle:MTLTextureSwizzleChannelsMake(
                                        (MTLTextureSwizzle)params->swizzle.r, (MTLTextureSwizzle)params->swizzle.g,
                                        (MTLTextureSwizzle)params->swizzle.b, (MTLTextureSwizzle)params->swizzle.a
                                    )];
  params->ret = (obj_handle_t)ret;
  params->gpu_resource_id = [ret gpuResourceID]._impl;
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_minimumLinearTextureAlignmentForPixelFormat(void *obj) {
  struct unixcall_generic_obj_uint64_uint64_ret *params = obj;
  params->ret = [(id<MTLDevice>)params->handle minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)params->arg];
  return STATUS_SUCCESS;
}

const void *__winemetal_unixcalls[] = {
    &_NSObject_retain,
    &_NSObject_release,
    &_NSArray_object,
    &_NSArray_count,
    &_MTLCopyAllDevices,
    &_MTLDevice_recommendedMaxWorkingSetSize,
    &_MTLDevice_currentAllocatedSize,
    &_MTLDevice_name,
    &_NSString_getCString,
    &_MTLDevice_newCommandQueue,
    &_NSAutoreleasePool_alloc_init,
    &_MTLCommandQueue_commandBuffer,
    &_MTLCommandBuffer_commit,
    &_MTLCommandBuffer_waitUntilCompleted,
    &_MTLCommandBuffer_status,
    &_MTLDevice_newSharedEvent,
    &_MTLSharedEvent_signaledValue,
    &_MTLCommandBuffer_encodeSignalEvent,
    &_MTLDevice_newBuffer,
    &_MTLDevice_newSamplerState,
    &_MTLDevice_newDepthStencilState,
    &_MTLDevice_newTexture,
    &_MTLBuffer_newTexture,
    &_MTLTexture_newTextureView,
    &_MTLDevice_minimumLinearTextureAlignmentForPixelFormat,
};

const unsigned int __winemetal_unixcalls_num = sizeof(__winemetal_unixcalls) / sizeof(void *);
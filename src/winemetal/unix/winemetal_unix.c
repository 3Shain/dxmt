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

static NTSTATUS
_MTLDevice_newLibrary(void *obj) {
  struct unixcall_mtldevice_newlibrary *params = obj;
  id<MTLDevice> device = (id<MTLDevice>)params->device;
  dispatch_data_t data = dispatch_data_create(params->bytecode.ptr, params->bytecode_length, NULL, NULL);
  NSError *err = NULL;
  params->ret_library = (obj_handle_t)[device newLibraryWithData:data error:&err];
  params->ret_error = (obj_handle_t)err;
  dispatch_release(data);
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLLibrary_newFunction(void *obj) {
  struct unixcall_generic_obj_uint64_obj_ret *params = obj;
  id<MTLLibrary> library = (id<MTLLibrary>)params->handle;
  NSString *name = [[NSString alloc] initWithCString:(char *)params->arg encoding:NSUTF8StringEncoding];
  params->ret = (obj_handle_t)[library newFunctionWithName:name];
  [name release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSString_lengthOfBytesUsingEncoding(void *obj) {
  struct unixcall_generic_obj_uint64_uint64_ret *params = obj;
  params->ret = (uint64_t)[(NSString *)params->handle lengthOfBytesUsingEncoding:(NSStringEncoding)params->arg];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSError_description(void *obj) {
  struct unixcall_generic_obj_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(NSError *)params->handle description];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_newComputePipelineState(void *obj) {
  struct unixcall_mtldevice_newcomputepso *params = obj;
  id<MTLDevice> device = (id<MTLDevice>)params->device;
  NSError *err = NULL;
  params->ret_pso =
      (obj_handle_t)[device newComputePipelineStateWithFunction:(id<MTLFunction>)params->function error:&err];
  params->ret_error = (obj_handle_t)err;
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandBuffer_blitCommandEncoder(void *obj) {
  struct unixcall_generic_obj_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(id<MTLCommandBuffer>)params->handle blitCommandEncoder];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandBuffer_computeCommandEncoder(void *obj) {
  struct unixcall_generic_obj_uint64_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(id<MTLCommandBuffer>)params->handle
      computeCommandEncoderWithDispatchType:params->arg ? MTLDispatchTypeConcurrent : MTLDispatchTypeSerial];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandBuffer_renderCommandEncoder(void *obj) {
  struct unixcall_generic_obj_uint64_obj_ret *params = obj;
  struct WMTRenderPassInfo *info = (struct WMTRenderPassInfo *)params->arg;
  MTLRenderPassDescriptor *descriptor = [[MTLRenderPassDescriptor alloc] init];
  for (unsigned i = 0; i < 8; i++) {
    descriptor.colorAttachments[i].clearColor = MTLClearColorMake(
        info->colors[i].clear_color.r, info->colors[i].clear_color.g, info->colors[i].clear_color.b,
        info->colors[i].clear_color.a
    );
    descriptor.colorAttachments[i].level = info->colors[i].level;
    descriptor.colorAttachments[i].slice = info->colors[i].slice;
    descriptor.colorAttachments[i].depthPlane = info->colors[i].depth_plane;
    descriptor.colorAttachments[i].texture = (id<MTLTexture>)info->colors[i].texture;
    descriptor.colorAttachments[i].loadAction = (MTLLoadAction)info->colors[i].load_action;
    descriptor.colorAttachments[i].storeAction = (MTLStoreAction)info->colors[i].store_action;
    descriptor.colorAttachments[i].resolveTexture = (id<MTLTexture>)info->colors[i].resolve_texture;
    descriptor.colorAttachments[i].resolveLevel = info->colors[i].resolve_level;
    descriptor.colorAttachments[i].resolveSlice = info->colors[i].resolve_slice;
    descriptor.colorAttachments[i].resolveDepthPlane = info->colors[i].resolve_depth_plane;
  }

  if (info->depth.texture) {
    descriptor.depthAttachment.clearDepth = info->depth.clear_depth;
    descriptor.depthAttachment.depthPlane = info->depth.depth_plane;
    descriptor.depthAttachment.level = info->depth.level;
    descriptor.depthAttachment.slice = info->depth.slice;
    descriptor.depthAttachment.texture = (id<MTLTexture>)info->depth.texture;
    descriptor.depthAttachment.loadAction = (MTLLoadAction)info->depth.load_action;
    descriptor.depthAttachment.storeAction = (MTLStoreAction)info->depth.store_action;
  }

  if (info->stencil.texture) {
    descriptor.stencilAttachment.clearStencil = info->stencil.clear_stencil;
    descriptor.stencilAttachment.depthPlane = info->stencil.depth_plane;
    descriptor.stencilAttachment.level = info->stencil.level;
    descriptor.stencilAttachment.slice = info->stencil.slice;
    descriptor.stencilAttachment.texture = (id<MTLTexture>)info->stencil.texture;
    descriptor.stencilAttachment.loadAction = (MTLLoadAction)info->stencil.load_action;
    descriptor.stencilAttachment.storeAction = (MTLStoreAction)info->stencil.store_action;
  }

  descriptor.defaultRasterSampleCount = info->default_raster_sample_count;
  descriptor.renderTargetArrayLength = info->render_target_array_length;
  descriptor.renderTargetHeight = info->render_target_height;
  descriptor.renderTargetWidth = info->render_target_width;
  descriptor.visibilityResultBuffer = (id<MTLBuffer>)info->visibility_buffer;

  params->ret = (obj_handle_t)[(id<MTLCommandBuffer>)params->handle renderCommandEncoderWithDescriptor:descriptor];

  [descriptor release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCommandEncoder_endEncoding(void *obj) {
  struct unixcall_generic_obj_noret *params = obj;
  [(id<MTLCommandEncoder>)params->handle endEncoding];
  return STATUS_SUCCESS;
}

#ifndef DXMT_NO_PRIVATE_API

typedef NS_ENUM(NSUInteger, MTLLogicOperation) {
  MTLLogicOperationClear,
  MTLLogicOperationSet,
  MTLLogicOperationCopy,
  MTLLogicOperationCopyInverted,
  MTLLogicOperationNoop,
  MTLLogicOperationInvert,
  MTLLogicOperationAnd,
  MTLLogicOperationNand,
  MTLLogicOperationOr,
  MTLLogicOperationNor,
  MTLLogicOperationXor,
  MTLLogicOperationEquivalence,
  MTLLogicOperationAndReverse,
  MTLLogicOperationAndInverted,
  MTLLogicOperationOrReverse,
  MTLLogicOperationOrInverted,
};

@interface
MTLRenderPipelineDescriptor ()

- (void)setLogicOperationEnabled:(BOOL)enable;
- (void)setLogicOperation:(MTLLogicOperation)op;

@end

@interface
MTLMeshRenderPipelineDescriptor ()

- (void)setLogicOperationEnabled:(BOOL)enable;
- (void)setLogicOperation:(MTLLogicOperation)op;

@end

#endif

static NTSTATUS
_MTLDevice_newRenderPipelineState(void *obj) {
  struct unixcall_mtldevice_newrenderpso *params = obj;
  struct WMTRenderPipelineInfo *info = (struct WMTRenderPipelineInfo *)params->info;
  MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];

  for (unsigned i = 0; i < 8; i++) {
    descriptor.colorAttachments[i].pixelFormat = (MTLPixelFormat)info->colors[i].pixel_format;
    descriptor.colorAttachments[i].blendingEnabled = info->colors[i].blending_enabled;
    descriptor.colorAttachments[i].writeMask = (MTLColorWriteMask)info->colors[i].write_mask;

    descriptor.colorAttachments[i].alphaBlendOperation = (MTLBlendOperation)info->colors[i].alpha_blend_operation;
    descriptor.colorAttachments[i].rgbBlendOperation = (MTLBlendOperation)info->colors[i].rgb_blend_operation;

    descriptor.colorAttachments[i].sourceRGBBlendFactor = (MTLBlendFactor)info->colors[i].src_rgb_blend_factor;
    descriptor.colorAttachments[i].sourceAlphaBlendFactor = (MTLBlendFactor)info->colors[i].src_alpha_blend_factor;
    descriptor.colorAttachments[i].destinationRGBBlendFactor = (MTLBlendFactor)info->colors[i].dst_rgb_blend_factor;
    descriptor.colorAttachments[i].destinationAlphaBlendFactor = (MTLBlendFactor)info->colors[i].dst_alpha_blend_factor;
  }

  for (unsigned i = 0; i < 31; i++) {
    if (info->immutable_fragment_buffers & (1 << i))
      descriptor.fragmentBuffers[i].mutability = MTLMutabilityImmutable;
    if (info->immutable_vertex_buffers & (1 << i))
      descriptor.vertexBuffers[i].mutability = MTLMutabilityImmutable;
  }

#ifndef DXMT_NO_PRIVATE_API
  [descriptor setLogicOperationEnabled:info->logic_operation_enabled];
  [descriptor setLogicOperation:(MTLLogicOperation)info->logic_operation];
#endif
  descriptor.depthAttachmentPixelFormat = (MTLPixelFormat)info->depth_pixel_format;
  descriptor.stencilAttachmentPixelFormat = (MTLPixelFormat)info->stencil_pixel_format;
  descriptor.alphaToCoverageEnabled = info->alpha_to_coverage_enabled;
  descriptor.rasterizationEnabled = info->rasterization_enabled;
  descriptor.rasterSampleCount = info->raster_sample_count;
  descriptor.inputPrimitiveTopology = (MTLPrimitiveTopologyClass)info->input_primitive_topology;
  descriptor.tessellationPartitionMode = (MTLTessellationPartitionMode)info->tessellation_partition_mode;
  descriptor.tessellationFactorStepFunction = (MTLTessellationFactorStepFunction)info->tessellation_factor_step;
  descriptor.tessellationOutputWindingOrder = (MTLWinding)info->tessellation_output_winding_order;
  descriptor.maxTessellationFactor = info->max_tessellation_factor;

  descriptor.vertexFunction = (id<MTLFunction>)info->vertex_function;
  descriptor.fragmentFunction = (id<MTLFunction>)info->fragment_function;

  NSError *err = NULL;
  params->ret_pso =
      (obj_handle_t)[(id<MTLDevice>)params->device newRenderPipelineStateWithDescriptor:descriptor error:&err];
  params->ret_error = (obj_handle_t)err;
  [descriptor release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_newMeshRenderPipelineState(void *obj) {
  struct unixcall_mtldevice_newmeshrenderpso *params = obj;
  struct WMTMeshRenderPipelineInfo *info = (struct WMTMeshRenderPipelineInfo *)params->info;
  MTLMeshRenderPipelineDescriptor *descriptor = [[MTLMeshRenderPipelineDescriptor alloc] init];

  for (unsigned i = 0; i < 8; i++) {
    descriptor.colorAttachments[i].pixelFormat = (MTLPixelFormat)info->colors[i].pixel_format;
    descriptor.colorAttachments[i].blendingEnabled = info->colors[i].blending_enabled;
    descriptor.colorAttachments[i].writeMask = (MTLColorWriteMask)info->colors[i].write_mask;

    descriptor.colorAttachments[i].alphaBlendOperation = (MTLBlendOperation)info->colors[i].alpha_blend_operation;
    descriptor.colorAttachments[i].rgbBlendOperation = (MTLBlendOperation)info->colors[i].rgb_blend_operation;

    descriptor.colorAttachments[i].sourceRGBBlendFactor = (MTLBlendFactor)info->colors[i].src_rgb_blend_factor;
    descriptor.colorAttachments[i].sourceAlphaBlendFactor = (MTLBlendFactor)info->colors[i].src_alpha_blend_factor;
    descriptor.colorAttachments[i].destinationRGBBlendFactor = (MTLBlendFactor)info->colors[i].dst_rgb_blend_factor;
    descriptor.colorAttachments[i].destinationAlphaBlendFactor = (MTLBlendFactor)info->colors[i].dst_alpha_blend_factor;
  }

  for (unsigned i = 0; i < 31; i++) {
    if (info->immutable_fragment_buffers & (1 << i))
      descriptor.fragmentBuffers[i].mutability = MTLMutabilityImmutable;
    if (info->immutable_mesh_buffers & (1 << i))
      descriptor.meshBuffers[i].mutability = MTLMutabilityImmutable;
    if (info->immutable_object_buffers & (1 << i))
      descriptor.objectBuffers[i].mutability = MTLMutabilityImmutable;
  }

#ifndef DXMT_NO_PRIVATE_API
  [descriptor setLogicOperationEnabled:info->logic_operation_enabled];
  [descriptor setLogicOperation:(MTLLogicOperation)info->logic_operation];
#endif
  descriptor.depthAttachmentPixelFormat = (MTLPixelFormat)info->depth_pixel_format;
  descriptor.stencilAttachmentPixelFormat = (MTLPixelFormat)info->stencil_pixel_format;
  descriptor.alphaToCoverageEnabled = info->alpha_to_coverage_enabled;
  descriptor.rasterizationEnabled = info->rasterization_enabled;
  descriptor.rasterSampleCount = info->raster_sample_count;

  descriptor.objectFunction = (id<MTLFunction>)info->object_function;
  descriptor.meshFunction = (id<MTLFunction>)info->mesh_function;
  descriptor.fragmentFunction = (id<MTLFunction>)info->fragment_function;
  descriptor.payloadMemoryLength = info->payload_memory_length;

  NSError *err = NULL;
  params->ret_pso =
      (obj_handle_t)[(id<MTLDevice>)params->device newRenderPipelineStateWithMeshDescriptor:descriptor
                                                                                    options:MTLPipelineOptionNone
                                                                                 reflection:nil
                                                                                      error:&err];
  params->ret_error = (obj_handle_t)err;
  [descriptor release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLBlitCommandEncoder_encodeCommands(void *obj) {
  struct unixcall_generic_obj_cmd_noret *params = obj;
  struct wmtcmd_base *next = (struct wmtcmd_base *)params->cmd_head;
  id<MTLBlitCommandEncoder> encoder = (id<MTLBlitCommandEncoder>)params->encoder;
  while (next) {
    switch ((enum WMTBlitCommandType)next->type) {
    default:
      assert(!next->type && "unhandled blit command type");
      break;
    case WMTBlitCommandCopyFromBufferToBuffer: {
      struct wmtcmd_blit_copy_from_buffer_to_buffer *body = (struct wmtcmd_blit_copy_from_buffer_to_buffer *)next;
      [encoder copyFromBuffer:(id<MTLBuffer>)body->src
                 sourceOffset:body->src_offset
                     toBuffer:(id<MTLBuffer>)body->dst
            destinationOffset:body->dst_offset
                         size:body->copy_length];
      break;
    }
    case WMTBlitCommandCopyFromBufferToTexture: {
      struct wmtcmd_blit_copy_from_buffer_to_texture *body = (struct wmtcmd_blit_copy_from_buffer_to_texture *)next;
      [encoder copyFromBuffer:(id<MTLBuffer>)body->src
                 sourceOffset:body->src_offset
            sourceBytesPerRow:body->bytes_per_row
          sourceBytesPerImage:body->bytes_per_image
                   sourceSize:MTLSizeMake(body->size.width, body->size.height, body->size.depth)
                    toTexture:(id<MTLTexture>)body->dst
             destinationSlice:body->slice
             destinationLevel:body->level
            destinationOrigin:MTLOriginMake(body->origin.x, body->origin.y, body->origin.z)];
      break;
    }
    case WMTBlitCommandCopyFromTextureToBuffer: {
      struct wmtcmd_blit_copy_from_texture_to_buffer *body = (struct wmtcmd_blit_copy_from_texture_to_buffer *)next;
      [encoder copyFromTexture:(id<MTLTexture>)body->src
                       sourceSlice:body->slice
                       sourceLevel:body->level
                      sourceOrigin:MTLOriginMake(body->origin.x, body->origin.y, body->origin.z)
                        sourceSize:MTLSizeMake(body->size.width, body->size.height, body->size.depth)
                          toBuffer:(id<MTLBuffer>)body->dst
                 destinationOffset:body->offset
            destinationBytesPerRow:body->bytes_per_row
          destinationBytesPerImage:body->bytes_per_image];
      break;
    }
    case WMTBlitCommandCopyFromTextureToTexture: {
      struct wmtcmd_blit_copy_from_texture_to_texture *body = (struct wmtcmd_blit_copy_from_texture_to_texture *)next;
      [encoder copyFromTexture:(id<MTLTexture>)body->src
                   sourceSlice:body->src_slice
                   sourceLevel:body->src_level
                  sourceOrigin:MTLOriginMake(body->src_origin.x, body->src_origin.y, body->src_origin.z)
                    sourceSize:MTLSizeMake(body->src_size.width, body->src_size.height, body->src_size.depth)
                     toTexture:(id<MTLTexture>)body->dst
              destinationSlice:body->dst_slice
              destinationLevel:body->dst_level
             destinationOrigin:MTLOriginMake(body->dst_origin.x, body->dst_origin.y, body->dst_origin.z)];
      break;
    }
    case WMTBlitCommandGenerateMipmaps: {
      struct wmtcmd_blit_generate_mipmaps *body = (struct wmtcmd_blit_generate_mipmaps *)next;
      [encoder generateMipmapsForTexture:(id<MTLTexture>)body->texture];
      break;
    }
    }

    next = next->next.ptr;
  }
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLComputeCommandEncoder_encodeCommands(void *obj) {
  struct unixcall_generic_obj_cmd_noret *params = obj;
  struct wmtcmd_base *next = (struct wmtcmd_base *)params->cmd_head;
  id<MTLComputeCommandEncoder> encoder = (id<MTLComputeCommandEncoder>)params->encoder;
  MTLSize threadgroup_size = {0, 0, 0};
  while (next) {
    switch ((enum WMTComputeCommandType)next->type) {
    default:
      assert(!next->type && "unhandled compute command type");
      break;
    case WMTComputeCommandDispatch: {
      struct wmtcmd_compute_dispatch *body = (struct wmtcmd_compute_dispatch *)next;
      [encoder dispatchThreadgroups:MTLSizeMake(body->size.width, body->size.height, body->size.depth)
              threadsPerThreadgroup:threadgroup_size];
      break;
    }
    case WMTComputeCommandDispatchThreads: {
      struct wmtcmd_compute_dispatch *body = (struct wmtcmd_compute_dispatch *)next;
      [encoder dispatchThreads:MTLSizeMake(body->size.width, body->size.height, body->size.depth)
          threadsPerThreadgroup:threadgroup_size];
      break;
    }
    case WMTComputeCommandDispatchIndirect: {
      struct wmtcmd_compute_dispatch_indirect *body = (struct wmtcmd_compute_dispatch_indirect *)next;
      [encoder dispatchThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)body->indirect_args_buffer
                                 indirectBufferOffset:body->indirect_args_offset
                                threadsPerThreadgroup:threadgroup_size];
      break;
    }
    case WMTComputeCommandSetPSO: {
      struct wmtcmd_compute_setpso *body = (struct wmtcmd_compute_setpso *)next;
      [encoder setComputePipelineState:(id<MTLComputePipelineState>)body->pso];
      threadgroup_size.width = body->threadgroup_size.width;
      threadgroup_size.height = body->threadgroup_size.height;
      threadgroup_size.depth = body->threadgroup_size.depth;
      break;
    }
    case WMTComputeCommandSetBuffer: {
      struct wmtcmd_compute_setbuffer *body = (struct wmtcmd_compute_setbuffer *)next;
      [encoder setBuffer:(id<MTLBuffer>)body->buffer offset:body->offset atIndex:body->index];
      break;
    }
    case WMTComputeCommandSetBufferOffset: {
      struct wmtcmd_compute_setbufferoffset *body = (struct wmtcmd_compute_setbufferoffset *)next;
      [encoder setBufferOffset:body->offset atIndex:body->index];
      break;
    }
    case WMTComputeCommandUseResource: {
      struct wmtcmd_compute_useresource *body = (struct wmtcmd_compute_useresource *)next;
      [encoder useResource:(id<MTLResource>)body->resource usage:(MTLResourceUsage)body->usage];
      break;
    }
    case WMTComputeCommandSetBytes: {
      struct wmtcmd_compute_setbytes *body = (struct wmtcmd_compute_setbytes *)next;
      [encoder setBytes:body->bytes.ptr length:body->length atIndex:body->index];
      break;
    }
    case WMTComputeCommandSetTexture: {
      struct wmtcmd_compute_settexture *body = (struct wmtcmd_compute_settexture *)next;
      [encoder setTexture:(id<MTLTexture>)body->texture atIndex:body->index];
      break;
    }
    }

    next = next->next.ptr;
  }
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLRenderCommandEncoder_encodeCommands(void *obj) {
  struct unixcall_generic_obj_cmd_noret *params = obj;
  struct wmtcmd_base *next = (struct wmtcmd_base *)params->cmd_head;
  id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)params->encoder;
  while (next) {
    switch ((enum WMTRenderCommandType)next->type) {
    default:
      assert(!next->type && "unhandled render command type");
      break;
    case WMTRenderCommandNop:
      break;
    case WMTRenderCommandUseResource: {
      struct wmtcmd_render_useresource *body = (struct wmtcmd_render_useresource *)next;
      [encoder useResource:(id<MTLResource>)body->resource
                     usage:(MTLResourceUsage)body->usage
                    stages:(MTLRenderStages)body->stages];
      break;
    }
    case WMTRenderCommandSetVertexBuffer: {
      struct wmtcmd_render_setbuffer *body = (struct wmtcmd_render_setbuffer *)next;
      [encoder setVertexBuffer:(id<MTLBuffer>)body->buffer offset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetVertexBufferOffset: {
      struct wmtcmd_render_setbufferoffset *body = (struct wmtcmd_render_setbufferoffset *)next;
      [encoder setVertexBufferOffset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetFragmentBuffer: {
      struct wmtcmd_render_setbuffer *body = (struct wmtcmd_render_setbuffer *)next;
      [encoder setFragmentBuffer:(id<MTLBuffer>)body->buffer offset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetFragmentBufferOffset: {
      struct wmtcmd_render_setbufferoffset *body = (struct wmtcmd_render_setbufferoffset *)next;
      [encoder setFragmentBufferOffset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetMeshBuffer: {
      struct wmtcmd_render_setbuffer *body = (struct wmtcmd_render_setbuffer *)next;
      [encoder setMeshBuffer:(id<MTLBuffer>)body->buffer offset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetMeshBufferOffset: {
      struct wmtcmd_render_setbufferoffset *body = (struct wmtcmd_render_setbufferoffset *)next;
      [encoder setMeshBufferOffset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetObjectBuffer: {
      struct wmtcmd_render_setbuffer *body = (struct wmtcmd_render_setbuffer *)next;
      [encoder setObjectBuffer:(id<MTLBuffer>)body->buffer offset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetObjectBufferOffset: {
      struct wmtcmd_render_setbufferoffset *body = (struct wmtcmd_render_setbufferoffset *)next;
      [encoder setObjectBufferOffset:body->offset atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetRasterizerState: {
      struct wmtcmd_render_setrasterizerstate *body = (struct wmtcmd_render_setrasterizerstate *)next;
      [encoder setTriangleFillMode:(MTLTriangleFillMode)body->fill_mode];
      [encoder setCullMode:(MTLCullMode)body->cull_mode];
      [encoder setDepthClipMode:(MTLDepthClipMode)body->depth_clip_mode];
      [encoder setDepthBias:body->depth_bias slopeScale:body->scole_scale clamp:body->depth_bias_clamp];
      [encoder setFrontFacingWinding:(MTLWinding)body->winding];
      break;
    }
    case WMTRenderCommandSetViewports: {
      struct wmtcmd_render_setviewports *body = (struct wmtcmd_render_setviewports *)next;
      [encoder setViewports:(const MTLViewport *)body->viewports.ptr count:body->viewport_count];
      break;
    }
    case WMTRenderCommandSetScissorRects: {
      struct wmtcmd_render_setscissorrects *body = (struct wmtcmd_render_setscissorrects *)next;
      [encoder setScissorRects:(const MTLScissorRect *)body->scissor_rects.ptr count:body->rect_count];
      break;
    }
    case WMTRenderCommandSetPSO: {
      struct wmtcmd_render_setpso *body = (struct wmtcmd_render_setpso *)next;
      [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)body->pso];
      break;
    }
    case WMTRenderCommandSetDSSO: {
      struct wmtcmd_render_setdsso *body = (struct wmtcmd_render_setdsso *)next;
      [encoder setDepthStencilState:(id<MTLDepthStencilState>)body->dsso];
      [encoder setStencilReferenceValue:body->stencil_ref];
      break;
    }
    case WMTRenderCommandSetBlendFactorAndStencilRef: {
      struct wmtcmd_render_setblendcolor *body = (struct wmtcmd_render_setblendcolor *)next;
      [encoder setBlendColorRed:body->red green:body->green blue:body->blue alpha:body->alpha];
      [encoder setStencilReferenceValue:body->stencil_ref];
      break;
    }
    case WMTRenderCommandSetVisibilityMode: {
      struct wmtcmd_render_setvisibilitymode *body = (struct wmtcmd_render_setvisibilitymode *)next;
      [encoder setVisibilityResultMode:(MTLVisibilityResultMode)body->mode offset:body->offset];
      break;
    }
    case WMTRenderCommandDraw: {
      struct wmtcmd_render_draw *body = (struct wmtcmd_render_draw *)next;
      [encoder drawPrimitives:(MTLPrimitiveType)body->primitive_type
                  vertexStart:body->vertex_start
                  vertexCount:body->vertex_count
                instanceCount:body->instance_count
                 baseInstance:body->base_instance];
      break;
    }
    case WMTRenderCommandDrawIndexed: {
      struct wmtcmd_render_draw_indexed *body = (struct wmtcmd_render_draw_indexed *)next;
      [encoder drawIndexedPrimitives:(MTLPrimitiveType)body->primitive_type
                          indexCount:body->index_count
                           indexType:(MTLIndexType)body->index_type
                         indexBuffer:(id<MTLBuffer>)body->index_buffer
                   indexBufferOffset:body->index_buffer_offset
                       instanceCount:body->instance_count
                          baseVertex:body->base_vertex
                        baseInstance:body->base_instance];
      break;
    }
    case WMTRenderCommandDrawIndirect: {
      struct wmtcmd_render_draw_indirect *body = (struct wmtcmd_render_draw_indirect *)next;
      [encoder drawPrimitives:(MTLPrimitiveType)body->primitive_type
                indirectBuffer:(id<MTLBuffer>)body->indirect_args_buffer
          indirectBufferOffset:body->indirect_args_offset];
      break;
    }
    case WMTRenderCommandDrawIndexedIndirect: {
      struct wmtcmd_render_draw_indexed_indirect *body = (struct wmtcmd_render_draw_indexed_indirect *)next;
      [encoder drawIndexedPrimitives:(MTLPrimitiveType)body->primitive_type
                           indexType:(MTLIndexType)body->index_type
                         indexBuffer:(id<MTLBuffer>)body->index_buffer
                   indexBufferOffset:body->index_buffer_offset
                      indirectBuffer:(id<MTLBuffer>)body->indirect_args_buffer
                indirectBufferOffset:body->indirect_args_offset];
      break;
    }
    case WMTRenderCommandDrawMeshThreadgroups: {
      struct wmtcmd_render_draw_meshthreadgroups *body = (struct wmtcmd_render_draw_meshthreadgroups *)next;
      [encoder drawMeshThreadgroups:MTLSizeMake(
                                        body->threadgroup_per_grid.width, body->threadgroup_per_grid.height,
                                        body->threadgroup_per_grid.depth
                                    )
          threadsPerObjectThreadgroup:MTLSizeMake(
                                          body->object_threadgroup_size.width, body->object_threadgroup_size.height,
                                          body->object_threadgroup_size.depth
                                      )
            threadsPerMeshThreadgroup:MTLSizeMake(
                                          body->mesh_threadgroup_size.width, body->mesh_threadgroup_size.height,
                                          body->mesh_threadgroup_size.depth
                                      )];
      break;
    }
    case WMTRenderCommandDrawMeshThreadgroupsIndirect: {
      struct wmtcmd_render_draw_meshthreadgroups_indirect *body =
          (struct wmtcmd_render_draw_meshthreadgroups_indirect *)next;
      [encoder drawMeshThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)body->indirect_args_buffer
                                 indirectBufferOffset:body->indirect_args_offset
                          threadsPerObjectThreadgroup:MTLSizeMake(
                                                          body->object_threadgroup_size.width,
                                                          body->object_threadgroup_size.height,
                                                          body->object_threadgroup_size.depth
                                                      )
                            threadsPerMeshThreadgroup:MTLSizeMake(
                                                          body->mesh_threadgroup_size.width,
                                                          body->mesh_threadgroup_size.height,
                                                          body->mesh_threadgroup_size.depth
                                                      )];
      break;
    }
    case WMTRenderCommandMemoryBarrier: {
      struct wmtcmd_render_memory_barrier *body = (struct wmtcmd_render_memory_barrier *)next;
      [encoder memoryBarrierWithScope:(MTLBarrierScope)body->scope
                          afterStages:(MTLRenderStages)body->stages_after
                         beforeStages:(MTLRenderStages)body->stages_before];
      break;
    }
    case WMTRenderCommandDXMTTessellationDraw: {
      struct wmtcmd_render_dxmt_tess_draw *body = (struct wmtcmd_render_dxmt_tess_draw *)next;
      [encoder setVertexBufferOffset:body->draw_arguments_offset atIndex:23];
      [encoder setVertexBuffer:(id<MTLBuffer>)body->control_point_buffer
                        offset:body->control_point_buffer_offset
                       atIndex:20];
      [encoder setVertexBuffer:(id<MTLBuffer>)body->patch_constant_buffer
                        offset:body->patch_constant_buffer_offset
                       atIndex:21];
      [encoder setVertexBuffer:(id<MTLBuffer>)body->tessellation_factor_buffer
                        offset:body->tessellation_factor_buffer_offset
                       atIndex:22];
      [encoder setTessellationFactorBuffer:(id<MTLBuffer>)body->tessellation_factor_buffer
                                    offset:body->tessellation_factor_buffer_offset
                            instanceStride:0];
      [encoder drawPatches:0
                      patchStart:0
                      patchCount:body->patch_count_per_instance * body->instance_count
                patchIndexBuffer:nil
          patchIndexBufferOffset:0
                   instanceCount:1
                    baseInstance:0];
      break;
    }
    case WMTRenderCommandDXMTTessellationMeshDispatchIndexed: {
      struct wmtcmd_render_dxmt_tess_mesh_dispatch_indexed *body =
          (struct wmtcmd_render_dxmt_tess_mesh_dispatch_indexed *)next;
      [encoder setObjectBuffer:(id<MTLBuffer>)body->index_buffer offset:body->index_buffer_offset atIndex:20];
      [encoder setObjectBufferOffset:body->draw_arguments_offset atIndex:21];
      [encoder setMeshBuffer:(id<MTLBuffer>)body->control_point_buffer
                      offset:body->control_point_buffer_offset
                     atIndex:20];
      [encoder setMeshBuffer:(id<MTLBuffer>)body->patch_constant_buffer
                      offset:body->patch_constant_buffer_offset
                     atIndex:21];
      [encoder setMeshBuffer:(id<MTLBuffer>)body->tessellation_factor_buffer
                      offset:body->tessellation_factor_buffer_offset
                     atIndex:22];
      [encoder drawMeshThreadgroups:MTLSizeMake(body->patch_per_mesh_instance, body->instance_count, 1)
          threadsPerObjectThreadgroup:MTLSizeMake(body->threads_per_patch, body->patch_per_group, 1)
            threadsPerMeshThreadgroup:MTLSizeMake(body->threads_per_patch, body->patch_per_group, 1)];
      break;
    }
    case WMTRenderCommandDXMTTessellationMeshDispatch: {
      struct wmtcmd_render_dxmt_tess_mesh_dispatch *body = (struct wmtcmd_render_dxmt_tess_mesh_dispatch *)next;
      [encoder setObjectBufferOffset:body->draw_arguments_offset atIndex:21];
      [encoder setMeshBuffer:(id<MTLBuffer>)body->control_point_buffer
                      offset:body->control_point_buffer_offset
                     atIndex:20];
      [encoder setMeshBuffer:(id<MTLBuffer>)body->patch_constant_buffer
                      offset:body->patch_constant_buffer_offset
                     atIndex:21];
      [encoder setMeshBuffer:(id<MTLBuffer>)body->tessellation_factor_buffer
                      offset:body->tessellation_factor_buffer_offset
                     atIndex:22];
      [encoder drawMeshThreadgroups:MTLSizeMake(body->patch_per_mesh_instance, body->instance_count, 1)
          threadsPerObjectThreadgroup:MTLSizeMake(body->threads_per_patch, body->patch_per_group, 1)
            threadsPerMeshThreadgroup:MTLSizeMake(body->threads_per_patch, body->patch_per_group, 1)];
      break;
    }
    case WMTRenderCommandDXMTGeometryDraw: {
      struct wmtcmd_render_dxmt_geometry_draw *body = (struct wmtcmd_render_dxmt_geometry_draw *)next;
      [encoder setObjectBufferOffset:body->draw_arguments_offset atIndex:21];
      [encoder drawMeshThreadgroups:MTLSizeMake(body->warp_count, body->instance_count, 1)
          threadsPerObjectThreadgroup:MTLSizeMake(body->vertex_per_warp, 1, 1)
            threadsPerMeshThreadgroup:MTLSizeMake(1, 1, 1)];
      break;
    }
    case WMTRenderCommandDXMTGeometryDrawIndexed: {
      struct wmtcmd_render_dxmt_geometry_draw_indexed *body = (struct wmtcmd_render_dxmt_geometry_draw_indexed *)next;
      [encoder setObjectBuffer:(id<MTLBuffer>)body->index_buffer offset:body->index_buffer_offset atIndex:20];
      [encoder setObjectBufferOffset:body->draw_arguments_offset atIndex:21];
      [encoder drawMeshThreadgroups:MTLSizeMake(body->warp_count, body->instance_count, 1)
          threadsPerObjectThreadgroup:MTLSizeMake(body->vertex_per_warp, 1, 1)
            threadsPerMeshThreadgroup:MTLSizeMake(1, 1, 1)];
      break;
    }
    case WMTRenderCommandDXMTGeometryDrawIndirect: {
      struct wmtcmd_render_dxmt_geometry_draw_indirect *body = (struct wmtcmd_render_dxmt_geometry_draw_indirect *)next;
      [encoder setObjectBuffer:(id<MTLBuffer>)body->indirect_args_buffer offset:body->indirect_args_offset atIndex:21];
      [encoder drawMeshThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)body->dispatch_args_buffer
                                 indirectBufferOffset:body->dispatch_args_offset
                          threadsPerObjectThreadgroup:MTLSizeMake(body->vertex_per_warp, 1, 1)
                            threadsPerMeshThreadgroup:MTLSizeMake(1, 1, 1)];
      [encoder setObjectBuffer:(id<MTLBuffer>)body->dispatch_args_buffer offset:0 atIndex:21];
      break;
    }
    case WMTRenderCommandDXMTGeometryDrawIndexedIndirect: {
      struct wmtcmd_render_dxmt_geometry_draw_indexed_indirect *body =
          (struct wmtcmd_render_dxmt_geometry_draw_indexed_indirect *)next;
      [encoder setObjectBuffer:(id<MTLBuffer>)body->index_buffer offset:body->index_buffer_offset atIndex:20];
      [encoder setObjectBuffer:(id<MTLBuffer>)body->indirect_args_buffer offset:body->indirect_args_offset atIndex:21];
      [encoder drawMeshThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)body->dispatch_args_buffer
                                 indirectBufferOffset:body->dispatch_args_offset
                          threadsPerObjectThreadgroup:MTLSizeMake(body->vertex_per_warp, 1, 1)
                            threadsPerMeshThreadgroup:MTLSizeMake(1, 1, 1)];
      [encoder setObjectBuffer:(id<MTLBuffer>)body->dispatch_args_buffer offset:0 atIndex:21];
      break;
    }
    }
    next = next->next.ptr;
  }
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_pixelFormat(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLTexture>)params->handle pixelFormat];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_width(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLTexture>)params->handle width];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_height(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLTexture>)params->handle height];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_depth(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLTexture>)params->handle depth];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_arrayLength(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLTexture>)params->handle arrayLength];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_mipmapLevelCount(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLTexture>)params->handle mipmapLevelCount];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLTexture_replaceRegion(void *obj) {
  struct unixcall_mtltexture_replaceregion *params = obj;
  [(id<MTLTexture>)params->texture replaceRegion:MTLRegionMake3D(
                                                     params->origin.x, params->origin.y, params->origin.z,
                                                     params->size.width, params->size.height, params->size.depth
                                                 )
                                     mipmapLevel:params->level
                                           slice:params->slice
                                       withBytes:params->data.ptr
                                     bytesPerRow:params->bytes_per_row
                                   bytesPerImage:params->bytes_per_image];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLBuffer_didModifyRange(void *obj) {
  struct unixcall_generic_obj_uint64_uint64_ret *params = obj;
  [(id<MTLBuffer>)params->handle didModifyRange:NSMakeRange(params->arg, params->ret)];
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
    &_MTLDevice_newLibrary,
    &_MTLLibrary_newFunction,
    &_NSString_lengthOfBytesUsingEncoding,
    &_NSError_description,
    &_MTLDevice_newComputePipelineState,
    &_MTLCommandBuffer_blitCommandEncoder,
    &_MTLCommandBuffer_computeCommandEncoder,
    &_MTLCommandBuffer_renderCommandEncoder,
    &_MTLCommandEncoder_endEncoding,
    &_MTLDevice_newRenderPipelineState,
    &_MTLDevice_newMeshRenderPipelineState,
    &_MTLBlitCommandEncoder_encodeCommands,
    &_MTLComputeCommandEncoder_encodeCommands,
    &_MTLRenderCommandEncoder_encodeCommands,
    &_MTLTexture_pixelFormat,
    &_MTLTexture_width,
    &_MTLTexture_height,
    &_MTLTexture_depth,
    &_MTLTexture_arrayLength,
    &_MTLTexture_mipmapLevelCount,
    &_MTLTexture_replaceRegion,
    &_MTLBuffer_didModifyRange,
};

const unsigned int __winemetal_unixcalls_num = sizeof(__winemetal_unixcalls) / sizeof(void *);
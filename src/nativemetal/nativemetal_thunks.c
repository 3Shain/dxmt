#include <dlfcn.h>
#import <Cocoa/Cocoa.h>
#import <ColorSync/ColorSync.h>
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#import <QuartzCore/QuartzCore.h>
#include "objc/objc-runtime.h"
#define WINEMETAL_API
#include "nativemetal.h"

void
execute_on_main(dispatch_block_t block) {
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
}

WINEMETAL_API void NSObject_retain(obj_handle_t obj)
{
  [(NSObject *)obj retain];
}

WINEMETAL_API void NSObject_release(obj_handle_t obj)
{
  [(NSObject *)obj release];
}

WINEMETAL_API obj_handle_t NSArray_object(obj_handle_t array, uint64_t index)
{
  return (obj_handle_t)[(NSArray *)array objectAtIndex:index];
}

WINEMETAL_API uint64_t NSArray_count(obj_handle_t array)
{
  return [(NSArray *)array count];
}

WINEMETAL_API obj_handle_t WMTCopyAllDevices(void)
{
  return (obj_handle_t)MTLCopyAllDevices();
}

WINEMETAL_API uint64_t MTLDevice_recommendedMaxWorkingSetSize(obj_handle_t device)
{
  return [(id<MTLDevice>)device recommendedMaxWorkingSetSize];
}

WINEMETAL_API uint64_t MTLDevice_currentAllocatedSize(obj_handle_t device)
{
  return [(id<MTLDevice>)device currentAllocatedSize];
}

WINEMETAL_API obj_handle_t MTLDevice_name(obj_handle_t device)
{
  return (obj_handle_t)[(id<MTLDevice>)device name];
}

WINEMETAL_API uint32_t NSString_getCString(obj_handle_t str, char *buffer, uint64_t maxLength, enum WMTStringEncoding encoding)
{
  return (uint32_t)[(NSString *)str getCString:buffer
                                    maxLength:maxLength
                                    encoding:encoding];
}

WINEMETAL_API obj_handle_t MTLDevice_newCommandQueue(obj_handle_t device, uint64_t maxCommandBufferCount)
{
  return (obj_handle_t)[(id<MTLDevice>)device newCommandQueueWithMaxCommandBufferCount:maxCommandBufferCount];
}

WINEMETAL_API obj_handle_t NSAutoreleasePool_alloc_init(void)
{
  return (obj_handle_t)[[NSAutoreleasePool alloc] init];
}

WINEMETAL_API obj_handle_t MTLCommandQueue_commandBuffer(obj_handle_t queue)
{
  return (obj_handle_t)[(id<MTLCommandQueue>)queue commandBuffer];
}

WINEMETAL_API void MTLCommandBuffer_commit(obj_handle_t cmdbuf)
{
  [(id<MTLCommandBuffer>)cmdbuf commit];
}

WINEMETAL_API void MTLCommandBuffer_waitUntilCompleted(obj_handle_t cmdbuf)
{
  [(id<MTLCommandBuffer>)cmdbuf waitUntilCompleted];
}

WINEMETAL_API enum WMTCommandBufferStatus MTLCommandBuffer_status(obj_handle_t cmdbuf)
{
  return (enum WMTCommandBufferStatus)[(id<MTLCommandBuffer>)cmdbuf status];
}

WINEMETAL_API obj_handle_t MTLDevice_newSharedEvent(obj_handle_t device)
{
  return (obj_handle_t)[(id<MTLDevice>)device newSharedEvent];
}

WINEMETAL_API uint64_t MTLSharedEvent_signaledValue(obj_handle_t event)
{
  return [(id<MTLSharedEvent>)event signaledValue];
}

WINEMETAL_API void MTLCommandBuffer_encodeSignalEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value)
{
  [(id<MTLCommandBuffer>)cmdbuf encodeSignalEvent:(id<MTLSharedEvent>)event value:value];
}

WINEMETAL_API obj_handle_t MTLDevice_newBuffer(obj_handle_t device, struct WMTBufferInfo *info)
{
  id<MTLDevice> hDevice = (id<MTLDevice>)device;
  id<MTLBuffer> buffer;
  if (info->memory.ptr) {
    buffer = [hDevice newBufferWithBytesNoCopy:info->memory.ptr
                                        length:info->length
                                       options:(enum MTLResourceOptions)info->options
                                   deallocator:NULL];
  } else {
    buffer = [hDevice newBufferWithLength:info->length options:(enum MTLResourceOptions)info->options];
    info->memory.ptr = [buffer storageMode] == MTLStorageModePrivate ? NULL : [buffer contents];
  }
  info->gpu_address = [buffer gpuAddress];
  return (obj_handle_t)buffer;
}

WINEMETAL_API obj_handle_t MTLDevice_newSamplerState(obj_handle_t device, struct WMTSamplerInfo *info)
{
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

  id<MTLSamplerState> sampler = [(id<MTLDevice>)device newSamplerStateWithDescriptor:sampler_desc];
  info->gpu_resource_id = info->support_argument_buffers ? [sampler gpuResourceID]._impl : 0;
  [sampler_desc release];
  return (obj_handle_t)sampler;
}

WINEMETAL_API obj_handle_t MTLDevice_newDepthStencilState(obj_handle_t device, const struct WMTDepthStencilInfo *info)
{
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

  obj_handle_t ret = (obj_handle_t)[(id<MTLDevice>)device newDepthStencilStateWithDescriptor:desc];
  [desc release];
  return ret;
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

WINEMETAL_API obj_handle_t MTLDevice_newTexture(obj_handle_t device, struct WMTTextureInfo *info)
{
  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  fill_texture_descriptor(desc, info);

  id<MTLTexture> ret = [(id<MTLDevice>)device newTextureWithDescriptor:desc];
  info->gpu_resource_id = [ret gpuResourceID]._impl;
  [desc release];
  return (obj_handle_t)ret;
}

WINEMETAL_API obj_handle_t MTLBuffer_newTexture(obj_handle_t buffer, struct WMTTextureInfo *info,
                                                uint64_t offset, uint64_t bytes_per_row)
{
  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  fill_texture_descriptor(desc, info);

  id<MTLTexture> ret = [(id<MTLBuffer>)buffer newTextureWithDescriptor:desc offset:offset bytesPerRow:bytes_per_row];
  info->gpu_resource_id = [ret gpuResourceID]._impl;

  [desc release];
  return (obj_handle_t)ret;
}

WINEMETAL_API obj_handle_t MTLTexture_newTextureView(obj_handle_t texture, enum WMTPixelFormat format, enum WMTTextureType texture_type, uint16_t level_start,
                                                     uint16_t level_count, uint16_t slice_start, uint16_t slice_count, struct WMTTextureSwizzleChannels swizzle,
                                                     uint64_t *out_gpu_resource_id
)
{
  id<MTLTexture> ret = [(id<MTLTexture>)texture
      newTextureViewWithPixelFormat:(MTLPixelFormat)format
                        textureType:(MTLTextureType)texture_type
                             levels:NSMakeRange(level_start, level_count)
                             slices:NSMakeRange(slice_start, slice_count)
                            swizzle:MTLTextureSwizzleChannelsMake(
                                        (MTLTextureSwizzle)swizzle.r, (MTLTextureSwizzle)swizzle.g,
                                        (MTLTextureSwizzle)swizzle.b, (MTLTextureSwizzle)swizzle.a
                                    )];
  *out_gpu_resource_id = [ret gpuResourceID]._impl;
  return (obj_handle_t)ret;
}

WINEMETAL_API uint64_t
MTLDevice_minimumLinearTextureAlignmentForPixelFormat(obj_handle_t device, enum WMTPixelFormat format)
{
  return [(id<MTLDevice>)device minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)format];
}

WINEMETAL_API obj_handle_t MTLDevice_newLibrary(
    obj_handle_t device, struct WMTMemoryPointer bytecode, uint64_t bytecode_length, obj_handle_t *err_out
)
{
  dispatch_data_t data = dispatch_data_create(bytecode.ptr, bytecode_length, NULL, NULL);
  NSError *err = NULL;
  obj_handle_t ret_library = (obj_handle_t)[(id<MTLDevice>)device newLibraryWithData:data error:&err];
  *err_out = (obj_handle_t)err;
  dispatch_release(data);
  return ret_library;
}

WINEMETAL_API obj_handle_t MTLLibrary_newFunction(obj_handle_t library, const char *name)
{
  NSString *ns_name = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
  obj_handle_t ret = (obj_handle_t)[(id<MTLLibrary>)library newFunctionWithName:ns_name];
  [ns_name release];
  return ret;
}

WINEMETAL_API uint64_t NSString_lengthOfBytesUsingEncoding(obj_handle_t str, enum WMTStringEncoding encoding)
{
  return (uint64_t)[(NSString *)str lengthOfBytesUsingEncoding:(NSStringEncoding)encoding];
}

WINEMETAL_API obj_handle_t NSObject_description(obj_handle_t nserror)
{
  return (obj_handle_t)[(NSObject *)nserror description];
}

WINEMETAL_API obj_handle_t
MTLDevice_newComputePipelineState(obj_handle_t device, obj_handle_t function, obj_handle_t *err_out)
{
  NSError *err = NULL;
  obj_handle_t ret_pso = (obj_handle_t)[(id<MTLDevice>)device newComputePipelineStateWithFunction:(id<MTLFunction>)function error:&err];
  *err_out = (obj_handle_t)err;
  return ret_pso;
}

WINEMETAL_API obj_handle_t MTLCommandBuffer_blitCommandEncoder(obj_handle_t cmdbuf)
{
  return (obj_handle_t)[(id<MTLCommandBuffer>)cmdbuf blitCommandEncoder];
}

WINEMETAL_API obj_handle_t MTLCommandBuffer_computeCommandEncoder(obj_handle_t cmdbuf, bool concurrent)
{
  return (obj_handle_t)[(id<MTLCommandBuffer>)cmdbuf
      computeCommandEncoderWithDispatchType:concurrent ? MTLDispatchTypeConcurrent : MTLDispatchTypeSerial];
}

WINEMETAL_API obj_handle_t MTLCommandBuffer_renderCommandEncoder(obj_handle_t cmdbuf, struct WMTRenderPassInfo *info)
{
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

  obj_handle_t ret = (obj_handle_t)[(id<MTLCommandBuffer>)cmdbuf renderCommandEncoderWithDescriptor:descriptor];

  [descriptor release];
  return ret;
}

WINEMETAL_API void MTLCommandEncoder_endEncoding(obj_handle_t encoder)
{
  [(id<MTLCommandEncoder>)encoder endEncoding];
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

WINEMETAL_API obj_handle_t
MTLDevice_newRenderPipelineState(obj_handle_t device, const struct WMTRenderPipelineInfo *info, obj_handle_t *err_out)
{
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
  obj_handle_t ret_pso =
      (obj_handle_t)[(id<MTLDevice>)device newRenderPipelineStateWithDescriptor:descriptor error:&err];
  *err_out = (obj_handle_t)err;
  [descriptor release];
  return ret_pso;
}

WINEMETAL_API obj_handle_t MTLDevice_newMeshRenderPipelineState(
    obj_handle_t device, const struct WMTMeshRenderPipelineInfo *info, obj_handle_t *err_out
)
{
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
  obj_handle_t ret_pso =
      (obj_handle_t)[(id<MTLDevice>)device newRenderPipelineStateWithMeshDescriptor:descriptor
                                                                            options:MTLPipelineOptionNone
                                                                         reflection:nil
                                                                              error:&err];
  *err_out = (obj_handle_t)err;
  [descriptor release];
  return ret_pso;
}

WINEMETAL_API void MTLBlitCommandEncoder_encodeCommands(obj_handle_t obj_encoder, const struct wmtcmd_base *cmd_head)
{
  id<MTLBlitCommandEncoder> encoder = (id<MTLBlitCommandEncoder>)obj_encoder;
  const struct wmtcmd_base *next = cmd_head;
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
}

WINEMETAL_API void MTLComputeCommandEncoder_encodeCommands(obj_handle_t obj_encoder, const struct wmtcmd_base *cmd_head)
{
  const struct wmtcmd_base *next = cmd_head;
  id<MTLComputeCommandEncoder> encoder = (id<MTLComputeCommandEncoder>)obj_encoder;
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
}

WINEMETAL_API void MTLRenderCommandEncoder_encodeCommands(obj_handle_t obj_encoder, const struct wmtcmd_base *cmd_head)
{
  const struct wmtcmd_base *next = cmd_head;
  id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)obj_encoder;
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
    case WMTRenderCommandSetFragmentBytes: {
      struct wmtcmd_render_setbytes *body = (struct wmtcmd_render_setbytes *)next;
      [encoder setFragmentBytes:body->bytes.ptr length:body->length atIndex:body->index];
      break;
    }
    case WMTRenderCommandSetFragmentTexture: {
      struct wmtcmd_render_settexture *body = (struct wmtcmd_render_settexture *)next;
      [encoder setFragmentTexture:(id<MTLTexture>)body->texture atIndex:body->index];
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
}

WINEMETAL_API enum WMTPixelFormat MTLTexture_pixelFormat(obj_handle_t texture)
{
  return (enum WMTPixelFormat)[(id<MTLTexture>)texture pixelFormat];
}

WINEMETAL_API uint64_t MTLTexture_width(obj_handle_t texture)
{
  return [(id<MTLTexture>)texture width];
}

WINEMETAL_API uint64_t MTLTexture_height(obj_handle_t texture)
{
  return [(id<MTLTexture>)texture height];
}

WINEMETAL_API uint64_t MTLTexture_depth(obj_handle_t texture)
{
  return [(id<MTLTexture>)texture depth];
}

WINEMETAL_API uint64_t MTLTexture_arrayLength(obj_handle_t texture)
{
  return [(id<MTLTexture>)texture arrayLength];
}

WINEMETAL_API uint64_t MTLTexture_mipmapLevelCount(obj_handle_t texture)
{
  return [(id<MTLTexture>)texture mipmapLevelCount];
}

WINEMETAL_API void MTLTexture_replaceRegion(
    obj_handle_t texture, struct WMTOrigin origin, struct WMTSize size, uint64_t level, uint64_t slice,
    struct WMTMemoryPointer data, uint64_t bytes_per_row, uint64_t bytes_per_image
)
{
  [(id<MTLTexture>)texture replaceRegion:MTLRegionMake3D(
                                                     origin.x, origin.y, origin.z,
                                                     size.width, size.height, size.depth
                                                 )
                                     mipmapLevel:level
                                           slice:slice
                                       withBytes:data.ptr
                                     bytesPerRow:bytes_per_row
                                   bytesPerImage:bytes_per_image];
}

WINEMETAL_API void MTLBuffer_didModifyRange(obj_handle_t buffer, uint64_t start, uint64_t length)
{
  [(id<MTLBuffer>)buffer didModifyRange:NSMakeRange(start, length)];
}

WINEMETAL_API void MTLCommandBuffer_presentDrawable(obj_handle_t cmdbuf, obj_handle_t drawable)
{
  [(id<MTLCommandBuffer>)cmdbuf presentDrawable:(id<MTLDrawable>)drawable];
}

WINEMETAL_API void
MTLCommandBuffer_presentDrawableAfterMinimumDuration(obj_handle_t cmdbuf, obj_handle_t drawable, double after)
{
  [(id<MTLCommandBuffer>)cmdbuf presentDrawable:(id<MTLDrawable>)drawable
                           afterMinimumDuration:after];
}

WINEMETAL_API bool MTLDevice_supportsFamily(obj_handle_t device, enum WMTGPUFamily gpu_family)
{
  return [(id<MTLDevice>)device supportsFamily:(MTLGPUFamily)gpu_family];
}

WINEMETAL_API bool MTLDevice_supportsBCTextureCompression(obj_handle_t device)
{
  return [(id<MTLDevice>)device supportsBCTextureCompression];
}

WINEMETAL_API bool MTLDevice_supportsTextureSampleCount(obj_handle_t device, uint8_t sample_count)
{
  return [(id<MTLDevice>)device supportsTextureSampleCount:sample_count];
}

WINEMETAL_API bool MTLDevice_hasUnifiedMemory(obj_handle_t device)
{
  return [(id<MTLDevice>)device hasUnifiedMemory];
}

WINEMETAL_API obj_handle_t MTLCaptureManager_sharedCaptureManager(void)
{
  return (obj_handle_t)[MTLCaptureManager sharedCaptureManager];
}

WINEMETAL_API bool MTLCaptureManager_startCapture(obj_handle_t mgr, struct WMTCaptureInfo *info)
{
  MTLCaptureDescriptor *desc = [[MTLCaptureDescriptor alloc] init];
  desc.destination = (MTLCaptureDestination)info->destination;
  desc.captureObject = (id)info->capture_object;
  NSString *path_str = [[NSString alloc] initWithCString:info->output_url.ptr encoding:NSUTF8StringEncoding];
  NSURL *url = [[NSURL alloc] initFileURLWithPath:path_str];
  desc.outputURL = url;
  [(MTLCaptureManager *)mgr startCaptureWithDescriptor:desc error:nil];
  [url release];
  [path_str release];
  [desc release];
  return true; /** @todo */
}

WINEMETAL_API void MTLCaptureManager_stopCapture(obj_handle_t mgr)
{
  [(MTLCaptureManager *)mgr stopCapture];
}

#include <signal.h>

void
temp_handler(int signum) {
  printf("received signal %d in temp_handler(), and it may cause problem!\n", signum);
}

static const int SIGNALS[] = {
    SIGHUP,
    SIGINT,
    SIGTERM,
    SIGUSR2,
    SIGILL,
    SIGTRAP,
    SIGABRT,
    SIGFPE,
    SIGBUS,
    SIGSEGV,
    SIGQUIT
#ifdef SIGSYS
    ,
    SIGSYS
#endif
#ifdef SIGXCPU
    ,
    SIGXCPU
#endif
#ifdef SIGXFSZ
    ,
    SIGXFSZ
#endif
#ifdef SIGEMT
    ,
    SIGEMT
#endif
    ,
    SIGUSR1
#ifdef SIGINFO
    ,
    SIGINFO
#endif
};

WINEMETAL_API obj_handle_t MTLDevice_newTemporalScaler(obj_handle_t device, const struct WMTFXTemporalScalerInfo *info)
{
  MTLFXTemporalScalerDescriptor *desc = [[MTLFXTemporalScalerDescriptor alloc] init];
  desc.colorTextureFormat = (MTLPixelFormat)info->color_format;
  desc.outputTextureFormat = (MTLPixelFormat)info->output_format;
  desc.depthTextureFormat = (MTLPixelFormat)info->depth_format;
  desc.motionTextureFormat = (MTLPixelFormat)info->motion_format;
  desc.inputWidth = info->input_width;
  desc.inputHeight = info->input_height;
  desc.outputWidth = info->output_width;
  desc.outputHeight = info->output_height;
  desc.inputContentMaxScale = info->input_content_max_scale;
  desc.inputContentMinScale = info->input_content_min_scale;
  desc.inputContentPropertiesEnabled = info->input_content_properties_enabled;
  if (@available(macOS 15, *)) {
    desc.requiresSynchronousInitialization = info->requires_synchronous_initialization;
  }
  desc.autoExposureEnabled = info->auto_exposure;

  struct sigaction old_action[sizeof(SIGNALS) / sizeof(int)], new_action;
  new_action.sa_handler = temp_handler;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;

  for (unsigned int i = 0; i < sizeof(SIGNALS) / sizeof(int); i++)
    sigaction(SIGNALS[i], &new_action, &old_action[i]);

  obj_handle_t ret = (obj_handle_t)[desc newTemporalScalerWithDevice:(id<MTLDevice>)device];

  for (unsigned int i = 0; i < sizeof(SIGNALS) / sizeof(int); i++)
    sigaction(SIGNALS[i], &old_action[i], NULL);

  [desc release];
  return ret;
}

WINEMETAL_API obj_handle_t MTLDevice_newSpatialScaler(obj_handle_t device, const struct WMTFXSpatialScalerInfo *info)
{
  MTLFXSpatialScalerDescriptor *desc = [[MTLFXSpatialScalerDescriptor alloc] init];
  desc.colorTextureFormat = (MTLPixelFormat)info->color_format;
  desc.outputTextureFormat = (MTLPixelFormat)info->output_format;
  desc.inputWidth = info->input_width;
  desc.inputHeight = info->input_height;
  desc.outputWidth = info->output_width;
  desc.outputHeight = info->output_height;
  obj_handle_t ret = (obj_handle_t)[desc newSpatialScalerWithDevice:(id<MTLDevice>)device];
  [desc release];
  return ret;
}

WINEMETAL_API void MTLCommandBuffer_encodeTemporalScale(
    obj_handle_t obj_cmdbuf, obj_handle_t obj_scaler, obj_handle_t obj_color, obj_handle_t obj_output, obj_handle_t obj_depth,
    obj_handle_t obj_motion, obj_handle_t obj_exposure, const struct WMTFXTemporalScalerProps *props
)
{
  id<MTLCommandBuffer> cmdbuf = (id<MTLCommandBuffer>)obj_cmdbuf;
  id<MTLFXTemporalScaler> scaler = (id<MTLFXTemporalScaler>)obj_scaler;
  scaler.colorTexture = (id<MTLTexture>)obj_color;
  scaler.outputTexture = (id<MTLTexture>)obj_output;
  scaler.depthTexture = (id<MTLTexture>)obj_depth;
  scaler.motionTexture = (id<MTLTexture>)obj_motion;
  scaler.exposureTexture = (id<MTLTexture>)obj_exposure;
  scaler.inputContentWidth = props->input_content_width;
  scaler.inputContentHeight = props->input_content_height;
  scaler.reset = props->reset;
  scaler.depthReversed = props->depth_reversed;
  scaler.motionVectorScaleX = props->motion_vector_scale_x;
  scaler.motionVectorScaleY = props->motion_vector_scale_y;
  scaler.jitterOffsetX = props->jitter_offset_x;
  scaler.jitterOffsetY = props->jitter_offset_y;
  scaler.preExposure = props->pre_exposure;
  [scaler encodeToCommandBuffer:cmdbuf];
}

WINEMETAL_API void
MTLCommandBuffer_encodeSpatialScale(obj_handle_t obj_cmdbuf, obj_handle_t obj_scaler, obj_handle_t obj_color, obj_handle_t obj_output)
{
  id<MTLCommandBuffer> cmdbuf = (id<MTLCommandBuffer>)obj_cmdbuf;
  id<MTLFXSpatialScaler> scaler = (id<MTLFXSpatialScaler>)obj_scaler;
  scaler.colorTexture = (id<MTLTexture>)obj_color;
  scaler.outputTexture = (id<MTLTexture>)obj_output;
  [scaler encodeToCommandBuffer:cmdbuf];
}

WINEMETAL_API obj_handle_t NSString_string(const char *data, enum WMTStringEncoding encoding)
{
  return (obj_handle_t)[NSString stringWithCString:data encoding:(NSStringEncoding)encoding];
}

WINEMETAL_API obj_handle_t NSString_alloc_init(const char *data, enum WMTStringEncoding encoding)
{
  return (obj_handle_t)[[NSString alloc] initWithCString:data encoding:(NSStringEncoding)encoding];
}

WINEMETAL_API obj_handle_t DeveloperHUDProperties_instance()
{
  return (obj_handle_t)((id(*)(id, SEL))objc_msgSend)(objc_lookUpClass("_CADeveloperHUDProperties"), @selector(instance));
}

WINEMETAL_API bool DeveloperHUDProperties_addLabel(obj_handle_t obj, obj_handle_t label, obj_handle_t after)
{
  return ((bool (*)(id, SEL, id, id)
  )objc_msgSend)((id)obj, @selector(addLabel:after:), (id)label, (id)after);
}

WINEMETAL_API void DeveloperHUDProperties_updateLabel(obj_handle_t obj, obj_handle_t label, obj_handle_t value)
{
  ((void (*)(id, SEL, id, id)
  )objc_msgSend)((id)obj, @selector(updateLabel:value:), (id)label, (id)value);
}

WINEMETAL_API void DeveloperHUDProperties_remove(obj_handle_t obj, obj_handle_t label)
{
  ((void (*)(id, SEL, id))objc_msgSend)((id)obj, @selector(remove:), (id)label);
}

WINEMETAL_API obj_handle_t MetalDrawable_texture(obj_handle_t drawable)
{
  return (obj_handle_t)[(id<CAMetalDrawable>)drawable texture];
}

WINEMETAL_API obj_handle_t MetalLayer_nextDrawable(obj_handle_t layer)
{
  return (obj_handle_t)[(CAMetalLayer *)layer nextDrawable];
}

WINEMETAL_API bool MTLDevice_supportsFXSpatialScaler(obj_handle_t device)
{
  return [MTLFXSpatialScalerDescriptor supportsDevice:(id<MTLDevice>)device];
}

WINEMETAL_API bool MTLDevice_supportsFXTemporalScaler(obj_handle_t device)
{
  return [MTLFXTemporalScalerDescriptor supportsDevice:(id<MTLDevice>)device];
}

WINEMETAL_API void MetalLayer_setProps(obj_handle_t obj_layer, const struct WMTLayerProps *props)
{
  CAMetalLayer *layer = (CAMetalLayer *)obj_layer;
  execute_on_main(^{
    layer.device = (id<MTLDevice>)props->device;
    layer.opaque = props->opaque;
    layer.framebufferOnly = props->framebuffer_only;
    layer.contentsScale = props->contents_scale;
    layer.displaySyncEnabled = props->display_sync_enabled;
    layer.drawableSize = CGSizeMake(props->drawable_width, props->drawable_height);
    layer.pixelFormat = (MTLPixelFormat)props->pixel_format;
  });
}

WINEMETAL_API void MetalLayer_getProps(obj_handle_t obj_layer, struct WMTLayerProps *props)
{
  CAMetalLayer *layer = (CAMetalLayer *)obj_layer;
  props->device = (obj_handle_t)layer.device;
  props->opaque = layer.opaque;
  props->framebuffer_only = layer.framebufferOnly;
  props->contents_scale = layer.contentsScale;
  props->display_sync_enabled = layer.displaySyncEnabled;
  props->drawable_height = layer.drawableSize.height;
  props->drawable_width = layer.drawableSize.width;
  props->pixel_format = layer.pixelFormat;
}

#if 0
typedef struct macdrv_opaque_metal_device *macdrv_metal_device;
typedef struct macdrv_opaque_metal_view *macdrv_metal_view;
typedef struct macdrv_opaque_metal_layer *macdrv_metal_layer;
typedef struct macdrv_opaque_view *macdrv_view;
typedef struct macdrv_opaque_window *macdrv_window;
typedef struct macdrv_opaque_window_data *macdrv_window_data;
typedef struct opaque_window_surface *window_surface;
typedef struct opaque_HWND *HWND;
struct macdrv_win_data {
  HWND hwnd; /* hwnd that this private data belongs to */
  macdrv_window cocoa_window;
  macdrv_view cocoa_view;
  macdrv_view client_cocoa_view;
};

struct macdrv_functions_t {
  void (*macdrv_init_display_devices)(BOOL);
  struct macdrv_win_data *(*get_win_data)(HWND hwnd);
  void (*release_win_data)(struct macdrv_win_data *data);
  macdrv_window (*macdrv_get_cocoa_window)(HWND hwnd, BOOL require_on_screen);
  macdrv_metal_device (*macdrv_create_metal_device)(void);
  void (*macdrv_release_metal_device)(macdrv_metal_device d);
  macdrv_metal_view (*macdrv_view_create_metal_view)(macdrv_view v, macdrv_metal_device d);
  macdrv_metal_layer (*macdrv_view_get_metal_layer)(macdrv_metal_view v);
  void (*macdrv_view_release_metal_view)(macdrv_metal_view v);
  void (*on_main_thread)(dispatch_block_t b);
};
#endif

WINEMETAL_API obj_handle_t CreateMetalViewFromHWND(intptr_t hwnd, obj_handle_t device, obj_handle_t *layer)
{
#if 0 /** @todo */
  struct macdrv_win_data *(*pfn_get_win_data)(HWND hwnd) = NULL;
  void (*pfn_release_win_data)(struct macdrv_win_data *data) = NULL;
  macdrv_metal_view (*pfn_macdrv_view_create_metal_view)(macdrv_view v, macdrv_metal_device d) = NULL;
  macdrv_metal_layer (*pfn_macdrv_view_get_metal_layer)(macdrv_metal_view v) = NULL;

  struct macdrv_functions_t *macdrv_functions;
  if ((macdrv_functions = dlsym(RTLD_DEFAULT, "macdrv_functions"))) {
    pfn_get_win_data = macdrv_functions->get_win_data;
    pfn_release_win_data = macdrv_functions->release_win_data;
    pfn_macdrv_view_create_metal_view = macdrv_functions->macdrv_view_create_metal_view;
    pfn_macdrv_view_get_metal_layer = macdrv_functions->macdrv_view_get_metal_layer;
  } else {
    pfn_get_win_data = dlsym(RTLD_DEFAULT, "get_win_data");
    pfn_release_win_data = dlsym(RTLD_DEFAULT, "release_win_data");
    pfn_macdrv_view_create_metal_view = dlsym(RTLD_DEFAULT, "macdrv_view_create_metal_view");
    pfn_macdrv_view_get_metal_layer = dlsym(RTLD_DEFAULT, "macdrv_view_get_metal_layer");
  }

  if (pfn_get_win_data && pfn_release_win_data && pfn_macdrv_view_create_metal_view &&
      pfn_macdrv_view_get_metal_layer) {
    struct macdrv_win_data *win_data = pfn_get_win_data((HWND)params->hwnd);
    macdrv_metal_view view =
        pfn_macdrv_view_create_metal_view(win_data->client_cocoa_view, (macdrv_metal_device)params->device);
    params->ret_view = (obj_handle_t)view;
    if (view) {
      params->ret_layer = (obj_handle_t)pfn_macdrv_view_get_metal_layer(view);
    }
    pfn_release_win_data(win_data);
  }

  return STATUS_SUCCESS;
#else
  return 0;
#endif
}

WINEMETAL_API void ReleaseMetalView(obj_handle_t view)
{
#if 0 /** @todo */
  void (*pfn_macdrv_view_release_metal_view)(macdrv_metal_view v) = NULL;

  struct macdrv_functions_t *macdrv_functions;
  if ((macdrv_functions = dlsym(RTLD_DEFAULT, "macdrv_functions"))) {
    pfn_macdrv_view_release_metal_view = macdrv_functions->macdrv_view_release_metal_view;
  } else {
    pfn_macdrv_view_release_metal_view = dlsym(RTLD_DEFAULT, "macdrv_view_release_metal_view");
  }

  if (pfn_macdrv_view_release_metal_view)
    pfn_macdrv_view_release_metal_view((macdrv_metal_view)params->handle);

  return STATUS_SUCCESS;
#else
#endif
}

#if 0 /** @todo */
static NTSTATUS
thunk_SM50Initialize(void *args) {
  struct sm50_initialize_params *params = args;

  params->ret =
      SM50Initialize(params->bytecode, params->bytecode_size, params->shader, params->reflection, params->error);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50Destroy(void *args) {
  struct sm50_destroy_params *params = args;

  SM50Destroy(params->shader);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50Compile(void *args) {
  struct sm50_compile_params *params = args;

  params->ret = SM50Compile(params->shader, params->args, params->func_name, params->bitcode, params->error);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50GetCompiledBitcode(void *args) {
  struct sm50_get_compiled_bitcode_params *params = args;

  SM50GetCompiledBitcode(params->bitcode, params->data_out);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50DestroyBitcode(void *args) {
  struct sm50_destroy_bitcode_params *params = args;

  SM50DestroyBitcode(params->bitcode);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50GetErrorMessage(void *args) {
  struct sm50_get_error_message_params *params = args;

  params->ret_size = SM50GetErrorMessage(params->error, params->buffer, params->buffer_size);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50FreeError(void *args) {
  struct sm50_free_error_params *params = args;

  SM50FreeError(params->error);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50CompileTessellationPipelineVertex(void *args) {
  struct sm50_compile_tessellation_pipeline_vertex_params *params = args;

  params->ret = SM50CompileTessellationPipelineVertex(
      params->vertex, params->hull, params->vertex_args, params->func_name, params->bitcode, params->error
  );

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50CompileTessellationPipelineHull(void *args) {
  struct sm50_compile_tessellation_pipeline_hull_params *params = args;

  params->ret = SM50CompileTessellationPipelineHull(
      params->vertex, params->hull, params->hull_args, params->func_name, params->bitcode, params->error
  );

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50CompileTessellationPipelineDomain(void *args) {
  struct sm50_compile_tessellation_pipeline_domain_params *params = args;

  params->ret = SM50CompileTessellationPipelineDomain(
      params->hull, params->domain, params->domain_args, params->func_name, params->bitcode, params->error
  );

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50CompileGeometryPipelineVertex(void *args) {
  struct sm50_compile_geometry_pipeline_vertex_params *params = args;

  params->ret = SM50CompileGeometryPipelineVertex(
      params->vertex, params->geometry, params->vertex_args, params->func_name, params->bitcode, params->error
  );

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk_SM50CompileGeometryPipelineGeometry(void *args) {
  struct sm50_compile_geometry_pipeline_geometry_params *params = args;

  params->ret = SM50CompileGeometryPipelineGeometry(
      params->vertex, params->geometry, params->geometry_args, params->func_name, params->bitcode, params->error
  );

  return STATUS_SUCCESS;
}
#endif

WINEMETAL_API void MTLCommandEncoder_setLabel(obj_handle_t encoder, obj_handle_t label)
{
  [(id<MTLCommandEncoder>)encoder setLabel:(NSString *)label];
}

WINEMETAL_API void MTLDevice_setShouldMaximizeConcurrentCompilation(obj_handle_t device, bool value)
{
  [(id<MTLDevice>)device setShouldMaximizeConcurrentCompilation:(BOOL)value];
}

#if 0 /** @todo */
static NTSTATUS
thunk_SM50GetArgumentsInfo(void *args) {
  struct sm50_get_arguments_info_params *params = args;
  SM50GetArgumentsInfo(params->shader, params->constant_buffers, params->arguments);
  return STATUS_SUCCESS;
}

inline void *
UInt32ToPtr(uint32_t v) {
  return (void *)(uint64_t)v;
}

static NTSTATUS
thunk32_SM50Initialize(void *args) {
  struct sm50_initialize_params32 *params = args;

  params->ret = SM50Initialize(
      UInt32ToPtr(params->bytecode), params->bytecode_size, UInt32ToPtr(params->shader),
      UInt32ToPtr(params->reflection), UInt32ToPtr(params->error)
  );

  return STATUS_SUCCESS;
}

struct SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA32 {
  uint32_t next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint32_t num_output_slots;
  uint32_t num_elements;
  uint32_t strides[4];
  uint32_t elements;
};

struct SM50_SHADER_DEBUG_IDENTITY_DATA32 {
  uint32_t next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint64_t id;
};

struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 {
  uint32_t next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
};

struct SM50_SHADER_IA_INPUT_LAYOUT_DATA32 {
  uint32_t next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  enum SM50_INDEX_BUFFER_FORAMT index_buffer_format;
  uint32_t slot_mask;
  uint32_t num_elements;
  uint32_t elements;
};

struct SM50_SHADER_PSO_PIXEL_SHADER_DATA32 {
  uint32_t next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint32_t sample_mask;
  bool dual_source_blending;
  bool disable_depth_output;
  uint32_t unorm_output_reg_mask;
};

struct SM50_SHADER_GS_PASS_THROUGH_DATA32 {
  uint32_t next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  union {
    struct MTL_GEOMETRY_SHADER_PASS_THROUGH Data;
    uint32_t DataEncoded;
  };
  bool RasterizationDisabled;
};

struct SM50_SHADER_PSO_GEOMETRY_SHADER_DATA32 {
  uint32_t next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  bool strip_topology;
};

void
sm50_compilation_argument32_convert(
    struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *first_arg, struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 *args32
) {
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *last_arg = first_arg;

  first_arg->type = SM50_SHADER_ARGUMENT_TYPE_MAX;
  first_arg->next = NULL;

  while (args32) {
    switch (args32->type) {
    case SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT: {
      struct SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA32 *src = (void *)args32;
      struct SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA *data =
          malloc(sizeof(struct SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA));
      last_arg->next = data;
      last_arg = (void *)data;
      last_arg->next = NULL;
      data->type = src->type;
      data->num_output_slots = src->num_output_slots;
      data->num_elements = src->num_elements;
      data->strides[0] = src->strides[0];
      data->strides[1] = src->strides[1];
      data->strides[2] = src->strides[2];
      data->strides[3] = src->strides[3];
      data->elements = UInt32ToPtr(src->elements);
      break;
    }
    case SM50_SHADER_DEBUG_IDENTITY: {
      struct SM50_SHADER_DEBUG_IDENTITY_DATA32 *src = (void *)args32;
      struct SM50_SHADER_DEBUG_IDENTITY_DATA *data = malloc(sizeof(struct SM50_SHADER_DEBUG_IDENTITY_DATA));
      last_arg->next = data;
      last_arg = (void *)data;
      last_arg->next = NULL;
      data->type = src->type;
      data->id = src->id;
      break;
    }
    case SM50_SHADER_PSO_PIXEL_SHADER: {
      struct SM50_SHADER_PSO_PIXEL_SHADER_DATA32 *src = (void *)args32;
      struct SM50_SHADER_PSO_PIXEL_SHADER_DATA *data = malloc(sizeof(struct SM50_SHADER_PSO_PIXEL_SHADER_DATA));
      last_arg->next = data;
      last_arg = (void *)data;
      last_arg->next = NULL;
      data->type = src->type;
      data->unorm_output_reg_mask = src->unorm_output_reg_mask;
      data->disable_depth_output = src->disable_depth_output;
      data->sample_mask = src->sample_mask;
      data->dual_source_blending = src->dual_source_blending;
      break;
    }
    case SM50_SHADER_IA_INPUT_LAYOUT: {
      struct SM50_SHADER_IA_INPUT_LAYOUT_DATA32 *src = (void *)args32;
      struct SM50_SHADER_IA_INPUT_LAYOUT_DATA *data = malloc(sizeof(struct SM50_SHADER_IA_INPUT_LAYOUT_DATA));
      last_arg->next = data;
      last_arg = (void *)data;
      last_arg->next = NULL;
      data->type = src->type;
      data->slot_mask = src->slot_mask;
      data->index_buffer_format = src->index_buffer_format;
      data->num_elements = src->num_elements;
      data->elements = UInt32ToPtr(src->elements);
      break;
    }
    case SM50_SHADER_GS_PASS_THROUGH: {
      struct SM50_SHADER_GS_PASS_THROUGH_DATA32 *src = (void *)args32;
      struct SM50_SHADER_GS_PASS_THROUGH_DATA *data = malloc(sizeof(struct SM50_SHADER_GS_PASS_THROUGH_DATA));
      last_arg->next = data;
      last_arg = (void *)data;
      last_arg->next = NULL;
      data->type = src->type;
      data->Data = src->Data;
      data->RasterizationDisabled = src->RasterizationDisabled;
      break;
    }
    case SM50_SHADER_PSO_GEOMETRY_SHADER: {
      struct SM50_SHADER_PSO_GEOMETRY_SHADER_DATA32 *src = (void *)args32;
      struct SM50_SHADER_PSO_GEOMETRY_SHADER_DATA *data = malloc(sizeof(struct SM50_SHADER_PSO_GEOMETRY_SHADER_DATA));
      last_arg->next = data;
      last_arg = (void *)data;
      last_arg->next = NULL;
      data->type = src->type;
      data->strip_topology = src->strip_topology;
      break;
    }
    case SM50_SHADER_ARGUMENT_TYPE_MAX:
      break;
    }
    args32 = UInt32ToPtr(args32->next);
  }
}

void
sm50_compilation_argument32_free(struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *first_arg) {
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *arg = first_arg->next;

  while (arg) {
    struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *next = arg->next;
    free(arg);
    arg = next;
  }
}

static NTSTATUS
thunk32_SM50Compile(void *args) {
  struct sm50_compile_params32 *params = args;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA first_arg;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 *args32 = UInt32ToPtr(params->args);
  sm50_compilation_argument32_convert(&first_arg, args32);

  params->ret = SM50Compile(
      params->shader, &first_arg, UInt32ToPtr(params->func_name), UInt32ToPtr(params->bitcode),
      UInt32ToPtr(params->error)
  );

  sm50_compilation_argument32_free(&first_arg);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50GetCompiledBitcode(void *args) {
  struct sm50_get_compiled_bitcode_params32 *params = args;

  SM50GetCompiledBitcode(params->bitcode, UInt32ToPtr(params->data_out));

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50GetErrorMessage(void *args) {
  struct sm50_get_error_message_params32 *params = args;

  params->ret_size = SM50GetErrorMessage(params->error, UInt32ToPtr(params->buffer), params->buffer_size);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50CompileTessellationPipelineVertex(void *args) {
  struct sm50_compile_tessellation_pipeline_vertex_params32 *params = args;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA first_arg;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 *args32 = UInt32ToPtr(params->vertex_args);
  sm50_compilation_argument32_convert(&first_arg, args32);

  params->ret = SM50CompileTessellationPipelineVertex(
      params->vertex, params->hull, &first_arg, UInt32ToPtr(params->func_name), UInt32ToPtr(params->bitcode),
      UInt32ToPtr(params->error)
  );

  sm50_compilation_argument32_free(&first_arg);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50CompileTessellationPipelineHull(void *args) {
  struct sm50_compile_tessellation_pipeline_hull_params32 *params = args;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA first_arg;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 *args32 = UInt32ToPtr(params->hull_args);
  sm50_compilation_argument32_convert(&first_arg, args32);

  params->ret = SM50CompileTessellationPipelineHull(
      params->vertex, params->hull, &first_arg, UInt32ToPtr(params->func_name), UInt32ToPtr(params->bitcode),
      UInt32ToPtr(params->error)
  );

  sm50_compilation_argument32_free(&first_arg);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50CompileTessellationPipelineDomain(void *args) {
  struct sm50_compile_tessellation_pipeline_domain_params32 *params = args;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA first_arg;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 *args32 = UInt32ToPtr(params->domain_args);
  sm50_compilation_argument32_convert(&first_arg, args32);

  params->ret = SM50CompileTessellationPipelineDomain(
      params->hull, params->domain, &first_arg, UInt32ToPtr(params->func_name), UInt32ToPtr(params->bitcode),
      UInt32ToPtr(params->error)
  );

  sm50_compilation_argument32_free(&first_arg);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50CompileGeometryPipelineVertex(void *args) {
  struct sm50_compile_geometry_pipeline_vertex_params32 *params = args;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA first_arg;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 *args32 = UInt32ToPtr(params->vertex_args);
  sm50_compilation_argument32_convert(&first_arg, args32);

  params->ret = SM50CompileGeometryPipelineVertex(
      params->vertex, params->geometry, &first_arg, UInt32ToPtr(params->func_name), UInt32ToPtr(params->bitcode),
      UInt32ToPtr(params->error)
  );

  sm50_compilation_argument32_free(&first_arg);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50CompileGeometryPipelineGeometry(void *args) {
  struct sm50_compile_geometry_pipeline_geometry_params32 *params = args;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA first_arg;
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA32 *args32 = UInt32ToPtr(params->geometry_args);
  sm50_compilation_argument32_convert(&first_arg, args32);

  params->ret = SM50CompileGeometryPipelineGeometry(
      params->vertex, params->geometry, &first_arg, UInt32ToPtr(params->func_name), UInt32ToPtr(params->bitcode),
      UInt32ToPtr(params->error)
  );

  sm50_compilation_argument32_free(&first_arg);

  return STATUS_SUCCESS;
}

static NTSTATUS
thunk32_SM50GetArgumentsInfo(void *args) {
  struct sm50_get_arguments_info_params32 *params = args;

  SM50GetArgumentsInfo(params->shader, UInt32ToPtr(params->constant_buffers), UInt32ToPtr(params->arguments));

  return STATUS_SUCCESS;
}
#endif

WINEMETAL_API obj_handle_t MTLCommandBuffer_error(obj_handle_t cmdbuf)
{
  return (obj_handle_t)[(id<MTLCommandBuffer>)cmdbuf error];
}

WINEMETAL_API obj_handle_t MTLCommandBuffer_logs(obj_handle_t cmdbuf)
{
  return (obj_handle_t)[(id<MTLCommandBuffer>)cmdbuf logs];
}

WINEMETAL_API uint64_t
MTLLogContainer_enumerate(obj_handle_t logs, uint64_t start, uint64_t buffer_size, obj_handle_t *obj_buffer)
{
  uint64_t count = 0;
  uint64_t read = 0;
  id *buffer = (id *)obj_buffer;
  for (id _ in (id<MTLLogContainer>)logs) {
    if (count >= start) {
      if (count < start + buffer_size) {
        buffer[count - start] = _;
        read++;
      } else {
        break;
      }
    }
    count++;
  }
  return read;
}

CFStringRef
GetColorSpaceName(enum WMTColorSpace colorspace) {
  switch (colorspace) {
  case WMTColorSpaceSRGB:
    return kCGColorSpaceSRGB;
  case WMTColorSpaceSRGBLinear:
  case WMTColorSpaceHDR_scRGB:
    return kCGColorSpaceExtendedLinearSRGB;
  case WMTColorSpaceBT2020:
    return kCGColorSpaceITUR_2020_sRGBGamma;
  case WMTColorSpaceHDR_PQ:
    return kCGColorSpaceITUR_2100_PQ;
  default:
    return nil;
  }
}

WINEMETAL_API bool CGColorSpace_checkColorSpaceSupported(enum WMTColorSpace colorspace)
{
  CFStringRef name = GetColorSpaceName((enum WMTColorSpace)colorspace);
  if (!name)
    return false;
  CGColorSpaceRef ref = CGColorSpaceCreateWithName(name);
  if (!ref)
    return false;
  CGColorSpaceRelease(ref);
  return true;
}

WINEMETAL_API bool MetalLayer_setColorSpace(obj_handle_t obj_layer, enum WMTColorSpace colorspace)
{
  CAMetalLayer *layer = (CAMetalLayer *)obj_layer;
  CFStringRef name = GetColorSpaceName(colorspace);
  if (!name)
    return false;
  CGColorSpaceRef ref = CGColorSpaceCreateWithName(name);
  if (!ref)
    return false;
  execute_on_main(^{
    layer.colorspace = ref;
    layer.wantsExtendedDynamicRangeContent = WMT_COLORSPACE_IS_HDR(colorspace);
    CGColorSpaceRelease(ref);
  });
  return true;
}

WINEMETAL_API uint32_t WMTGetPrimaryDisplayId()
{
  return CGMainDisplayID();
}

WINEMETAL_API uint32_t WMTGetSecondaryDisplayId()
{
  uint32_t count = 0;
  CGGetOnlineDisplayList(0, NULL, &count);

  if (count == 0)
    return kCGNullDirectDisplay;

  CGDirectDisplayID main_display = CGMainDisplayID();
  CGDirectDisplayID displays[count];
  CGGetOnlineDisplayList(count, displays, &count);

  for (uint32_t i = 0; i < count; i++) {
    CGDirectDisplayID id = displays[i];
    if (id == main_display)
      continue;
    if (CGDisplayMirrorsDisplay(id) != kCGNullDirectDisplay)
      continue;
    return id;
  }

  return kCGNullDirectDisplay;
}

typedef struct icc_XYZ_t {
  uint32_t sig;      // 0x205a5958
  uint32_t reserved; // 0
  int32_t x;
  int32_t y;
  int32_t z;
} icc_XYZ_t;

bool
GetChromaticity_xy(ColorSyncProfileRef profile, CFStringRef tag, float *out_x, float *out_y) {
  CFDataRef tag_data = ColorSyncProfileCopyTag(profile, tag);
  if (!tag_data)
    return false;
  if (CFDataGetLength(tag_data) != sizeof(icc_XYZ_t))
    return false;
  icc_XYZ_t *data = (icc_XYZ_t *)CFDataGetBytePtr(tag_data);
  if (data->sig != 0x205a5958)
    return false;
  double X = (int32_t)__builtin_bswap32(data->x) / 65536.0;
  double Y = (int32_t)__builtin_bswap32(data->y) / 65536.0;
  double Z = (int32_t)__builtin_bswap32(data->z) / 65536.0;
  *out_x = X / (X + Y + Z);
  *out_y = Y / (X + Y + Z);
  return true;
}

bool
GetDisplayColorGamut(ColorSyncProfileRef profile, struct WMTDisplayDescription *desc_out) {
  return GetChromaticity_xy(
             profile, kColorSyncSigMediaWhitePointTag, &desc_out->white_points[0], &desc_out->white_points[1]
         ) &&
         GetChromaticity_xy(
             profile, kColorSyncSigRedColorantTag, &desc_out->red_primaries[0], &desc_out->red_primaries[1]
         ) &&
         GetChromaticity_xy(
             profile, kColorSyncSigGreenColorantTag, &desc_out->green_primaries[0], &desc_out->green_primaries[1]
         ) &&
         GetChromaticity_xy(
             profile, kColorSyncSigBlueColorantTag, &desc_out->blue_primaries[0], &desc_out->blue_primaries[1]
         );
}

NSScreen *
GetNSScreenForDisplayID(CGDirectDisplayID display_id) {
  for (NSScreen *screen in [NSScreen screens]) {
    CGDirectDisplayID id = [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
    if (id == display_id) {
      return screen;
    }
  }
  return nil;
}

WINEMETAL_API void WMTGetDisplayDescription(uint32_t in_display_id, struct WMTDisplayDescription *desc)
{
  CGDirectDisplayID display_id = in_display_id;
  struct WMTDisplayDescription *desc_out = desc;
  ColorSyncProfileRef profile = ColorSyncProfileCreateWithDisplayID(display_id);
  if (!profile || !GetDisplayColorGamut(profile, desc_out))
    GetDisplayColorGamut(ColorSyncProfileCreateWithName(kColorSyncGenericRGBProfile), desc_out);
  NSScreen *screen = GetNSScreenForDisplayID(display_id);
  if (screen) {
    desc_out->maximum_edr_color_component_value = [screen maximumExtendedDynamicRangeColorComponentValue];
    desc_out->maximum_reference_edr_color_component_value =
        [screen maximumReferenceExtendedDynamicRangeColorComponentValue];
    desc_out->maximum_potential_edr_color_component_value =
        [screen maximumPotentialExtendedDynamicRangeColorComponentValue];
  } else {
    desc_out->maximum_edr_color_component_value = 1.0;
    desc_out->maximum_reference_edr_color_component_value = 0.0;
    desc_out->maximum_potential_edr_color_component_value = 1.0;
  }
}

struct DisplaySetting {
  uint64_t version;
  enum WMTColorSpace colorspace;
  struct WMTHDRMetadata hdr_metadata;
};

struct DisplaySetting g_display_settings[2] = {{0, 0, {}}, {0, 0, {}}};

WINEMETAL_API void MetalLayer_getEDRValue(obj_handle_t obj_layer, struct WMTEDRValue *value)
{
  CAMetalLayer *layer = (CAMetalLayer *)obj_layer;
  value->maximum_edr_color_component_value = 1.0;
  value->maximum_potential_edr_color_component_value = 1.0;

  if (![layer.delegate isKindOfClass:NSView.class])
    return;

  NSView *view = (NSView *)layer.delegate;
  if (!view.window)
    return;

  if (!view.window.screen)
    return;

  NSScreen *screen = view.window.screen;

  value->maximum_edr_color_component_value =
      layer.wantsExtendedDynamicRangeContent ? screen.maximumExtendedDynamicRangeColorComponentValue : 1.0;
  value->maximum_potential_edr_color_component_value = screen.maximumPotentialExtendedDynamicRangeColorComponentValue;
}

WINEMETAL_API obj_handle_t MTLLibrary_newFunctionWithConstants(
    obj_handle_t obj_library, const char *in_name, const struct WMTFunctionConstant *constants, uint32_t num_constants,
    obj_handle_t *err_out
)
{
  id<MTLLibrary> library = (id<MTLLibrary>)obj_library;
  NSString *name = [[NSString alloc] initWithCString:in_name encoding:NSUTF8StringEncoding];
  NSError *err = NULL;
  MTLFunctionConstantValues *values = [[MTLFunctionConstantValues alloc] init];
  for (uint64_t i = 0; i < num_constants; i++)
    [values setConstantValue:constants[i].data.ptr type:(MTLDataType)constants[i].type atIndex:constants[i].index];

  obj_handle_t ret = (obj_handle_t)[library newFunctionWithName:name constantValues:values error:&err];
  *err_out = (obj_handle_t)err;
  [name release];
  [values release];
  return ret;
}

WINEMETAL_API bool
WMTQueryDisplaySetting(uint32_t in_display_id, enum WMTColorSpace *colorspace, struct WMTHDRMetadata *metadata)
{
  CGDirectDisplayID display_id = in_display_id;
  struct WMTHDRMetadata *value = metadata;
  struct DisplaySetting *setting = &g_display_settings[display_id == CGMainDisplayID()];
  if (setting->version) {
    *value = setting->hdr_metadata;
    *colorspace = setting->colorspace;
    return true;
  }
  return false;
}

WINEMETAL_API void
WMTUpdateDisplaySetting(uint32_t in_display_id, enum WMTColorSpace colorspace, const struct WMTHDRMetadata *metadata)
{
  CGDirectDisplayID display_id = in_display_id;
  const struct WMTHDRMetadata *value = metadata;
  struct DisplaySetting *setting = &g_display_settings[display_id == CGMainDisplayID()];
  if (value) {
    setting->hdr_metadata = *value;
    setting->colorspace = colorspace;
    setting->version++;
  } else {
    setting->version = 0;
  }
}

WINEMETAL_API void WMTQueryDisplaySettingForLayer(
    obj_handle_t obj_layer, uint64_t *version, enum WMTColorSpace *colorspace, struct WMTHDRMetadata *metadata,
    struct WMTEDRValue *edr_value
)
{
  CAMetalLayer *layer = (CAMetalLayer *)obj_layer;
  struct WMTHDRMetadata *hdr_metadata_out = metadata;

  *version = 0;
  if (![layer.delegate isKindOfClass:NSView.class])
    return;

  NSView *view = (NSView *)layer.delegate;
  if (!view.window)
    return;

  if (!view.window.screen)
    return;

  NSScreen *screen = view.window.screen;
  CGDirectDisplayID id = [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];

  struct DisplaySetting *setting = &g_display_settings[id == CGMainDisplayID()];
  *hdr_metadata_out = setting->hdr_metadata;
  *version = setting->version;
  *colorspace = setting->colorspace;
  edr_value->maximum_edr_color_component_value =
      layer.wantsExtendedDynamicRangeContent ? screen.maximumExtendedDynamicRangeColorComponentValue : 1.0;
  edr_value->maximum_potential_edr_color_component_value =
      screen.maximumPotentialExtendedDynamicRangeColorComponentValue;
}

WINEMETAL_API void MTLCommandBuffer_encodeWaitForEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value)
{
  [(id<MTLCommandBuffer>)cmdbuf encodeWaitForEvent:(id<MTLSharedEvent>)event value:value];
}

WINEMETAL_API void MTLSharedEvent_signalValue(obj_handle_t event, uint64_t value)
{
  [(id<MTLSharedEvent>)event setSignaledValue:value];
}

WINEMETAL_API void MTLSharedEvent_setWin32EventAtValue(obj_handle_t event, void *nt_event_handle, uint64_t at_value)
{
  static MTLSharedEventListener *shared_listener = nil;
  static dispatch_once_t pred;
  dispatch_once(&pred, ^{
    shared_listener = [[MTLSharedEventListener alloc] init];
  });

  [(id<MTLSharedEvent>)event notifyListener:shared_listener
                                             atValue:at_value
                                               block:^(id<MTLSharedEvent> _e, uint64_t _v) {
                                                 /*NtSetEvent(nt_event_handle, NULL);*/ /**@todo*/
                                                *(uint8_t *)0 = 0;
                                               }];
}

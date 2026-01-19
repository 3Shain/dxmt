#include "dxmt_command.hpp"
#include "Metal.hpp"
#include "dxmt_context.hpp"
#include "dxmt_format.hpp"

#include "dxmt_command.h"

#define CREATE_PIPELINE(name)                                                                                          \
  auto name##_function = library.newFunction(#name);           \
  name##_pipeline = device.newComputePipelineState(name##_function, error);

namespace dxmt {

InternalCommandLibrary::InternalCommandLibrary(WMT::Device device) {
  WMT::Reference<WMT::Error> error;
  library_ = device.newLibrary(dxmt_command, dxmt_command_len, error);

  if (error) {
    ERR("Failed to create internal command library: ", error.description().getUTF8String());
    abort();
  }
};

EmulatedCommandContext::EmulatedCommandContext(WMT::Device device, InternalCommandLibrary &lib, ArgumentEncodingContext &ctx): ctx(ctx) {
  auto pool = WMT::MakeAutoreleasePool();

  auto library = lib.getLibrary();

  WMT::Reference<WMT::Error> error;

  CREATE_PIPELINE(clear_texture_1d_uint);
  CREATE_PIPELINE(clear_texture_1d_array_uint);
  CREATE_PIPELINE(clear_texture_2d_uint);
  CREATE_PIPELINE(clear_texture_2d_array_uint);
  CREATE_PIPELINE(clear_texture_3d_uint);
  CREATE_PIPELINE(clear_texture_buffer_uint);
  CREATE_PIPELINE(clear_buffer_uint);
  CREATE_PIPELINE(clear_texture_1d_float);
  CREATE_PIPELINE(clear_texture_1d_array_float);
  CREATE_PIPELINE(clear_texture_2d_float);
  CREATE_PIPELINE(clear_texture_2d_array_float);
  CREATE_PIPELINE(clear_texture_3d_float);
  CREATE_PIPELINE(clear_texture_buffer_float);


  auto gs_draw_arguments_marshal_vs = library.newFunction("gs_draw_arguments_marshal");
  {
    WMTRenderPipelineInfo gs_marshal_pipeline;
    WMT::InitializeRenderPipelineInfo(gs_marshal_pipeline);
    gs_marshal_pipeline.vertex_function = gs_draw_arguments_marshal_vs;
    gs_marshal_pipeline.rasterization_enabled = false;
    gs_draw_arguments_marshal = device.newRenderPipelineState(gs_marshal_pipeline, error);
  }

  auto ts_draw_arguments_marshal_vs = library.newFunction("ts_draw_arguments_marshal");
  {
    WMTRenderPipelineInfo ts_marshal_pipeline;
    WMT::InitializeRenderPipelineInfo(ts_marshal_pipeline);
    ts_marshal_pipeline.vertex_function = ts_draw_arguments_marshal_vs;
    ts_marshal_pipeline.rasterization_enabled = false;
    ts_draw_arguments_marshal = device.newRenderPipelineState(ts_marshal_pipeline, error);
  }
}

void
EmulatedCommandContext::setComputePipelineState(WMT::ComputePipelineState state, const WMTSize &threadgroup_size) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setpso>();
  cmd.type = WMTComputeCommandSetPSO;
  cmd.pso = state;
  cmd.threadgroup_size = threadgroup_size;
}

void EmulatedCommandContext::dispatchThreads(const WMTSize &grid_size) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_dispatch>();
  cmd.type = WMTComputeCommandDispatchThreads;
  cmd.size = grid_size;
}

void
EmulatedCommandContext::setComputeBuffer(WMT::Buffer buffer, uint64_t offset, uint8_t index) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setbuffer>();
  cmd.type = WMTComputeCommandSetBuffer;
  cmd.buffer = buffer;
  cmd.offset = offset;
  cmd.index = index;
}

void
EmulatedCommandContext::setComputeTexture(WMT::Texture texture, uint8_t index) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_settexture>();
  cmd.type = WMTComputeCommandSetTexture;
  cmd.texture = texture;
  cmd.index = index;
}

void
EmulatedCommandContext::setComputeBytes(const void *buf, uint64_t length, uint8_t index) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setbytes>();
  cmd.type = WMTComputeCommandSetBytes;
  void *temp = ctx.allocate_cpu_heap(length, 8);
  memcpy(temp, buf, length);
  cmd.bytes.set(temp);
  cmd.length = length;
  cmd.index = index;
}

ClearRenderTargetContext::ClearRenderTargetContext(
    WMT::Device device, InternalCommandLibrary &lib, ArgumentEncodingContext &ctx
) :
    ctx_(ctx),
    device_(device) {
  auto library = lib.getLibrary();
  vs_clear_ = library.newFunction("vs_clear_rt");
  fs_clear_depth_ = library.newFunction("fs_clear_rt_depth");
  fs_clear_float_ = library.newFunction("fs_clear_rt_float");
  fs_clear_sint_ = library.newFunction("fs_clear_rt_sint");
  fs_clear_uint_ = library.newFunction("fs_clear_rt_uint");

  WMTDepthStencilInfo ds_info;
  ds_info.front_stencil.enabled = false;
  ds_info.back_stencil.enabled = false;
  ds_info.depth_compare_function = WMTCompareFunctionAlways;
  ds_info.depth_write_enabled = false;

  depth_readonly_state_ = device.newDepthStencilState(ds_info);

  ds_info.depth_write_enabled = true;

  depth_write_state_ = device.newDepthStencilState(ds_info);
}

void
ClearRenderTargetContext::begin(Rc<Texture> texture, TextureViewKey view) {
  assert(!clearing_texture_);

  WMT::Reference<WMT::Error> err;

  auto format = texture->pixelFormat(view);
  auto dsv_flag = DepthStencilPlanarFlags(format);

  if (dsv_flag & ~1u)
    return; // stencil clear is not supported

  // enforce array render target for now
  view = texture->checkViewUseArray(view, true);

  if (!pso_cache_.contains(format)) {
    WMTRenderPipelineInfo pipeline_info;
    WMT::InitializeRenderPipelineInfo(pipeline_info);
    pipeline_info.raster_sample_count = texture->sampleCount();
    pipeline_info.vertex_function = vs_clear_;
    if (dsv_flag) {
      pipeline_info.fragment_function = fs_clear_depth_;
      pipeline_info.depth_pixel_format = format;
    } else if (IsIntegerFormat(format)) {
      pipeline_info.colors[0].pixel_format = format;
      if (MTLGetUnsignedIntegerFormat(format) == format) {
        pipeline_info.fragment_function = fs_clear_uint_;
      } else {
        pipeline_info.fragment_function = fs_clear_sint_;
      }
    } else {
      pipeline_info.colors[0].pixel_format = format;
      pipeline_info.fragment_function = fs_clear_float_;
    }
    pipeline_info.rasterization_enabled = true;
    pipeline_info.input_primitive_topology = WMTPrimitiveTopologyClassTriangle;
    auto pso = device_.newRenderPipelineState(pipeline_info, err);
    if (pso == nullptr) {
      ERR("Failed to create ClearRenderTarget PSO of format ", format, ": ", err.description().getUTF8String());
    }
    pso_cache_.emplace(format, std::move(pso));
  }

  WMT::RenderPipelineState pso = pso_cache_.at(format);

  if (!pso)
    return;

  auto width = texture->width(view);
  auto height = texture->height(view);
  auto array_length = texture->arrayLength(view);
  auto &pass_info = ctx_.startRenderPass(dsv_flag, 0, 1, 0)->info;

  if (dsv_flag) {
    auto &depth = pass_info.depth;
    depth.texture = ctx_.access(texture, view, DXMT_ENCODER_RESOURCE_ACESS_WRITE).texture;
    depth.depth_plane = 0;
    depth.load_action = WMTLoadActionLoad;
    depth.store_action = WMTStoreActionStore;
  } else {
    auto &color = pass_info.colors[0];
    color.texture = ctx_.access(texture, view, DXMT_ENCODER_RESOURCE_ACESS_WRITE).texture;
    color.depth_plane = 0;
    color.load_action = WMTLoadActionLoad;
    color.store_action = WMTStoreActionStore;
  }

  pass_info.render_target_width = width;
  pass_info.render_target_height = height;
  pass_info.render_target_array_length = array_length;
  pass_info.default_raster_sample_count = texture->sampleCount();

  auto &setpso = ctx_.encodeRenderCommand<wmtcmd_render_setpso>();
  setpso.type = WMTRenderCommandSetPSO;
  setpso.pso = pso;

  auto &setvp = ctx_.encodeRenderCommand<wmtcmd_render_setviewport>();
  setvp.type = WMTRenderCommandSetViewport;
  setvp.viewport = {0.0, 0.0, (double)width, (double)height, 0.0, 1.0};

  auto &setdsso = ctx_.encodeRenderCommand<wmtcmd_render_setdsso>();
  setdsso.type = WMTRenderCommandSetDSSO;
  setdsso.dsso = dsv_flag ? depth_write_state_.handle : depth_readonly_state_.handle;
  setdsso.stencil_ref = 0;

  clearing_texture_ = std::move(texture);
  clearing_texture_view_ = view;
}

void
ClearRenderTargetContext::clear(
    uint32_t offset_x, uint32_t offset_y, uint32_t width, uint32_t height, const std::array<float, 4> &color
) {
  if (!clearing_texture_)
    return;
  auto &setscr = ctx_.encodeRenderCommand<wmtcmd_render_setscissorrect>();
  setscr.type = WMTRenderCommandSetScissorRect;
  setscr.scissor_rect = {offset_x, offset_y, width, height};

  auto &setcolor = ctx_.encodeRenderCommand<wmtcmd_render_setbytes>();
  setcolor.type = WMTRenderCommandSetFragmentBytes;
  void *temp = ctx_.allocate_cpu_heap(sizeof(color), 16);
  memcpy(temp, color.data(), sizeof(color));
  setcolor.bytes.set(temp);
  setcolor.length = sizeof(color);
  setcolor.index = 0;

  auto &draw = ctx_.encodeRenderCommand<wmtcmd_render_draw>();
  draw.type = WMTRenderCommandDraw;
  draw.primitive_type = WMTPrimitiveTypeTriangle;
  draw.vertex_start = 0;
  draw.vertex_count = 3;
  draw.base_instance = 0;
  draw.instance_count = ctx_.currentRenderEncoder()->info.render_target_array_length;
}

void
ClearRenderTargetContext::end() {
  if (!clearing_texture_)
    return;
  ctx_.endPass();
  clearing_texture_ = nullptr;
  clearing_texture_view_ = 0;
};

DepthStencilBlitContext::DepthStencilBlitContext(
    WMT::Device device, InternalCommandLibrary &lib, ArgumentEncodingContext &ctx
) :
    ctx_(ctx),
    device_(device) {
  WMT::Reference<WMT::Error> err;

  auto library = lib.getLibrary();

  auto vs_copy = library.newFunction("vs_present_quad");
  auto fs_copy_d24s8 = library.newFunction("fs_copy_from_buffer_d24s8");
  auto fs_copy_d32s8 = library.newFunction("fs_copy_from_buffer_d32s8");
  auto cs_copy_d24s8 = library.newFunction("cs_copy_to_buffer_d24s8");
  auto cs_copy_d32s8 = library.newFunction("cs_copy_to_buffer_d32s8");

  WMTRenderPipelineInfo pipeline_info;
  WMT::InitializeRenderPipelineInfo(pipeline_info);
  pipeline_info.vertex_function = vs_copy;
  pipeline_info.depth_pixel_format = WMTPixelFormatDepth32Float_Stencil8;
  pipeline_info.stencil_pixel_format = WMTPixelFormatDepth32Float_Stencil8;
  pipeline_info.rasterization_enabled = true;
  pipeline_info.input_primitive_topology = WMTPrimitiveTopologyClassTriangle;

  pipeline_info.fragment_function = fs_copy_d24s8;
  pso_copy_d24s8_ = device_.newRenderPipelineState(pipeline_info, err);

  pipeline_info.fragment_function = fs_copy_d32s8;
  pso_copy_d32s8_ = device_.newRenderPipelineState(pipeline_info, err);

  WMTDepthStencilInfo ds_info;
  ds_info.front_stencil.enabled = true;
  ds_info.front_stencil.stencil_compare_function = WMTCompareFunctionAlways;
  ds_info.front_stencil.depth_stencil_pass_op = WMTStencilOperationReplace;
  ds_info.front_stencil.depth_fail_op = WMTStencilOperationReplace;
  ds_info.front_stencil.stencil_fail_op = WMTStencilOperationReplace;
  ds_info.front_stencil.write_mask = 0xFF;
  ds_info.front_stencil.read_mask = 0;
  ds_info.back_stencil.enabled = true;
  ds_info.back_stencil.stencil_compare_function = WMTCompareFunctionAlways;
  ds_info.back_stencil.depth_stencil_pass_op = WMTStencilOperationReplace;
  ds_info.back_stencil.depth_fail_op = WMTStencilOperationReplace;
  ds_info.back_stencil.stencil_fail_op = WMTStencilOperationReplace;
  ds_info.back_stencil.write_mask = 0xFF;
  ds_info.back_stencil.read_mask = 0;
  ds_info.depth_compare_function = WMTCompareFunctionAlways;
  ds_info.depth_write_enabled = true;

  depth_stencil_state_ = device.newDepthStencilState(ds_info);

  pso_copy_to_buffer_d24s8_ = device_.newComputePipelineState(cs_copy_d24s8, err);
  pso_copy_to_buffer_d32s8_ = device_.newComputePipelineState(cs_copy_d32s8, err);
}

struct linear_texture_desc {
  uint32_t bytes_per_row;
  uint32_t bytes_per_image;
};

void
DepthStencilBlitContext::copyFromBuffer(
    const Rc<Buffer> &src, uint64_t src_offset, uint64_t src_length, uint32_t bytes_per_row, uint32_t bytes_per_image,
    const Rc<Texture> &depth_stencil, uint32_t level, uint32_t slice, bool from_d24s8
) {

  TextureViewDescriptor view_desc;
  switch (depth_stencil->textureType()) {
  case WMTTextureType2D:
  case WMTTextureType2DArray:
  case WMTTextureTypeCube:
  case WMTTextureTypeCubeArray:
    view_desc.type = WMTTextureType2D;
    break;
  /*
  - 1d is already mapped to 2d
  - staging texture cannot be multisampled
  */
  default:
    return;
  }
  view_desc.format = depth_stencil->pixelFormat();
  view_desc.firstMiplevel = level;
  view_desc.miplevelCount = 1;
  view_desc.firstArraySlice = slice;
  view_desc.arraySize = 1;

  auto view = depth_stencil->createView(view_desc);

  auto width = depth_stencil->width(view);
  auto height = depth_stencil->height(view);
  auto &pass_info = ctx_.startRenderPass(0b11, 0, 0, 0)->info;
  auto &depth = pass_info.depth;
  depth.texture = ctx_.access(depth_stencil, view, DXMT_ENCODER_RESOURCE_ACESS_WRITE).texture;
  depth.depth_plane = 0;
  depth.load_action = WMTLoadActionLoad;
  depth.store_action = WMTStoreActionStore;

  auto &stencil = pass_info.stencil;
  stencil.texture = depth.texture;
  stencil.depth_plane = 0;
  stencil.load_action = WMTLoadActionLoad;
  stencil.store_action = WMTStoreActionStore;

  auto [src_, src_sub_offset] = ctx_.access(src, src_offset, src_length, DXMT_ENCODER_RESOURCE_ACESS_READ);

  pass_info.render_target_width = width;
  pass_info.render_target_height = height;
  pass_info.render_target_array_length = 0;
  pass_info.default_raster_sample_count = depth_stencil->sampleCount();

  auto &setpso = ctx_.encodeRenderCommand<wmtcmd_render_setpso>();
  setpso.type = WMTRenderCommandSetPSO;
  setpso.pso = from_d24s8 ? pso_copy_d24s8_: pso_copy_d32s8_;

  auto &setvp = ctx_.encodeRenderCommand<wmtcmd_render_setviewport>();
  setvp.type = WMTRenderCommandSetViewport;
  setvp.viewport = {0.0, 0.0, (double)width, (double)height, 0.0, 1.0};

  auto &setdsso = ctx_.encodeRenderCommand<wmtcmd_render_setdsso>();
  setdsso.type = WMTRenderCommandSetDSSO;
  setdsso.dsso = depth_stencil_state_.handle;
  setdsso.stencil_ref = 0;

  auto &setbuf = ctx_.encodeRenderCommand<wmtcmd_render_setbuffer>();
  setbuf.type = WMTRenderCommandSetFragmentBuffer;
  setbuf.buffer = src_->buffer();
  setbuf.index = 0;
  setbuf.offset = src_offset + src_sub_offset;

  linear_texture_desc desc{bytes_per_row, bytes_per_image};
  auto &setdesc = ctx_.encodeRenderCommand<wmtcmd_render_setbytes>();
  setdesc.type = WMTRenderCommandSetFragmentBytes;
  void *temp = ctx_.allocate_cpu_heap(sizeof(desc), 16);
  memcpy(temp, &desc, sizeof(desc));
  setdesc.bytes.set(temp);
  setdesc.length = sizeof(desc);
  setdesc.index = 1;

  auto &draw = ctx_.encodeRenderCommand<wmtcmd_render_draw>();
  draw.type = WMTRenderCommandDraw;
  draw.primitive_type = WMTPrimitiveTypeTriangle;
  draw.vertex_start = 0;
  draw.vertex_count = 3;
  draw.base_instance = 0;
  draw.instance_count = 1;

  ctx_.endPass();
}

void
DepthStencilBlitContext::copyFromTexture(
    const Rc<Texture> &depth_stencil, uint32_t level, uint32_t slice, const Rc<Buffer> &dst, uint64_t dst_offset,
    uint64_t dst_length, uint32_t bytes_per_row, uint32_t bytes_per_image, bool to_d24s8
) {
  TextureViewDescriptor view_desc;
  switch (depth_stencil->textureType()) {
  case WMTTextureType2D:
  case WMTTextureType2DArray:
  case WMTTextureTypeCube:
  case WMTTextureTypeCubeArray:
    view_desc.type = WMTTextureType2D;
    break;
  /*
  - 1d is already mapped to 2d
  - staging texture cannot be multisampled
  */
  default:
    return;
  }
  view_desc.format = WMTPixelFormatDepth32Float_Stencil8;
  view_desc.firstMiplevel = level;
  view_desc.miplevelCount = 1;
  view_desc.firstArraySlice = slice;
  view_desc.arraySize = 1;
  auto depth_view = depth_stencil->createView(view_desc);

  view_desc.format = WMTPixelFormatX32_Stencil8;
  auto stencil_view = depth_stencil->createView(view_desc);

  ctx_.startComputePass(0);
  auto tex_depth = ctx_.access(depth_stencil, depth_view, DXMT_ENCODER_RESOURCE_ACESS_READ).texture;
  auto tex_stencil = ctx_.access(depth_stencil, stencil_view, DXMT_ENCODER_RESOURCE_ACESS_READ).texture;
  auto [dst_, dst_sub_offset] = ctx_.access(dst, dst_offset, dst_length, DXMT_ENCODER_RESOURCE_ACESS_WRITE);

  auto &setpso = ctx_.encodeComputeCommand<wmtcmd_compute_setpso>();
  setpso.type = WMTComputeCommandSetPSO;
  setpso.pso = to_d24s8 ? pso_copy_to_buffer_d24s8_ : pso_copy_to_buffer_d32s8_;
  setpso.threadgroup_size = {8, 4, 1};

  auto &setdepth = ctx_.encodeComputeCommand<wmtcmd_compute_settexture>();
  setdepth.type = WMTComputeCommandSetTexture;
  setdepth.texture = tex_depth;
  setdepth.index = 0;

  auto &setstencil = ctx_.encodeComputeCommand<wmtcmd_compute_settexture>();
  setstencil.type = WMTComputeCommandSetTexture;
  setstencil.texture = tex_stencil;
  setstencil.index = 1;

  auto &setbuf = ctx_.encodeComputeCommand<wmtcmd_compute_setbuffer>();
  setbuf.type = WMTComputeCommandSetBuffer;
  setbuf.buffer = dst_->buffer();
  setbuf.index = 0;
  setbuf.offset = dst_offset + dst_sub_offset;

  linear_texture_desc desc{bytes_per_row, bytes_per_image};
  auto &setdesc = ctx_.encodeComputeCommand<wmtcmd_compute_setbytes>();
  setdesc.type = WMTComputeCommandSetBytes;
  void *temp = ctx_.allocate_cpu_heap(sizeof(desc), 16);
  memcpy(temp, &desc, sizeof(desc));
  setdesc.bytes.set(temp);
  setdesc.length = sizeof(desc);
  setdesc.index = 1;

  auto width = depth_stencil->width(depth_view);
  auto height = depth_stencil->height(depth_view);
  auto &dispatch = ctx_.encodeComputeCommand<wmtcmd_compute_dispatch>();
  dispatch.type = WMTComputeCommandDispatchThreads;
  dispatch.size = {width, height, 1};

  ctx_.endPass();
}

ClearResourceKernelContext::ClearResourceKernelContext(
    WMT::Device device, InternalCommandLibrary &lib, ArgumentEncodingContext &ctx
) :
    ctx_(ctx),
    device_(device) {
  auto library = lib.getLibrary();
  cs_clear_buffer_uint_ = library.newFunction("cs_clear_buffer_uint");
  cs_clear_buffer_float_ = library.newFunction("cs_clear_buffer_float");
  cs_clear_tbuffer_uint_ = library.newFunction("cs_clear_tbuffer_uint");
  cs_clear_tbuffer_float_ = library.newFunction("cs_clear_tbuffer_float");
  cs_clear_texture2d_uint_ = library.newFunction("cs_clear_texture2d_uint");
  cs_clear_texture2d_float_ = library.newFunction("cs_clear_texture2d_float");
  cs_clear_texture2d_array_uint_ = library.newFunction("cs_clear_texture2d_array_uint");
  cs_clear_texture2d_array_float_ = library.newFunction("cs_clear_texture2d_array_float");
}

void
ClearResourceKernelContext::setClearColor(const std::array<float, 4> &color, bool is_integer) {
  if (is_integer) {
    meta_temp_.color_f32[0] = uint32_t(std::max(color[0], 0.0f));
    meta_temp_.color_f32[1] = uint32_t(std::max(color[1], 0.0f));
    meta_temp_.color_f32[2] = uint32_t(std::max(color[2], 0.0f));
    meta_temp_.color_f32[3] = uint32_t(std::max(color[3], 0.0f));
  } else {
    meta_temp_.color_f32[0] = color[0];
    meta_temp_.color_f32[1] = color[1];
    meta_temp_.color_f32[2] = color[2];
    meta_temp_.color_f32[3] = color[3];
  }
};

void
ClearResourceKernelContext::begin(const std::array<float, 4> &color, Rc<Texture> texture, TextureViewKey view) {

  clearing_texture_ = texture;
  clearing_view_ = 0;
  ctx_.startComputePass(0);

  bool is_integer = IsIntegerFormat(texture->pixelFormat(view));

  setClearColor(color, is_integer);

  bool is_array = false;
  switch (texture->textureType(view)) {
  case WMTTextureType1DArray:
  case WMTTextureType2DArray:
    is_array = true;
    dispatch_depth_ = texture->arrayLength(view);
    break;
  case WMTTextureTypeCube:
  case WMTTextureTypeCubeArray:
  case WMTTextureType3D:
  case WMTTextureType2DMultisample:
  case WMTTextureType2DMultisampleArray:
    // not a valid clear target
    return;
  default:
    break;
  }

  auto &setpso = ctx_.encodeComputeCommand<wmtcmd_compute_setpso>();
  setpso.type = WMTComputeCommandSetPSO;
  setpso.pso = is_integer ? (is_array ? cs_clear_texture2d_array_uint_ : cs_clear_texture2d_uint_)
                          : (is_array ? cs_clear_texture2d_array_float_ : cs_clear_texture2d_float_);
  setpso.threadgroup_size = {32, 1, 1};
}

void
ClearResourceKernelContext::begin(const std::array<float, 4> &color, Rc<Buffer> buffer, BufferViewKey view) {

  clearing_buffer_ = buffer;
  clearing_view_ = view;
  ctx_.startComputePass(0);

  bool is_integer = IsIntegerFormat(buffer->pixelFormat(view));

  setClearColor(color, is_integer);

  auto &setpso = ctx_.encodeComputeCommand<wmtcmd_compute_setpso>();
  setpso.type = WMTComputeCommandSetPSO;
  setpso.pso = is_integer ? cs_clear_tbuffer_uint_ : cs_clear_tbuffer_float_;
  setpso.threadgroup_size = {32, 1, 1};
}

void
ClearResourceKernelContext::begin(const std::array<float, 4> &color, Rc<Buffer> buffer, bool raw_buffer_is_integer) {

  clearing_buffer_ = buffer;
  clearing_view_ = 0;
  ctx_.startComputePass(0);

  setClearColor(color, raw_buffer_is_integer);

  auto &setpso = ctx_.encodeComputeCommand<wmtcmd_compute_setpso>();
  setpso.type = WMTComputeCommandSetPSO;
  setpso.pso = raw_buffer_is_integer ? cs_clear_buffer_uint_ : cs_clear_buffer_float_;
  setpso.threadgroup_size = {32, 1, 1};
}

void
ClearResourceKernelContext::clear(uint32_t offset_x, uint32_t offset_y, uint32_t width, uint32_t height) {
  meta_temp_.offset[0] = offset_x;
  meta_temp_.offset[1] = offset_y;
  meta_temp_.size[0] = width;
  meta_temp_.size[1] = height;

  if (clearing_texture_) {
    auto dst_ = ctx_.access(clearing_texture_, clearing_view_, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
    auto &settex = ctx_.encodeComputeCommand<wmtcmd_compute_settexture>();
    settex.type = WMTComputeCommandSetTexture;
    settex.texture = dst_.texture;
    settex.index = 0;
  } else if (clearing_buffer_) {
    if (clearing_view_) {
      auto [dst_, dst_sub_offset] = ctx_.access(clearing_buffer_, clearing_view_, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
      auto &settexbuf = ctx_.encodeComputeCommand<wmtcmd_compute_settexture>();
      settexbuf.type = WMTComputeCommandSetTexture;
      settexbuf.texture = dst_.texture;
      settexbuf.index = 0;
      meta_temp_.offset[0] += dst_sub_offset;
    } else {
      auto [dst_, dst_sub_offset] = ctx_.access(clearing_buffer_, offset_x, width, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
      auto &setbuf = ctx_.encodeComputeCommand<wmtcmd_compute_setbuffer>();
      setbuf.type = WMTComputeCommandSetBuffer;
      setbuf.buffer = dst_->buffer();
      setbuf.index = 0;
      setbuf.offset = 0;
      meta_temp_.offset[0] += dst_sub_offset;
    }
  } else {
    return;
  }

  auto &setmeta = ctx_.encodeComputeCommand<wmtcmd_compute_setbytes>();
  setmeta.type = WMTComputeCommandSetBytes;
  void *temp = ctx_.allocate_cpu_heap(sizeof(meta_temp_), 16);
  memcpy(temp, &meta_temp_, sizeof(meta_temp_));
  setmeta.bytes.set(temp);
  setmeta.length = sizeof(meta_temp_);
  setmeta.index = 1;

  auto &dispatch = ctx_.encodeComputeCommand<wmtcmd_compute_dispatch>();
  dispatch.type = WMTComputeCommandDispatchThreads;
  dispatch.size = {width, height, dispatch_depth_};
}

void
ClearResourceKernelContext::end() {
  if (!clearing_texture_ && !clearing_buffer_)
    return;
  ctx_.endPass();
  clearing_texture_ = nullptr;
  clearing_buffer_ = nullptr;
  clearing_view_ = 0;
  dispatch_depth_ = 1;
};

MTLFXMVScaleContext::MTLFXMVScaleContext(
    WMT::Device device, InternalCommandLibrary &lib, ArgumentEncodingContext &ctx
) :
    ctx_(ctx),
    device_(device) {
  WMT::Reference<WMT::Error> err;

  auto library = lib.getLibrary();

  auto cs_ = library.newFunction("cs_downscale_dilated_mv");

  pso_downscale_dilated_mv_ = device_.newComputePipelineState(cs_, err);
}

struct downscale_dilated_mv_desc {
  float scale_x;
  float scale_y;
};

void
MTLFXMVScaleContext::dispatch(
    const Rc<Texture> &dilated, TextureViewKey view_dilated, const Rc<Texture> &downscaled,
    TextureViewKey view_downscaled, float mv_scale_x, float mv_scale_y
) {
  ctx_.startComputePass(0);
  auto tex_dilated = ctx_.access(dilated, view_dilated, DXMT_ENCODER_RESOURCE_ACESS_READ).texture;
  auto tex_downscaled = ctx_.access(downscaled, view_downscaled, DXMT_ENCODER_RESOURCE_ACESS_WRITE).texture;

  auto &setpso = ctx_.encodeComputeCommand<wmtcmd_compute_setpso>();
  setpso.type = WMTComputeCommandSetPSO;
  setpso.pso = pso_downscale_dilated_mv_;
  setpso.threadgroup_size = {8, 4, 1};

  auto &setdepth = ctx_.encodeComputeCommand<wmtcmd_compute_settexture>();
  setdepth.type = WMTComputeCommandSetTexture;
  setdepth.texture = tex_dilated;
  setdepth.index = 0;

  auto &setstencil = ctx_.encodeComputeCommand<wmtcmd_compute_settexture>();
  setstencil.type = WMTComputeCommandSetTexture;
  setstencil.texture = tex_downscaled;
  setstencil.index = 1;

  downscale_dilated_mv_desc desc{mv_scale_x, mv_scale_y};
  auto &setdesc = ctx_.encodeComputeCommand<wmtcmd_compute_setbytes>();
  setdesc.type = WMTComputeCommandSetBytes;
  void *temp = ctx_.allocate_cpu_heap(sizeof(desc), 16);
  memcpy(temp, &desc, sizeof(desc));
  setdesc.bytes.set(temp);
  setdesc.length = sizeof(desc);
  setdesc.index = 0;

  auto width = downscaled->width(view_downscaled);
  auto height = downscaled->height(view_downscaled);
  auto &dispatch = ctx_.encodeComputeCommand<wmtcmd_compute_dispatch>();
  dispatch.type = WMTComputeCommandDispatchThreads;
  dispatch.size = {width, height, 1};

  ctx_.endPass();
}

} // namespace dxmt
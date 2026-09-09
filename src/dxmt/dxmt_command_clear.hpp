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

#pragma once

#include "dxmt_command_constants.hpp"
#include "dxmt_command_context.hpp"
#include "dxmt_format.hpp"

namespace dxmt {

template <typename Context> class ClearUAV {
public:
  ClearUAV(WMT::Device device, Context &ctx) : device_(device), ctx_(ctx) {
#define CREATE_COMPUTE_PIPELINE(name) name##_ = ctx_.getComputePipeline(#name);
    CREATE_COMPUTE_PIPELINE(cs_clear_buffer_uint);
    CREATE_COMPUTE_PIPELINE(cs_clear_buffer_float);
    CREATE_COMPUTE_PIPELINE(cs_clear_tbuffer_uint);
    CREATE_COMPUTE_PIPELINE(cs_clear_tbuffer_float);
    CREATE_COMPUTE_PIPELINE(cs_clear_texture2d_uint);
    CREATE_COMPUTE_PIPELINE(cs_clear_texture2d_float);
    CREATE_COMPUTE_PIPELINE(cs_clear_texture2d_array_uint);
    CREATE_COMPUTE_PIPELINE(cs_clear_texture2d_array_float);
    CREATE_COMPUTE_PIPELINE(cs_clear_texture3d_uint);
    CREATE_COMPUTE_PIPELINE(cs_clear_texture3d_float);
#undef CREATE_COMPUTE_PIPELINE
  }

  void
  begin(const std::array<float, 4> &color, Texture *texture, TextureViewKey view) {

    clearing_texture_ = texture;
    clearing_view_ = view;
    ctx_.startComputePass();

    setClearColor(color);

    bool is_array = false, is_3d = false;
    switch (texture->textureType(view)) {
    case WMTTextureType1DArray:
    case WMTTextureType2DArray:
      is_array = true;
      dispatch_depth_ = texture->arrayLength(view);
      break;
    case WMTTextureType3D:
      is_3d = true;
      dispatch_depth_ = texture->depth(view);
      break;
    case WMTTextureTypeCube:
    case WMTTextureTypeCubeArray:
    case WMTTextureType2DMultisample:
    case WMTTextureType2DMultisampleArray:
      // not a valid clear target
      return;
    default:
      break;
    }

    ctx_.setComputePSO(
        is_3d ? cs_clear_texture3d_float_ : (is_array ? cs_clear_texture2d_array_float_ : cs_clear_texture2d_float_),
        {32, 1, 1}
    );
  }

  void
  begin(const std::array<uint32_t, 4> &color, Texture *texture, TextureViewKey view) {

    clearing_texture_ = texture;
    clearing_view_ = view;
    ctx_.startComputePass();

    auto view_format = texture->pixelFormat(view);
    auto uint_format = MTLGetUnsignedIntegerFormat(view_format);
    if (view_format == WMTPixelFormatRG11B10Float || view_format == WMTPixelFormatRGB9E5Float) {
      uint_format = WMTPixelFormatR32Uint;
    }

    if (view_format != uint_format) {
      clearing_view_ = texture->checkViewUseFormat(clearing_view_, uint_format);
    }

    setClearColor(color, view_format);

    bool is_array = false, is_3d = false;
    switch (texture->textureType(view)) {
    case WMTTextureType1DArray:
    case WMTTextureType2DArray:
      is_array = true;
      dispatch_depth_ = texture->arrayLength(view);
      break;
    case WMTTextureType3D:
      is_3d = true;
      dispatch_depth_ = texture->depth(view);
      break;
    case WMTTextureTypeCube:
    case WMTTextureTypeCubeArray:
    case WMTTextureType2DMultisample:
    case WMTTextureType2DMultisampleArray:
      // not a valid clear target
      return;
    default:
      break;
    }

    ctx_.setComputePSO(
        is_3d ? cs_clear_texture3d_uint_ : (is_array ? cs_clear_texture2d_array_uint_ : cs_clear_texture2d_uint_),
        {32, 1, 1}
    );
  }

  void
  begin(const std::array<float, 4> &color, Buffer *buffer, BufferViewKey view) {

    clearing_buffer_ = buffer;
    clearing_view_ = view;

    ctx_.startComputePass();
    setClearColor(color);
    ctx_.setComputePSO(cs_clear_tbuffer_float_, {32, 1, 1});
  }

  void
  begin(const std::array<uint32_t, 4> &color, Buffer *buffer, BufferViewKey view) {

    clearing_buffer_ = buffer;
    clearing_view_ = view;

    ctx_.startComputePass();

    auto view_format = buffer->pixelFormat(view);
    auto uint_format = MTLGetUnsignedIntegerFormat(view_format);
    if (view_format == WMTPixelFormatRG11B10Float) {
      uint_format = WMTPixelFormatR32Uint;
    }

    if (view_format != uint_format) {
      BufferViewDescriptor view;
      view.format = uint_format;
      clearing_view_ = buffer->createView(view);
    }

    setClearColor(color, view_format);
    ctx_.setComputePSO(cs_clear_tbuffer_uint_, {32, 1, 1});
  }

  void
  begin(const std::array<float, 4> &color, Buffer *buffer) {

    clearing_buffer_ = buffer;
    clearing_view_ = 0;

    ctx_.startComputePass();
    setClearColor(color);
    ctx_.setComputePSO(cs_clear_buffer_float_, {32, 1, 1});
  }

  void
  begin(const std::array<uint32_t, 4> &color, Buffer *buffer) {

    clearing_buffer_ = buffer;
    clearing_view_ = 0;

    ctx_.startComputePass();
    setClearColor(color, WMTPixelFormatInvalid);
    ctx_.setComputePSO(cs_clear_buffer_uint_, {32, 1, 1});
  }

  void
  clear(uint32_t offset_x, uint32_t offset_y, uint32_t width, uint32_t height) {
    meta_temp_.offset[0] = offset_x;
    meta_temp_.offset[1] = offset_y;
    meta_temp_.size[0] = width;
    meta_temp_.size[1] = height;

    if (clearing_texture_) {
      ctx_.setComputeTexture(0, clearing_texture_, clearing_view_, ResourceAccess::Write);
    } else if (clearing_buffer_) {
      if (clearing_view_) {
        auto &dst_ = clearing_buffer_->view_(clearing_view_);
        auto dst_sub_offset = clearing_buffer_->current()->currentSuballocationOffset(dst_.suballocation_texel);
        ctx_.setComputeTexelBuffer(0, clearing_buffer_, clearing_view_, ResourceAccess::Write);
        meta_temp_.offset[0] += dst_sub_offset; // TODO: wrong?
        assert(!dst_sub_offset);
      } else {
        auto dst_ = clearing_buffer_->current();
        auto dst_sub_offset = dst_->currentSuballocationOffset();
        ctx_.setComputeBuffer(0, clearing_buffer_, offset_x, width, ResourceAccess::Write);
        meta_temp_.offset[0] += dst_sub_offset;
      }
    } else {
      return;
    }

    void *temp = ctx_.setComputeBytes(1, sizeof(meta_temp_));
    memcpy(temp, &meta_temp_, sizeof(meta_temp_));

    ctx_.dispatch({width, height, dispatch_depth_});
  }

  void
  end() {
    if (!clearing_texture_ && !clearing_buffer_)
      return;
    ctx_.endPass();
    clearing_texture_ = nullptr;
    clearing_buffer_ = nullptr;
    clearing_view_ = 0;
    dispatch_depth_ = 1;
  };

private:
  void
  setClearColor(const std::array<float, 4> &color) {
    meta_temp_.color_f32[0] = color[0];
    meta_temp_.color_f32[1] = color[1];
    meta_temp_.color_f32[2] = color[2];
    meta_temp_.color_f32[3] = color[3];
  };
  void
  setClearColor(const std::array<uint32_t, 4> &color, WMTPixelFormat source_format) {
    switch (source_format) {
    case WMTPixelFormatA8Unorm:
      meta_temp_.color_u32[0] = color[3] & 0xff;
      break;
    case WMTPixelFormatR8Unorm:
    case WMTPixelFormatR8Unorm_sRGB:
    case WMTPixelFormatR8Snorm:
    case WMTPixelFormatR8Uint:
    case WMTPixelFormatR8Sint:
    case WMTPixelFormatRG8Unorm_sRGB:
    case WMTPixelFormatRG8Snorm:
    case WMTPixelFormatRG8Unorm:
    case WMTPixelFormatRG8Uint:
    case WMTPixelFormatRG8Sint:
    case WMTPixelFormatRGBA8Unorm:
    case WMTPixelFormatRGBA8Unorm_sRGB:
    case WMTPixelFormatRGBA8Snorm:
    case WMTPixelFormatRGBA8Uint:
    case WMTPixelFormatRGBA8Sint:
      meta_temp_.color_u32[0] = color[0] & 0xff;
      meta_temp_.color_u32[1] = color[1] & 0xff;
      meta_temp_.color_u32[2] = color[2] & 0xff;
      meta_temp_.color_u32[3] = color[3] & 0xff;
      break;
    case WMTPixelFormatBGRA8Unorm:
    case WMTPixelFormatBGRA8Unorm_sRGB:
    case WMTPixelFormatBGRX8Unorm:
    case WMTPixelFormatBGRX8Unorm_sRGB:
      meta_temp_.color_u32[0] = color[2] & 0xff;
      meta_temp_.color_u32[1] = color[1] & 0xff;
      meta_temp_.color_u32[2] = color[0] & 0xff;
      meta_temp_.color_u32[3] = color[3] & 0xff;
      break;
    case WMTPixelFormatR16Unorm:
    case WMTPixelFormatR16Snorm:
    case WMTPixelFormatR16Uint:
    case WMTPixelFormatR16Sint:
    case WMTPixelFormatR16Float:
    case WMTPixelFormatRG16Unorm:
    case WMTPixelFormatRG16Snorm:
    case WMTPixelFormatRG16Uint:
    case WMTPixelFormatRG16Sint:
    case WMTPixelFormatRG16Float:
    case WMTPixelFormatRGBA16Unorm:
    case WMTPixelFormatRGBA16Snorm:
    case WMTPixelFormatRGBA16Uint:
    case WMTPixelFormatRGBA16Sint:
    case WMTPixelFormatRGBA16Float:
      meta_temp_.color_u32[0] = color[0] & 0xffff;
      meta_temp_.color_u32[1] = color[1] & 0xffff;
      meta_temp_.color_u32[2] = color[2] & 0xffff;
      meta_temp_.color_u32[3] = color[3] & 0xffff;
      break;
    case WMTPixelFormatRGB10A2Unorm:
    case WMTPixelFormatRGB10A2Uint:
      meta_temp_.color_u32[0] = color[0] & 0x3ff;
      meta_temp_.color_u32[1] = color[1] & 0x3ff;
      meta_temp_.color_u32[2] = color[2] & 0x3ff;
      meta_temp_.color_u32[3] = color[3] & 0x3;
      break;
    case WMTPixelFormatBGR10A2Unorm:
    case WMTPixelFormatBGR10_XR:
    case WMTPixelFormatBGR10_XR_sRGB:
      meta_temp_.color_u32[0] = color[2] & 0x3ff;
      meta_temp_.color_u32[1] = color[1] & 0x3ff;
      meta_temp_.color_u32[2] = color[0] & 0x3ff;
      meta_temp_.color_u32[3] = color[3] & 0x3;
      break;
    case WMTPixelFormatRG11B10Float:
      // interpreted as R32Uint
      meta_temp_.color_u32[0] = ((color[0] & 0x7FF) << 0) | ((color[1] & 0x7FF) << 11) | ((color[2] & 0x3FF) << 22);
      meta_temp_.color_u32[1] = 0;
      meta_temp_.color_u32[2] = 0;
      meta_temp_.color_u32[3] = 0;
      break;
    case WMTPixelFormatRGB9E5Float:
      // interpreted as R32Uint
      meta_temp_.color_u32[0] = ((color[0] & 0x1FF) << 0) | ((color[1] & 0x1FF) << 9) | ((color[2] & 0x1FF) << 18) |
                                ((color[3] & 0b11111) << 27);
      meta_temp_.color_u32[1] = 0;
      meta_temp_.color_u32[2] = 0;
      meta_temp_.color_u32[3] = 0;
      break;
    default:
      meta_temp_.color_u32[0] = color[0];
      meta_temp_.color_u32[1] = color[1];
      meta_temp_.color_u32[2] = color[2];
      meta_temp_.color_u32[3] = color[3];
      break;
    }
  };

  WMT::Device device_;
  SimpleCommandContext<Context> ctx_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_buffer_uint_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_buffer_float_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_tbuffer_uint_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_tbuffer_float_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_texture2d_uint_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_texture2d_float_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_texture2d_array_uint_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_texture2d_array_float_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_texture3d_uint_;
  WMT::Reference<WMT::ComputePipelineState> cs_clear_texture3d_float_;
  Rc<Texture> clearing_texture_;
  Rc<Buffer> clearing_buffer_;
  uint64_t clearing_view_ = 0; // type compatible with BufferViewKey
  uint32_t dispatch_depth_ = 1;

  struct DXMTClearMetadata {
    union {
      float color_f32[4];
      uint32_t color_u32[4];
    };
    uint32_t offset[2];
    uint32_t size[2];
  };
  DXMTClearMetadata meta_temp_;
};

template <typename Context> class ClearRTV {
public:
  ClearRTV(WMT::Device device, Context &ctx) : device_(device), ctx_(ctx) {
    auto library = ctx_.getDefaultLibrary();
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

    ds_info.front_stencil.enabled = true;
    ds_info.front_stencil.depth_stencil_pass_op = WMTStencilOperationReplace;
    ds_info.front_stencil.depth_fail_op = WMTStencilOperationReplace;
    ds_info.front_stencil.stencil_fail_op = WMTStencilOperationReplace;
    ds_info.front_stencil.stencil_compare_function = WMTCompareFunctionAlways;
    ds_info.front_stencil.read_mask = 0xff;
    ds_info.front_stencil.write_mask = 0xff;
    depth_stencil_write_state_ = device.newDepthStencilState(ds_info);

    ds_info.depth_write_enabled = false;
    stencil_write_state_ = device.newDepthStencilState(ds_info);
  }

  void
  begin(Rc<Texture> texture, TextureViewKey view, uint32_t depth_plane, uint32_t dsv_flag = 0) {
    assert(!clearing_texture_);

    WMT::Reference<WMT::Error> err;

    auto format = texture->pixelFormat(view);

    union {
      uint64_t u64;
      struct {
        uint32_t dsv_flag     : 2;
        uint32_t sample_count : 30;
        WMTPixelFormat format;
      };
    } key;
    static_assert(sizeof(key) == sizeof(uint64_t));
    key.dsv_flag = dsv_flag;
    key.sample_count = texture->sampleCount();
    key.format = format;

    if (!pso_cache_.contains(key.u64)) {
      WMTRenderPipelineInfo pipeline_info;
      WMT::InitializeRenderPipelineInfo(pipeline_info);
      pipeline_info.raster_sample_count = texture->sampleCount();
      pipeline_info.vertex_function = vs_clear_;
      if (dsv_flag) {
        pipeline_info.fragment_function = fs_clear_depth_;
        pipeline_info.depth_pixel_format = dsv_flag & 1 ? format : WMTPixelFormatInvalid;
        pipeline_info.stencil_pixel_format = dsv_flag & 2 ? format : WMTPixelFormatInvalid;
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
      pso_cache_.emplace(key.u64, std::move(pso));
    }

    WMT::RenderPipelineState pso = pso_cache_.at(key.u64);

    if (!pso)
      return;

    auto width = texture->width(view);
    auto height = texture->height(view);

    ctx_.startRenderPass();
    if (dsv_flag)
      ctx_.setDepthStencilAttachment(texture, view, dsv_flag);
    else
      ctx_.setColorAttachment(0, texture, view, depth_plane);
    ctx_.setRenderPSO(pso);
    ctx_.setViewport({0.0, 0.0, (double)width, (double)height, 0.0, 1.0});

    switch (dsv_flag) {
    case 3:
      ctx_.setDepthStencilState(depth_stencil_write_state_);
      break;
    case 2:
      ctx_.setDepthStencilState(stencil_write_state_);
      break;
    case 1:
      ctx_.setDepthStencilState(depth_write_state_);
      break;
    default:
      ctx_.setDepthStencilState(depth_readonly_state_);
      break;
    }

    clearing_texture_ = std::move(texture);
    clearing_texture_view_ = view;
  }

  void
  clear(
      uint32_t offset_x, uint32_t offset_y, uint32_t width, uint32_t height, uint32_t array_length,
      const std::array<float, 4> &color
  ) {
    if (!clearing_texture_)
      return;
    ctx_.setScissorRect({offset_x, offset_y, width, height});
    auto temp = ctx_.setFragmentBytes(kCustomBufferArgumentIndex0, sizeof(color));
    memcpy(temp, color.data(), sizeof(color));
    ctx_.draw(WMTPrimitiveTypeTriangle, 0, 3, 0, std::max(array_length, 1u));
  }

  void
  clear(uint32_t offset_x, uint32_t offset_y, uint32_t width, uint32_t height, float depth, uint8_t stencil) {
    if (!clearing_texture_)
      return;
    ctx_.setScissorRect({offset_x, offset_y, width, height});
    const std::array<float, 4> color = {depth, depth, depth, depth};
    auto temp = ctx_.setFragmentBytes(kCustomBufferArgumentIndex0, sizeof(color));
    memcpy(temp, color.data(), sizeof(color));
    ctx_.setStencilReference(stencil);
    ctx_.draw(WMTPrimitiveTypeTriangle, 0, 3, 0, std::max(clearing_texture_->arrayLength(clearing_texture_view_), 1u));
  }

  void
  end() {
    if (!clearing_texture_)
      return;
    ctx_.endPass();
    clearing_texture_ = nullptr;
    clearing_texture_view_ = 0;
  }

  WMT::Device device_;
  SimpleCommandContext<Context> ctx_;
  WMT::Reference<WMT::Function> vs_clear_;
  WMT::Reference<WMT::Function> fs_clear_float_;
  WMT::Reference<WMT::Function> fs_clear_uint_;
  WMT::Reference<WMT::Function> fs_clear_sint_;
  WMT::Reference<WMT::Function> fs_clear_depth_;
  WMT::Reference<WMT::DepthStencilState> depth_write_state_;
  WMT::Reference<WMT::DepthStencilState> depth_stencil_write_state_;
  WMT::Reference<WMT::DepthStencilState> stencil_write_state_;
  WMT::Reference<WMT::DepthStencilState> depth_readonly_state_;
  std::unordered_map<uint64_t, WMT::Reference<WMT::RenderPipelineState>> pso_cache_;
  Rc<Texture> clearing_texture_;
  TextureViewKey clearing_texture_view_;
};

} // namespace dxmt
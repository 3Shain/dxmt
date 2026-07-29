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

} // namespace dxmt
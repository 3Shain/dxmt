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

#include "dxmt_texture.hpp"
#include <cstdint>

namespace dxmt {

enum class EncoderType {
  Null,
  Clear,
  Render,
  Blit,
  Compute,
  Resolve,
};

struct EncoderData {
  EncoderType type;
  EncoderData *next = nullptr;
};

struct ClearEncoderData : EncoderData {
  union {
    WMTClearColor color;
    std::pair<float, uint8_t> depth_stencil;
  };
  TextureViewRef attachment;
  unsigned clear_dsv = 0;
  unsigned array_length = 0;
  unsigned width = 0;
  unsigned height = 0;
  unsigned depth_plane = 0;

  ClearEncoderData() {
    type = EncoderType::Clear;
    color = {0, 0, 0, 0};
  }
};

struct RenderEncoderColorAttachmentData {
  TextureViewRef attachment;
  enum WMTLoadAction load_action;
  enum WMTStoreAction store_action;
  uint16_t level;
  uint16_t slice;
  uint32_t depth_plane;
  struct WMTClearColor clear_color;
  TextureViewRef resolve_attachment;
  uint16_t resolve_level;
  uint16_t resolve_slice;
  uint32_t resolve_depth_plane;
};

struct RenderEncoderDepthAttachmentData {
  TextureViewRef attachment;
  enum WMTLoadAction load_action;
  enum WMTStoreAction store_action;
  uint16_t level;
  uint16_t slice;
  uint32_t depth_plane;
  float clear_depth;
};

struct RenderEncoderStencilAttachmentData {
  TextureViewRef attachment;
  enum WMTLoadAction load_action;
  enum WMTStoreAction store_action;
  uint16_t level;
  uint16_t slice;
  uint32_t depth_plane;
  uint8_t clear_stencil;
};

struct RenderEncoderData : EncoderData {
  std::array<RenderEncoderColorAttachmentData, 8> colors{{}};
  RenderEncoderDepthAttachmentData depth{};
  RenderEncoderStencilAttachmentData stencil{};
  uint8_t default_raster_sample_count = 0;
  uint16_t render_target_array_length = 0;
  uint32_t render_target_height = 16384;
  uint32_t render_target_width = 16384;
  wmtcmd_render_nop cmd_head;
  wmtcmd_base *cmd_tail;
  wmtcmd_compute_nop cmd_prepass_head;
  wmtcmd_base *cmd_prepass_tail;
  uint8_t dsv_planar_flags = 0;
  uint8_t dsv_readonly_flags = 0;
  uint8_t render_target_count = 0;
  bool use_visibility_result = 0;

  RenderEncoderData() {
    type = EncoderType::Render;
    cmd_head.type = WMTRenderCommandNop;
    cmd_head.next.set(nullptr);
    cmd_tail = (wmtcmd_base *)&cmd_head;
    cmd_prepass_head.type = WMTComputeCommandNop;
    cmd_prepass_head.next.set(nullptr);
    cmd_prepass_tail = (wmtcmd_base *)&cmd_prepass_head;
  }
};

struct BlitEncoderData : EncoderData {
  wmtcmd_blit_nop cmd_head;
  wmtcmd_base *cmd_tail;

  BlitEncoderData() {
    type = EncoderType::Blit;
    cmd_head.type = WMTBlitCommandNop;
    cmd_head.next.set(0);
    cmd_tail = (wmtcmd_base *)&cmd_head;
  }
};

struct ComputeEncoderData : EncoderData {
  wmtcmd_compute_nop cmd_head;
  wmtcmd_base *cmd_tail;

  ComputeEncoderData() {
    type = EncoderType::Compute;
    cmd_head.type = WMTComputeCommandNop;
    cmd_head.next.set(0);
    cmd_tail = (wmtcmd_base *)&cmd_head;
  }
};
struct ResolveEncoderData : EncoderData {
  TextureViewRef src;
  TextureViewRef dst;

  ResolveEncoderData() {
    type = EncoderType::Resolve;
  }
};

}; // namespace dxmt

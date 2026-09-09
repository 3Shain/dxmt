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

#include "Metal.hpp"
#include "dxmt_buffer.hpp"
#include "dxmt_texture.hpp"

namespace dxmt {

template <typename Context> struct SimpleCommandContext {
  Context &ctx;

  WMT::Reference<WMT::ComputePipelineState> getComputePipeline(std::string name);
  void startComputePass();
  void endPass();
  void setComputePSO(WMT::ComputePipelineState pso, WMTSize tgsize);
  void dispatch(WMTSize size);
  void setComputeTexture(uint32_t index, const Rc<Texture> &texture, uint64_t viewId, int flags);
  void setComputeTexelBuffer(uint32_t index, const Rc<Buffer> &buffer, uint64_t viewId, int flags);
  void setComputeBuffer(uint32_t index, const Rc<Buffer> &buffer, uint32_t offset, uint32_t length, int flags);
  void *setComputeBytes(uint32_t index, uint32_t length);

  WMT::Library getDefaultLibrary();

  void startRenderPass();
  /**
  All attachements (including depth-stencil) must have the same array length and sample count
  There must be no 'holes' (if index 2 is set, 0 and 1 must be also set)
  */
  void setColorAttachment(uint32_t index, const Rc<Texture> &texture, uint64_t viewId, uint32_t depth_plane);
  void setDepthStencilAttachment(const Rc<Texture> &texture, uint64_t viewId, uint32_t dsv_flag);
  void setRenderPSO(WMT::RenderPipelineState pso);
  void setViewport(WMTViewport viewport);
  void setDepthStencilState(WMT::DepthStencilState dsso);
  void setScissorRect(WMTScissorRect rect);
  void setStencilReference(uint8_t stencil_ref);
  void *setFragmentBytes(uint32_t index, uint32_t length);
  void draw(
      WMTPrimitiveType primitive, uint32_t vertex_start, uint32_t vertex_count, int32_t base_instance,
      uint32_t instance_count
  );
};

} // namespace dxmt
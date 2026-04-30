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
#include "com/com_pointer.hpp"
#include "d3d11_3.h"
#include "dxmt_buffer.hpp"
#include "dxmt_counter.hpp"
#include "dxmt_subresource.hpp"
#include "dxmt_texture.hpp"

struct MTL_RENDER_PASS_ATTACHMENT_DESC {
  uint32_t RenderTargetArrayLength;
  uint32_t SampleCount;
  uint32_t DepthPlane;
  uint32_t Width;
  uint32_t Height;
};

namespace dxmt {

struct D3D11ResourceCommon;

struct D3D11ShaderResourceView : ID3D11ShaderResourceView1 {
  Com<D3D11ResourceCommon, false> resource_{};
  Buffer *buffer_{};
  BufferSlice slice_{};
  Texture *texture_{};
  uint64_t view_id_{};
  ResourceSubsetState subset_{};
  uint32_t bind_flags_{};

  Rc<Buffer>
  buffer() const {
    return buffer_;
  };
  BufferSlice
  bufferSlice() const {
    return slice_;
  };
  Rc<Texture>
  texture() const {
    return texture_;
  };
  uint64_t
  viewId() const {
    return view_id_;
  };
  uint32_t bindFlags() const {
    return bind_flags_;
  }
  bool
  hazardsFree() const {
    return (bind_flags_ & (D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET |
                           D3D11_BIND_DEPTH_STENCIL)) == 0;
  }

  virtual void AddRefPrivate() = 0;
  virtual void ReleasePrivate() = 0;
};

struct D3D11UnorderedAccessView : ID3D11UnorderedAccessView1 {
  Com<D3D11ResourceCommon, false> resource_{};
  Buffer *buffer_{};
  BufferSlice slice_{};
  Texture *texture_{};
  uint64_t view_id_{};
  Rc<Counter> counter_;
  ResourceSubsetState subset_{};
  uint32_t bind_flags_{};

  Rc<Buffer>
  buffer() const {
    return buffer_;
  };
  BufferSlice
  bufferSlice() const {
    return slice_;
  };
  Rc<Texture>
  texture() const {
    return texture_;
  };
  uint64_t
  viewId() const {
    return view_id_;
  };
  Rc<Counter>
  counter() const {
    return counter_;
  };
  uint32_t bindFlags() const {
    return bind_flags_;
  }
  bool
  hasCounter() const {
    return buffer_ && counter_;
  }

  virtual void AddRefPrivate() = 0;
  virtual void ReleasePrivate() = 0;
};

struct D3D11RenderTargetView : ID3D11RenderTargetView1 {
  Com<D3D11ResourceCommon, false> resource_{};
  Texture *texture_{};
  uint64_t view_id_{};
  MTL_RENDER_PASS_ATTACHMENT_DESC pass_desc_;
  WMTPixelFormat format_{};
  ResourceSubsetState subset_{};
  uint32_t bind_flags_{};

  WMTPixelFormat
  pixelFormat() const {
    return format_;
  };
  const MTL_RENDER_PASS_ATTACHMENT_DESC &
  description() const {
    return pass_desc_;
  };
  Rc<Texture>
  texture() const {
    return texture_;
  };
  uint64_t
  viewId() const {
    return view_id_;
  };
  uint32_t bindFlags() const {
    return bind_flags_;
  }

  virtual void AddRefPrivate() = 0;
  virtual void ReleasePrivate() = 0;
};

struct D3D11DepthStencilView : ID3D11DepthStencilView {
  Com<D3D11ResourceCommon, false> resource_{};
  Texture *texture_{};
  uint64_t view_id_{};
  MTL_RENDER_PASS_ATTACHMENT_DESC pass_desc_;
  WMTPixelFormat format_{};
  uint32_t readonly_flags_{};
  RenamableTexturePool *renamable_{};
  ResourceSubsetState subset_{};
  uint32_t bind_flags_{};

  WMTPixelFormat
  pixelFormat() const {
    return format_;
  };
  const MTL_RENDER_PASS_ATTACHMENT_DESC &
  description() const {
    return pass_desc_;
  };
  Rc<Texture>
  texture() const {
    return texture_;
  };
  uint64_t
  viewId() const {
    return view_id_;
  };
  UINT
  readonlyFlags() {
    return readonly_flags_;
  }
  Rc<RenamableTexturePool>
  renamable() {
    return renamable_;
  }
  uint32_t bindFlags() const {
    return bind_flags_;
  }

  virtual void AddRefPrivate() = 0;
  virtual void ReleasePrivate() = 0;
};

template <typename A, typename B>
inline bool
CheckOverlap(const A *pViewA, const B *pViewB) {
  return pViewA && pViewB && pViewA->resource_ == pViewB->resource_ && pViewA->subset_.overlapWith(pViewB->subset_);
}

template <typename View>
inline bool
CheckOverlap(D3D11ResourceCommon *pBuffer, const View *pView) {
  return pBuffer && pView && pBuffer == pView->resource_.ptr();
}

template <typename View>
inline bool
CheckOverlap(const View *pView, D3D11ResourceCommon *pBuffer) {
  return pBuffer && pView && pBuffer == pView->resource_.ptr();
}

inline bool
CheckOverlap(D3D11ResourceCommon *pBufferA, D3D11ResourceCommon *pBufferB) {
  return pBufferA && pBufferB && pBufferA == pBufferB;
}

}

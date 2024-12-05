#pragma once

#include "Metal/MTLBlitCommandEncoder.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLSampler.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"
#include "dxmt_buffer.hpp"
#include "dxmt_command.hpp"
#include "dxmt_command_list.hpp"
#include "dxmt_occlusion_query.hpp"
#include "dxmt_residency.hpp"
#include "dxmt_texture.hpp"
#include "log/log.hpp"
#include "rc/util_rc_ptr.hpp"
#include "airconv_public.h"
#include <cassert>

#define DXMT_IMPLEMENT_ME __builtin_unreachable();

namespace dxmt {

constexpr size_t kCommandChunkCPUHeapSize = 0x1000000;
constexpr size_t kCommandChunkGPUHeapSize = 0x400000;

inline std::size_t
align_forward_adjustment(const void *const ptr, const std::size_t &alignment) noexcept {
  const auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
  const auto aligned = (iptr - 1u + alignment) & -alignment;
  return aligned - iptr;
}

inline void *
ptr_add(const void *const p, const std::uintptr_t &amount) noexcept {
  return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(p) + amount);
}

enum class PipelineStage { Vertex = 0, Pixel = 1, Geometry = 2, Hull = 3, Domain = 4, Compute = 5 };

constexpr unsigned kStages = 6;
constexpr unsigned kSRVBindings = 128;
constexpr unsigned kUAVBindings = 64;
constexpr unsigned kVertexBufferSlots = 32;

struct VertexBufferBinding {
  Rc<Buffer> buffer;
  unsigned offset;
  unsigned stride;
};

struct ConstantBufferBinding {
  Rc<Buffer> buffer;
  unsigned offset;
};

struct SamplerBinding {
  Obj<MTL::SamplerState> sampler;
};

struct ResourceViewBinding {
  unsigned viewId;
  Rc<Buffer> buffer;
  Rc<Texture> texture;
  BufferSlice slice;
};

struct UnorderedAccessViewBinding {
  unsigned viewId;
  Rc<Buffer> buffer;
  Rc<Texture> texture;
  Rc<Buffer> counter;
  BufferSlice slice;
};

enum class EncoderType {
  Null,
  Render,
  Compute,
  Blit,
  Clear,
  Resolve,
  Present,
};

struct EncoderData {
  EncoderType type;
  EncoderData *next = nullptr;
  uint64_t id;
  // read list
  // write list
};

struct RenderCommandContext {
  MTL::RenderCommandEncoder *encoder;
  uint32_t dsv_planar_flags;
};

struct RenderEncoderData : EncoderData {
  Obj<MTL::RenderPassDescriptor> descriptor;
  CommandList<RenderCommandContext> cmds;
  CommandList<RenderCommandContext> pretess_cmds;
  uint32_t dsv_planar_flags;
  bool use_visibility_result = 0;
  bool use_tessellation = 0;
  bool coalesced_start = 0;
  bool coalesced_end = 0;
};

struct ComputeCommandContext {
  MTL::ComputeCommandEncoder *encoder;
  MTL::Size threadgroup_size;
  EmulatedCommandContext& cmd;
};

struct ComputeEncoderData : EncoderData {
  CommandList<ComputeCommandContext> cmds;
  bool coalesced_start;
  bool coalesced_end;
};

struct BlitCommandContext {
  MTL::BlitCommandEncoder *encoder;
};

struct BlitEncoderData : EncoderData {
  CommandList<BlitCommandContext> cmds;
};

struct ClearEncoderData : EncoderData {
  union {
    MTL::ClearColor color;
    std::pair<float, uint8_t> depth_stencil;
  };
  Obj<MTL::Texture> texture;
  unsigned clear_dsv;
  unsigned array_length;
  bool skipped = false;

  ClearEncoderData() {}
};

struct ResolveEncoderData : EncoderData {
  Obj<MTL::Texture> src;
  Obj<MTL::Texture> dst;
  unsigned src_slice;
  unsigned dst_slice;
  unsigned dst_level;
  bool skipped = false;
};

struct PresentData : EncoderData {
  Obj<MTL::Texture> backbuffer;
  Obj<CA::MetalDrawable> drawable;
  double after;
};

template <bool Tessellation>
constexpr DXMT_RESOURCE_RESIDENCY
GetResidencyMask(PipelineStage type, bool read, bool write) {
  switch (type) {
  case PipelineStage::Vertex:
    if constexpr (Tessellation)
      return (read ? DXMT_RESOURCE_RESIDENCY_OBJECT_READ : DXMT_RESOURCE_RESIDENCY_NULL) |
             (write ? DXMT_RESOURCE_RESIDENCY_OBJECT_WRITE : DXMT_RESOURCE_RESIDENCY_NULL);
    else
      return (read ? DXMT_RESOURCE_RESIDENCY_VERTEX_READ : DXMT_RESOURCE_RESIDENCY_NULL) |
             (write ? DXMT_RESOURCE_RESIDENCY_VERTEX_WRITE : DXMT_RESOURCE_RESIDENCY_NULL);
  case PipelineStage::Pixel:
    return (read ? DXMT_RESOURCE_RESIDENCY_FRAGMENT_READ : DXMT_RESOURCE_RESIDENCY_NULL) |
           (write ? DXMT_RESOURCE_RESIDENCY_FRAGMENT_WRITE : DXMT_RESOURCE_RESIDENCY_NULL);
  case PipelineStage::Hull:
    return (read ? DXMT_RESOURCE_RESIDENCY_MESH_READ : DXMT_RESOURCE_RESIDENCY_NULL) |
           (write ? DXMT_RESOURCE_RESIDENCY_MESH_WRITE : DXMT_RESOURCE_RESIDENCY_NULL);
  case PipelineStage::Domain:
    return (read ? DXMT_RESOURCE_RESIDENCY_VERTEX_READ : DXMT_RESOURCE_RESIDENCY_NULL) |
           (write ? DXMT_RESOURCE_RESIDENCY_VERTEX_WRITE : DXMT_RESOURCE_RESIDENCY_NULL);
  case PipelineStage::Geometry:
    assert(0 && "TODO");
    break;
  case PipelineStage::Compute:
    return (read ? DXMT_RESOURCE_RESIDENCY_READ : DXMT_RESOURCE_RESIDENCY_NULL) |
           (write ? DXMT_RESOURCE_RESIDENCY_WRITE : DXMT_RESOURCE_RESIDENCY_NULL);
  }
}

enum DXMT_ENCODER_LIST_OP {
  DXMT_ENCODER_LIST_OP_SYNCHRONIZE = 0,
  DXMT_ENCODER_LIST_OP_SWAP = 1,
  DXMT_ENCODER_LIST_OP_COALESCE = 2,
};

class CommandQueue;

class ArgumentEncodingContext {
public:
  template <PipelineStage stage>
  void
  bindConstantBuffer(unsigned slot, unsigned offset, Rc<Buffer> &&buffer) {
    unsigned idx = slot + 14 * unsigned(stage);
    auto &entry = cbuf_[idx];
    entry.offset = offset;
    entry.buffer = std::move(buffer);
  }

  template <PipelineStage stage>
  void
  bindConstantBufferOffset(unsigned slot, unsigned offset) {
    unsigned idx = slot + 14 * unsigned(stage);
    auto &entry = cbuf_[idx];
    entry.offset = offset;
  }

  template <PipelineStage stage>
  void
  bindSampler(unsigned slot, MTL::SamplerState *sampler) {
    unsigned idx = slot + 16 * unsigned(stage);
    auto &entry = sampler_[idx];
    entry.sampler = sampler;
  }

  template <PipelineStage stage>
  void
  bindBuffer(unsigned slot, Rc<Buffer> &&buffer, unsigned viewId, BufferSlice slice) {
    unsigned idx = slot + kSRVBindings * unsigned(stage);
    auto &entry = resview_[idx];
    entry.texture = {};
    entry.buffer = std::move(buffer);
    entry.viewId = viewId;
    entry.slice = slice;
  }

  template <PipelineStage stage>
  void
  bindTexture(unsigned slot, Rc<Texture> &&texture, unsigned viewId) {
    unsigned idx = slot + kSRVBindings * unsigned(stage);
    auto &entry = resview_[idx];
    entry.buffer = {};
    entry.texture = std::move(texture);
    entry.viewId = viewId;
  }

  void
  bindRenderTarget(unsigned rtIndex, Rc<Texture> &&texture, unsigned viewId) {
    auto &entry = rtv_[rtIndex];
    entry.texture = std::move(texture);
    entry.viewId = viewId;
  }
  void
  bindDepthStencilTarget(Rc<Texture> &&texture, unsigned viewId) {
    dsv_.texture = std::move(texture);
    dsv_.viewId = viewId;
  }

  template <PipelineStage stage>
  void bindOutputBuffer(unsigned slot, Rc<Buffer> &&buffer, unsigned viewId, Rc<Buffer> &&counter, BufferSlice slice);
  template <>
  void
  bindOutputBuffer<PipelineStage::Compute>(unsigned slot, Rc<Buffer> &&buffer, unsigned viewId, Rc<Buffer> &&counter, BufferSlice slice) {
    auto &entry = cs_uav_[slot];
    entry.texture = {};
    entry.buffer = std::move(buffer);
    entry.viewId = viewId;
    entry.counter = std::move(counter);
    entry.slice = slice;
  }
  template <>
  void
  bindOutputBuffer<PipelineStage::Pixel>(unsigned slot, Rc<Buffer> &&buffer, unsigned viewId, Rc<Buffer> &&counter, BufferSlice slice) {
    auto &entry = om_uav_[slot];
    entry.texture = {};
    entry.buffer = std::move(buffer);
    entry.viewId = viewId;
    entry.counter = std::move(counter);
    entry.slice = slice;
  }

  template <PipelineStage stage> void bindOutputTexture(unsigned slot, Rc<Texture> &&texture, unsigned viewId);
  template <>
  void
  bindOutputTexture<PipelineStage::Compute>(unsigned slot, Rc<Texture> &&texture, unsigned viewId) {
    auto &entry = cs_uav_[slot];
    entry.buffer = {};
    entry.texture = std::move(texture);
    entry.viewId = viewId;
  }
  template <>
  void
  bindOutputTexture<PipelineStage::Pixel>(unsigned slot, Rc<Texture> &&texture, unsigned viewId) {
    auto &entry = om_uav_[slot];
    entry.buffer = {};
    entry.texture = std::move(texture);
    entry.viewId = viewId;
  }

  void bindStreamOutputBuffer(unsigned slot, unsigned offset, Rc<Buffer> &&buffer);
  void bindStreamOutputBufferOffset(unsigned slot, unsigned offset);

  void
  bindVertexBuffer(unsigned slot, unsigned offset, unsigned stride, Rc<Buffer> &&buffer) {
    auto &entry = vbuf_[slot];
    entry.buffer = std::move(buffer);
    entry.offset = offset;
    entry.stride = stride;
  }

  void
  bindVertexBufferOffset(unsigned slot, unsigned offset, unsigned stride) {
    auto &entry = vbuf_[slot];
    entry.offset = offset;
    entry.stride = stride;
  }

  void
  bindIndexBuffer(Rc<Buffer> &&buffer) {
    ibuf_ = std::move(buffer);
  }

  MTL::Buffer *
  currentIndexBuffer() {
    return ibuf_->current()->buffer();
  };

  template <bool TessellationDraw> void encodeVertexBuffers(uint32_t ia_slot_mask);
  template <PipelineStage stage, bool TessellationDraw>
  void encodeConstantBuffers(const MTL_SHADER_REFLECTION *reflection);
  template <PipelineStage stage, bool TessellationDraw>
  void encodeShaderResources(const MTL_SHADER_REFLECTION *reflection);

  template <PipelineStage stage, bool TessellationDraw>
  constexpr void
  makeResident(MTL::Resource *resource, DXMT_RESOURCE_RESIDENCY requested) {
    if constexpr (stage == PipelineStage::Compute)
      encodeComputeCommand([resource = Obj(resource), requested](ComputeCommandContext &ctx) {
        ctx.encoder->useResource(resource, GetUsageFromResidencyMask(requested));
      });
    else if constexpr (TessellationDraw)
      encodePreTessCommand([resource = Obj(resource), requested](RenderCommandContext &ctx) {
        ctx.encoder->useResource(resource, GetUsageFromResidencyMask(requested), GetStagesFromResidencyMask(requested));
      });
    else
      encodeRenderCommand([resource = Obj(resource), requested](RenderCommandContext &ctx) {
        ctx.encoder->useResource(resource, GetUsageFromResidencyMask(requested), GetStagesFromResidencyMask(requested));
      });
  }

  template <PipelineStage stage, bool TessellationDraw>
  void
  makeResident(Buffer *buffer, bool read = true, bool write = false) {
    auto allocation = buffer->current();
    uint64_t encoder_id = currentEncoder()->id;
    DXMT_RESOURCE_RESIDENCY requested = GetResidencyMask<TessellationDraw>(stage, read, write);
    if (CheckResourceResidency(allocation->residencyState, encoder_id, requested)) {
      makeResident<stage, TessellationDraw>(allocation->buffer(), requested);
    };
  }
  template <PipelineStage stage, bool TessellationDraw>
  void
  makeResident(Buffer *buffer, unsigned viewId, bool read = true, bool write = false) {
    uint64_t encoder_id = currentEncoder()->id;
    DXMT_RESOURCE_RESIDENCY requested = GetResidencyMask<TessellationDraw>(stage, read, write);
    if (CheckResourceResidency(buffer->residency(viewId), encoder_id, requested)) {
      makeResident<stage, TessellationDraw>((MTL::Resource *)buffer->view(viewId), requested);
    };
  }
  template <PipelineStage stage, bool TessellationDraw>
  void
  makeResident(Texture *texture, unsigned viewId, bool read = true, bool write = false) {
    uint64_t encoder_id = currentEncoder()->id;
    DXMT_RESOURCE_RESIDENCY requested = GetResidencyMask<TessellationDraw>(stage, read, write);
    if (CheckResourceResidency(texture->residency(viewId), encoder_id, requested)) {
      makeResident<stage, TessellationDraw>((MTL::Resource *)texture->view(viewId), requested);
    };
  }

  template <CommandWithContext<RenderCommandContext> cmd>
  void
  encodeRenderCommand(cmd &&fn) {
    assert(encoder_current->type == EncoderType::Render);
    auto &cmds = static_cast<RenderEncoderData *>(encoder_current)->cmds;
    cmds.emit(std::forward<cmd>(fn), allocate_cpu_heap(cmds.calculateCommandSize<cmd>(), 16));
  }

  template <CommandWithContext<RenderCommandContext> cmd>
  void
  encodePreTessCommand(cmd &&fn) {
    assert(encoder_current->type == EncoderType::Render);
    auto &cmds = static_cast<RenderEncoderData *>(encoder_current)->pretess_cmds;
    cmds.emit(std::forward<cmd>(fn), allocate_cpu_heap(cmds.calculateCommandSize<cmd>(), 16));
  }

  template <CommandWithContext<ComputeCommandContext> cmd>
  void
  encodeComputeCommand(cmd &&fn) {
    assert(encoder_current->type == EncoderType::Compute);
    auto &cmds = static_cast<ComputeEncoderData *>(encoder_current)->cmds;
    cmds.emit(std::forward<cmd>(fn), allocate_cpu_heap(cmds.calculateCommandSize<cmd>(), 16));
  }

  template <CommandWithContext<BlitCommandContext> cmd>
  void
  encodeBlitCommand(cmd &&fn) {
    assert(encoder_current->type == EncoderType::Blit);
    auto &cmds = static_cast<BlitEncoderData *>(encoder_current)->cmds;
    cmds.emit(std::forward<cmd>(fn), allocate_cpu_heap(cmds.calculateCommandSize<cmd>(), 16));
  }

  template <typename T>
  T *
  allocate() {
    return (new (allocate_cpu_heap(sizeof(T), alignof(T))) T());
  };

  void present(Rc<Texture> &texture, CA::MetalDrawable *drawable, double after);

  uint64_t
  nextEncoderId() {
    static std::atomic_uint64_t global_id = 0;
    return global_id.fetch_add(1);
  };

  void clearColor(Rc<Texture> &&texture, unsigned viewId, unsigned arrayLength, MTL::ClearColor color);
  void clearDepthStencil(
      Rc<Texture> &&texture, unsigned viewId, unsigned arrayLength, unsigned flag, float depth, uint8_t stencil
  );
  void resolveTexture(Rc<Texture> &&src, unsigned srcSlice, Rc<Texture> &&dst, unsigned dstSlice, unsigned dstLevel);

  RenderEncoderData *startRenderPass(Obj<MTL::RenderPassDescriptor> &&descriptor, uint32_t dsv_planar_flags);
  EncoderData *startComputePass();
  EncoderData *startBlitPass();

  void endPass();

  constexpr EncoderData *
  currentEncoder() {
    assert(encoder_current);
    return encoder_current;
  }

  constexpr RenderEncoderData *
  currentRenderEncoder() {
    assert(encoder_current->type == EncoderType::Render);
    return static_cast<RenderEncoderData *>(encoder_current);
  }

  void *
  allocate_cpu_heap(size_t size, size_t alignment) {
    std::size_t adjustment = align_forward_adjustment((void *)cpu_buffer_offset_, alignment);
    auto aligned = cpu_buffer_offset_ + adjustment;
    cpu_buffer_offset_ = aligned + size;
    if (cpu_buffer_offset_ >= kCommandChunkCPUHeapSize) {
      ERR(cpu_buffer_offset_, " - cpu argument heap overflow, expect error.");
    }
    return ptr_add(cpu_buffer_, aligned);
  }

  uint64_t
  allocate_gpu_heap(size_t size, size_t alignment) {
    std::size_t adjustment = align_forward_adjustment((void *)gpu_bufer_offset_, alignment);
    auto aligned = gpu_bufer_offset_ + adjustment;
    gpu_bufer_offset_ = aligned + size;
    if (gpu_bufer_offset_ > kCommandChunkGPUHeapSize) {
      ERR("gpu argument heap overflow, expect error.");
    }
    return aligned;
  }

  template<typename T> T* get_gpu_heap_pointer(size_t offset) {
    return reinterpret_cast<T*>((char*)gpu_buffer_->contents() + offset);
  }

  std::pair<MTL::Buffer* , size_t> allocateTempBuffer(size_t size, size_t alignment);

  std::unique_ptr<VisibilityResultReadback> flushCommands(MTL::CommandBuffer *cmdbuf, uint64_t seqId);

  void
  $$setEncodingBuffer(
      void *cpu_buffer, uint64_t cpu_buffer_offset, MTL::Buffer *gpu_buffer, uint64_t gpu_bufer_offset, uint64_t seq_id
  ) {
    cpu_buffer_ = cpu_buffer;
    cpu_buffer_offset_ = cpu_buffer_offset;
    gpu_buffer_ = gpu_buffer;
    gpu_bufer_offset_ = gpu_bufer_offset;
    seq_id_ = seq_id;
  }

  void bumpVisibilityResultOffset();
  void beginVisibilityResultQuery(Rc<VisibilityResultQuery> &&query);
  void endVisibilityResultQuery(Rc<VisibilityResultQuery> &&query);

  ArgumentEncodingContext(CommandQueue &queue) :
      queue(queue){};

  uint32_t tess_num_output_control_point_element;
  uint32_t tess_num_output_patch_constant_scalar;
  uint32_t tess_threads_per_patch;

private:
  DXMT_ENCODER_LIST_OP checkEncoderRelation(EncoderData* former, EncoderData* latter);
  bool hasDataDependency(EncoderData* from, EncoderData* to);

  std::array<VertexBufferBinding, kVertexBufferSlots> vbuf_;
  Rc<Buffer> ibuf_;

  std::array<ConstantBufferBinding, 14 * kStages> cbuf_;
  std::array<SamplerBinding, 16 * kStages> sampler_;
  std::array<ResourceViewBinding, kSRVBindings * kStages> resview_;

  std::array<UnorderedAccessViewBinding, kUAVBindings> om_uav_;
  std::array<UnorderedAccessViewBinding, kUAVBindings> cs_uav_;

  std::array<ResourceViewBinding, 8> rtv_;
  ResourceViewBinding dsv_;

  uint64_t *current_encoding_buffer_;

  EncoderData encoder_head = {EncoderType::Null, nullptr};
  EncoderData *encoder_last = &encoder_head;
  EncoderData *encoder_current = nullptr;
  unsigned encoder_count_ = 0;

  void *cpu_buffer_;
  uint64_t cpu_buffer_offset_;
  MTL::Buffer *gpu_buffer_;
  uint64_t gpu_bufer_offset_;
  uint64_t seq_id_;

  VisibilityResultOffsetBumpState vro_state_;
  std::vector<Rc<VisibilityResultQuery>> pending_queries_;
  unsigned active_visibility_query_count_ = 0;


  CommandQueue& queue;
};

} // namespace dxmt
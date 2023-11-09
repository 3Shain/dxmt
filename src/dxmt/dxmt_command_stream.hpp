#pragma once
#include "./dxmt_precommand.hpp"
#include "./dxmt_command.hpp"
#include <array>
#include <vector>

namespace dxmt {

struct PipelineCommandState {
  MTLBindRenderTarget bindRenderTarget;
  MTLBindDepthStencilState bindDepthStencilState;
  MTLBindPipelineState bindPipelineState;
  MTLSetViewports setViewports;
  MTLSetScissorRects setScissorRects;
  MTLSetVertexBuffer setVertexBuffer[16];
  MTLSetConstantBuffer<Vertex> setVertexConstantBuffer[14];
  MTLSetConstantBuffer<Pixel> setPixelConstantBuffer[14];
  MTLSetSampler<Pixel> setPixelShaderSampler[16];
  std::array<MTLSetShaderResource<Pixel>, 64> setPixelShaderResource =
      {}; // TODO
};

class DXMTCommandStream : public RcObject {
public:
  DXMTCommandStream(MTL::Buffer *argument_buffer);

  void Emit(MTLCommand &&command);
  void EmitPreCommand(MTLPreCommand &&command);

  void Encode(PipelineCommandState &pcs, MTL::CommandBuffer *cbuffer,
              MTL::Device *device);

  uint64_t stream_seq_id;

private:
  uint64_t command_seq_id_;
  std::vector<MTLCommand> command_list_;
  std::vector<MTLPreCommand> precommand_list_;
  std::vector<NS::Object *> collect_;
  Obj<MTL::Buffer> argument_buffer;
};

enum class ActiveEncoder { None = 0, Render, Compute, Blit };

} // namespace dxmt
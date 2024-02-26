#include "./dxmt_command_stream.hpp"
#include "../util/objc_pointer.h"
#include "../util/util_error.h"
#include "Foundation/NSAutoreleasePool.hpp"
#include "Foundation/NSRange.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLResource.hpp"
#include "dxmt_binding.hpp"
#include "dxmt_command.hpp"
#include "dxmt_precommand.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <variant>
#include "Metal/MTLDevice.hpp"

namespace dxmt {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

DXMTCommandStream::DXMTCommandStream(MTL::Buffer *argument_buffer)
    : command_list_(), precommand_list_() {
  this->argument_buffer = argument_buffer;
}

void DXMTCommandStream::Encode(PipelineCommandState &pcs,
                               MTL::CommandBuffer *cbuffer,
                               MTL::Device *device) {

  uint32_t offset = 0;

  ActiveEncoder active_encoder = ActiveEncoder::None;

  // TODO: should combine them to be a sum type
  Obj<MTL::RenderCommandEncoder> render_encoder;
  Obj<MTL::BlitCommandEncoder> blit_encoder;
  Obj<MTL::ComputeCommandEncoder> compute_encoder;

  auto end_active_encoder = [&]() {
    switch (active_encoder) {
    case ActiveEncoder::None:
      break;
    case ActiveEncoder::Render:
      render_encoder->endEncoding();
      render_encoder = nullptr;

      break;
    case ActiveEncoder::Compute:
      compute_encoder->endEncoding();
      compute_encoder = nullptr;
      break;
    case ActiveEncoder::Blit:
      blit_encoder->endEncoding();
      blit_encoder = nullptr;
      break;
    }
    active_encoder = ActiveEncoder::None;
  };

  std::unordered_map<void *, Obj<MTL::Buffer>> lookup;

  AliasLookup lookupFn([&lookup](void *ref) -> void * {
    if (auto w = lookup.find(ref); w != lookup.end()) {
      return w->second.ptr();
    }
    return nullptr;
  });

  if (!precommand_list_.empty()) {
    active_encoder = ActiveEncoder::Blit;
    blit_encoder = cbuffer->blitCommandEncoder();
    // REFACTOR: actually precommands don't have to be ordered
    for (MTLPreCommand &precommand : precommand_list_) {
      std::visit(    // force
          overloaded // newline
          {[&](MTLInitializeBufferCommand &cmd) {
             blit_encoder->copyFromBuffer(cmd.staging_buffer.ptr(), 0,
                                          cmd.ondevice_buffer.ptr(), 0,
                                          cmd.ondevice_buffer->length());
           },
           [&](MTLInitializeTextureCommand &cmd) {
             blit_encoder->copyFromTexture(cmd.staging_texture.ptr(),
                                           cmd.ondevice_texture.ptr());
           },
           [&](MTLZerofillBufferCommand &cmd) {
             blit_encoder->fillBuffer(cmd.ondevice_buffer.ptr(),
                                      NS::Range::Make(cmd.offset, cmd.length),
                                      0);
           },
           [](auto command) { throw MTLD3DError("Unhandled Command"); }},
          precommand);
    }
  }

  for (MTLCommand &command : command_list_) {
    std::visit(    // force
        overloaded // newline
        {[&](MTLBufferRotated &cmd) {
           lookup.emplace(std::make_pair(cmd.resource_ref, cmd.new_buffer))
               .first->second = cmd.new_buffer;
         },

         [&](MTLRenderCommand &cmd) {
           if (active_encoder != ActiveEncoder::Render) {
             end_active_encoder();

             {
               Obj<MTL::RenderPassDescriptor> pass =
                   transfer(MTL::RenderPassDescriptor::alloc()->init());
               pcs.bindRenderTarget.thunk(pass.ptr());
               render_encoder = cbuffer->renderCommandEncoder(pass.ptr());
             }

             pcs.bindDepthStencilState.exec_thunk(render_encoder.ptr());
             auto p = pcs.bindPipelineState.accessor();
             render_encoder->setRenderPipelineState(p);
             if (!pcs.setViewports.viewports.empty()) {
               render_encoder->setViewports(&pcs.setViewports.viewports[0],
                                            pcs.setViewports.viewports.size());
             }
             if (!pcs.setScissorRects.scissorRects.empty()) {
               render_encoder->setScissorRects(
                   &pcs.setScissorRects.scissorRects[0],
                   pcs.setScissorRects.scissorRects.size());
             }
             for (unsigned i = 0; i < 16; i++) {
               if (pcs.setVertexBuffer[i].vertex_buffer_ref_ != nullptr) {
                 pcs.setVertexBuffer[i].vertex_buffer_ref_->Bind(
                     pcs.setVertexBuffer[i].index,
                     pcs.setVertexBuffer[i].offset, render_encoder.ptr());
               }
               if (pcs.setPixelShaderSampler[i].sampler != nullptr) {
                 render_encoder->setFragmentSamplerState(
                     pcs.setPixelShaderSampler[i].sampler.ptr(),
                     pcs.setPixelShaderSampler[i].slot);
               }
             }
             active_encoder = ActiveEncoder::Render;
           }
           //  Obj<MTL::Buffer> vs_ab = transfer(cbuffer->device()->newBuffer(
           //      pcs.bindPipelineState.vs_encoder->encodedLength(),
           //      MTL::ResourceStorageModeShared |
           //          MTL::ResourceCPUCacheModeWriteCombined));

           auto ps_enc = pcs.bindPipelineState.ps_encoder.ptr();
           auto vs_enc = pcs.bindPipelineState.vs_encoder.ptr();

           //  Obj<MTL::Buffer> fs_ab = (device->newBuffer(
           //      ps_enc->encodedLength(), MTL::ResourceStorageModeShared));
           //  Obj<MTL::Buffer> vs_ab = (device->newBuffer(
           //      vs_enc->encodedLength(), MTL::ResourceStorageModeShared));
           //  collect_.push_back(fs_ab.ptr());
           //  collect_.push_back(vs_ab.ptr());
           uint32_t fs_offset = offset;
           offset += ps_enc->encodedLength();
           uint32_t vs_offset = offset;
           offset += vs_enc->encodedLength();
           ps_enc->setArgumentBuffer(argument_buffer.ptr(), fs_offset);
           vs_enc->setArgumentBuffer(argument_buffer.ptr(), vs_offset);
           render_encoder->setVertexBuffer(argument_buffer.ptr(), vs_offset,
                                           30); // ARUGMENT TABLE AT 20
          //  render_encoder->setFragmentBuffer(argument_buffer.ptr(), fs_offset,
          //                                    30); // ARUGMENT TABLE AT 20
           for (uint32_t i = 0; i < 64; i++) {
             auto &r = pcs.setPixelShaderResource[i];
             if (r.shader_resource_ref_ != nullptr) {
               r.shader_resource_ref_->Bind(render_encoder.ptr(), ps_enc,
                                            r.slot + 128); // texture offset 128
             }
           }

           for (uint32_t i = 0; i < 14; i++) {
             auto &r = pcs.setVertexConstantBuffer[i];
             if (r.constant_buffer_ref_ != nullptr) {
               r.constant_buffer_ref_->Bind(render_encoder.ptr(), vs_enc,
                                            // r.index + 32,
                                            r.index,
                                            lookupFn); // constant offset 32
             }
           }
           for (uint32_t i = 0; i < 14; i++) {
             auto &r = pcs.setPixelConstantBuffer[i];
             if (r.constant_buffer_ref_ != nullptr) {
               r.constant_buffer_ref_->Bind(render_encoder.ptr(), ps_enc,
                                            r.index + 32,
                                            lookupFn); // constant offset 32
             }
           }

           cmd.exec_thunk(render_encoder.ptr());
           ps_enc->setArgumentBuffer(NULL, 0);
           vs_enc->setArgumentBuffer(NULL, 0);
           render_encoder->setVertexBuffer(NULL, 0,
                                           20); // ARUGMENT TABLE AT 20
           render_encoder->setFragmentBuffer(NULL, 0,
                                             20); // ARUGMENT TABLE AT 20
         },
         [&](MTLBlitCommand &cmd) {
           if (active_encoder != ActiveEncoder::Blit) {
             end_active_encoder();

             active_encoder = ActiveEncoder::Blit;
             blit_encoder = (cbuffer->blitCommandEncoder());
           }

           cmd.exec_thunk(blit_encoder.ptr());
         },
         [&](MTLComputeCommand &cmd) {
           if (active_encoder != ActiveEncoder::Compute) {
             end_active_encoder();

             compute_encoder = (cbuffer->computeCommandEncoder());
             active_encoder = ActiveEncoder::Compute;
           }

           cmd.exec_thunk(compute_encoder.ptr());
         },
         [&](MTLBindRenderTarget &cmd) {
           pcs.bindRenderTarget = cmd;
           if (active_encoder == ActiveEncoder::Render) {
             end_active_encoder(); // have to create a new encoder
           }
         },
         [&](MTLBindDepthStencilState &cmd) {
           pcs.bindDepthStencilState = cmd;
           if (active_encoder == ActiveEncoder::Render) {
             cmd.exec_thunk(render_encoder.ptr());
           }
         },
         [&](MTLBindPipelineState &cmd) {
           pcs.bindPipelineState = cmd;
           if (active_encoder == ActiveEncoder::Render) {
             render_encoder->setRenderPipelineState(cmd.accessor());
           }
         },
         [&](MTLSetScissorRects &cmd) {
           pcs.setScissorRects = cmd;
           if (active_encoder == ActiveEncoder::Render &&
               !cmd.scissorRects.empty()) {
             render_encoder->setScissorRects(&cmd.scissorRects[0],
                                             cmd.scissorRects.size());
           }
         },
         [&](MTLSetViewports &cmd) {
           pcs.setViewports = cmd;
           if (active_encoder == ActiveEncoder::Render &&
               !cmd.viewports.empty()) {
             render_encoder->setViewports(&cmd.viewports[0],
                                          cmd.viewports.size());
           }
         },
         [&](MTLSetVertexBuffer &cmd) {
           pcs.setVertexBuffer[cmd.index] = cmd;
           if (active_encoder == ActiveEncoder::Render) {
             // TODO: there is situation when a compute encoder can receive
             // vertex buffer
             cmd.vertex_buffer_ref_->Bind(cmd.index, cmd.offset,
                                          render_encoder.ptr());
           }
         },
         [&](MTLSetConstantBuffer<Vertex> &cmd) {
           pcs.setVertexConstantBuffer[cmd.index] = cmd;
           //  if (active_encoder == ActiveEncoder::Render) {
           //    // TODO: there is situation when a compute encoder can receive
           //    // vertex buffer
           //    cmd.constant_buffer_ref_->Bind(render_encoder, cmd.offset,
           //                                 render_encoder.ptr());
           //  }
         },
         [&](MTLSetConstantBuffer<Pixel> &cmd) {
           pcs.setPixelConstantBuffer[cmd.index] = cmd;
           //  if (active_encoder == ActiveEncoder::Render) {
           //    // TODO: there is situation when a compute encoder can receive
           //    // vertex buffer
           //    cmd.constant_buffer_ref_->Bind(render_encoder, cmd.offset,
           //                                 render_encoder.ptr());
           //  }
         },
         [&](MTLSetSampler<Pixel> &cmd) {
           pcs.setPixelShaderSampler[cmd.slot] = cmd;
           if (active_encoder == ActiveEncoder::Render) {
             render_encoder->setFragmentSamplerState(cmd.sampler.ptr(),
                                                     cmd.slot);
           }
         },
         [&](MTLSetShaderResource<Pixel> &cmd) {
           pcs.setPixelShaderResource[cmd.slot] = cmd;
           if (active_encoder == ActiveEncoder::Render) {
             // render_encoder->setFragmentSamplerState()
           }
         },
         [&](MTLCommandClearRenderTargetView &cmd) {
           //  if (active_encoder == ActiveEncoder::Render) {
           end_active_encoder();
           //  }
           // FIXME: should optimize it
           auto clear_pass =
               transfer(MTL::RenderPassDescriptor::alloc()->init());

           cmd.rtv->Bind(clear_pass.ptr(), 0, MTL::LoadActionClear,
                         MTL::StoreActionStore);
           clear_pass->colorAttachments()->object(0)->setClearColor(
               MTL::ClearColor::Make(cmd.color[0], cmd.color[1], cmd.color[2],
                                     cmd.color[3]));
           auto encoder = (cbuffer->renderCommandEncoder(clear_pass.ptr()));
           encoder->endEncoding();
         },
         [&](MTLCommandClearDepthStencilView &cmd) {
           //  if (active_encoder == ActiveEncoder::Render) {
           end_active_encoder();
           //  }
           // FIXME: should optimize it
           auto clear_pass =
               transfer(MTL::RenderPassDescriptor::alloc()->init());

           cmd.dsv->Bind(clear_pass.ptr());
           //  clear_pass->colorAttachments()->object(0)->setClearColor(
           //      MTL::ClearColor::Make(cmd.color[0], cmd.color[1],
           //      cmd.color[2],
           //                            cmd.color[3]));
           if (cmd.clearDepth) {
             clear_pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
             clear_pass->depthAttachment()->setClearDepth(cmd.depth);
             clear_pass->depthAttachment()->setStoreAction(
                 MTL::StoreActionStore);
           } else {
             clear_pass->depthAttachment()->setLoadAction(
                 MTL::LoadActionDontCare);
             clear_pass->depthAttachment()->setStoreAction(
                 MTL::StoreActionDontCare);
           }
           if (cmd.clearStencil) {
             clear_pass->stencilAttachment()->setLoadAction(
                 MTL::LoadActionClear);
             clear_pass->stencilAttachment()->setClearStencil(cmd.stencil);
             clear_pass->stencilAttachment()->setStoreAction(
                 MTL::StoreActionStore);
           } else {
             clear_pass->stencilAttachment()->setLoadAction(
                 MTL::LoadActionDontCare);
             clear_pass->stencilAttachment()->setStoreAction(
                 MTL::StoreActionDontCare);
           }
           auto encoder = (cbuffer->renderCommandEncoder(clear_pass.ptr()));
           encoder->endEncoding();
         },
         [](auto command) {
           // unknown?
           throw MTLD3DError("Unhandled Command");
         }},
        command);
  }

  end_active_encoder();
}

void DXMTCommandStream::Emit(MTLCommand &&command) {
  command_list_.push_back(command);
  command_seq_id_++;
}

void DXMTCommandStream::EmitPreCommand(MTLPreCommand &&command) {
  precommand_list_.push_back(command);
  // command_seq_id_++;
}

} // namespace dxmt
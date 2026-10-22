#include "dxil_converter.hpp"
#include "dxbc_converter.hpp"
#include "dxil_metadata.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include "airconv_error.hpp"
#include "nt/dxil_constants.hpp"
#include "nt/dxil_converter_base.hpp"

#include <memory>
#include <string>
#include <utility>

using namespace llvm;

namespace dxmt::dxil {

std::optional<dxmt::shader::common::ScalerDataType>
to_shader_scaler(ComponentType type) {
  using dxmt::shader::common::ScalerDataType;
  switch (type) {
  case ComponentType::I32:
    return ScalerDataType::Int;
  case ComponentType::U32:
    return ScalerDataType::Uint;
  case ComponentType::F32:
  case ComponentType::SNormF32:
  case ComponentType::UNormF32:
    return ScalerDataType::Float;
  case ComponentType::I16:
    return ScalerDataType::Short;
  case ComponentType::U16:
    return ScalerDataType::UShort;
  case ComponentType::F16:
  case ComponentType::SNormF16:
  case ComponentType::UNormF16:
    return ScalerDataType::Half;
  default:
    return std::nullopt;
  }
}

std::optional<dxmt::shader::common::ResourceType>
to_shader_resource_type(ResourceKind kind) {
  using dxmt::shader::common::ResourceType;
  switch (kind) {
  case ResourceKind::Texture1D:
    return ResourceType::Texture1D;
  case ResourceKind::Texture1DArray:
    return ResourceType::Texture1DArray;
  case ResourceKind::Texture2D:
    return ResourceType::Texture2D;
  case ResourceKind::Texture2DArray:
    return ResourceType::Texture2DArray;
  case ResourceKind::Texture2DMS:
    return ResourceType::Texture2DMultisampled;
  case ResourceKind::Texture2DMSArray:
    return ResourceType::Texture2DMultisampledArray;
  case ResourceKind::Texture3D:
    return ResourceType::Texture3D;
  case ResourceKind::TextureCube:
    return ResourceType::TextureCube;
  case ResourceKind::TextureCubeArray:
    return ResourceType::TextureCubeArray;
  case ResourceKind::TypedBuffer:
    return ResourceType::TextureBuffer;
  case ResourceKind::RawBuffer:
  case ResourceKind::StructuredBuffer:
    return ResourceType::NonApplicable;
  default:
    return std::nullopt;
  }
}

// defined in `nt/dxil_converter_base.hpp`
std::optional<uint32_t> asU32(llvm::Value *value);

llvm::Expected<std::pair<uint8_t, uint32_t>>
resolve_handle(const llvm::Value *handle) {
  if (auto *ci = dyn_cast<CallInst>(handle)) {
    if (auto *callee = ci->getCalledFunction()) {
      StringRef name = callee->getName();
      if (name == "dx.op.createHandle") {
        auto res_class = asU32(ci->getArgOperand(1));
        auto range = asU32(ci->getArgOperand(2));
        if (!res_class || !range)
          return unsupported("dxil: non-constant createHandle arguments");
        return std::make_pair((uint8_t)*res_class, *range);
      }
      return unsupported("dxil: unsupported resource handle operation " + name.str());
    }
  }
  return unsupported("dxil: unresolvable resource handle");
}

using dxmt::air::InputAttributeComponentType;
using dxmt::air::Interpolation;

std::optional<air::MSLScalerOrVectorType>
to_signature_msl_type(ComponentType type) {
  switch (type) {
  case ComponentType::I32:
    return air::msl_int4;
  case ComponentType::U32:
    return air::msl_uint4;
  case ComponentType::F32:
    return air::msl_float4;
  case ComponentType::F16:
    return air::msl_half4;
  case ComponentType::I16:
    return air::msl_short4;
  case ComponentType::U16:
    return air::msl_ushort4;
  default:
    return std::nullopt;
  }
}

std::optional<dxbc::RegisterComponentType>
to_register_component_type(ComponentType type) {
  switch (type) {
  case ComponentType::I32:
    return dxbc::RegisterComponentType::Int;
  case ComponentType::U32:
    return dxbc::RegisterComponentType::Uint;
  case ComponentType::F32:
    return dxbc::RegisterComponentType::Float;
  default:
    return std::nullopt;
  }
}

std::optional<InputAttributeComponentType>
to_attribute_component_type(ComponentType type) {
  switch (type) {
  case ComponentType::I32:
    return InputAttributeComponentType::Int;
  case ComponentType::U32:
    return InputAttributeComponentType::Uint;
  case ComponentType::F32:
    return InputAttributeComponentType::Float;
  default:
    return std::nullopt;
  }
}

uint32_t
element_usage_mask(const SignatureElement &element) {
  return element.usage_mask != 0 ? element.usage_mask : ((1u << element.cols) - 1u);
}

uint32_t
element_register_mask(const SignatureElement &element) {
  return element_usage_mask(element) << element.start_col;
}

llvm::Expected<std::string>
element_attribute_name(const SignatureElement &element, unsigned row) {
  if (row >= element.semantic_indices.size())
    return unsupported("dxil: signature row out of range");
  return element.name + std::to_string(element.semantic_indices[row]);
}

llvm::Error
initialize_vertex_signature(dxmt::dxbc::SM50ShaderInternal *shader) {
  using namespace dxmt::dxbc;

  const EntryPoint &entry = shader->dxil_entry;
  uint32_t input_count = (uint32_t)entry.inputs.size();
  uint32_t max_input_register = 0;
  for (uint32_t i = 0; i < input_count; ++i) {
    const auto &element = entry.inputs[i];
    if (element.start_row < 0 || element.start_col < 0)
      return unsupported("dxil: unsupported vertex shader input layout");
    uint32_t reg = element.start_row;
    uint32_t mask = element_register_mask(element);
    max_input_register = std::max(max_input_register, reg + element.rows);

    switch (element.semantic_kind) {
    case SemanticKind::VertexID:
    case SemanticKind::InstanceID: {
      bool instance = element.semantic_kind == SemanticKind::InstanceID;
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        ctx.prologue.Add([=](IOHelper &io) -> llvm::Error {
          return instance ? io.InitializeInstanceIDInput(reg, mask) : io.InitializeVertexIDInput(reg, mask);
        });
      });
      break;
    }
    case SemanticKind::Arbitrary: {
      auto attribute_type = to_attribute_component_type(element.component_type);
      if (!attribute_type)
        return unsupported("dxil: unsupported vertex shader input component type");
      for (unsigned row = 0; row < element.rows; row++) {
        auto name = element_attribute_name(element, row);
        if (auto err = name.takeError())
          return err;
        shader->signature_handlers.push_back([=, name = *name, reg = reg + row](SignatureContext &ctx) {
          if (ctx.ia_layout) {
            for (unsigned j = 0; j < ctx.ia_layout->num_elements; ++j) {
              if (ctx.ia_layout->elements[j].reg != reg)
                continue;
              auto element_info = ctx.ia_layout->elements[j];
              auto slot_mask = ctx.ia_layout->slot_mask;
              auto vbuf_table = ctx.func_signature.DefineInput(
                  air::ArgumentBindingBuffer{
                      .buffer_size = {},
                      .location_index = SM50_BINDING_INDEX_VERTEX_BUFFER,
                      .array_size = 0,
                      .memory_access = air::MemoryAccess::read,
                      .address_space = air::AddressSpace::constant,
                      .type = air::msl_uint,
                      .arg_name = "vertex_buffers",
                      .raster_order_group = {},
                  }
              );
              ctx.prologue.Add([=](IOHelper &io) -> llvm::Error {
                return io.PullVertexInput(vbuf_table, reg, mask, element_info, slot_mask);
              });
              return;
            }
          }
          uint32_t arg = ctx.func_signature.DefineInput(
              air::InputVertexStageIn{.attribute = reg, .type = *attribute_type, .name = name}
          );
          ctx.prologue.Add([=](IOHelper &io) -> llvm::Error { return io.InitializeInput(arg, reg, mask); });
        });
      }
      break;
    }
    default:
      return unsupported(
          "dxil: unsupported vertex shader input semantic " + std::to_string((uint32_t)element.semantic_kind)
      );
    }
  }
  shader->max_input_register = max_input_register;

  uint32_t output_count = (uint32_t)entry.outputs.size();
  uint32_t max_output_register = 0;
  for (uint32_t i = 0; i < output_count; ++i) {
    const auto &element = entry.outputs[i];
    if (element.start_row < 0 || element.start_col < 0)
      return unsupported("dxil: unsupported vertex shader output layout");
    uint32_t reg = element.start_row;
    uint32_t mask = element_register_mask(element);
    max_output_register = std::max(max_output_register, reg + element.rows);
    switch (element.semantic_kind) {
    case SemanticKind::Position: {
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        uint32_t out = ctx.func_signature.DefineOutput(air::OutputPosition{.type = air::msl_float4});
        if (ctx.skip_vertex_output)
          return;
        ctx.epilogue.Add([=](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
          return io.PopulateOutputSanitizePosition(reg, mask, ret, out);
        });
      });
      break;
    }
    case SemanticKind::Arbitrary: {
      auto msl_type = to_signature_msl_type(element.component_type);
      if (!msl_type)
        return unsupported("dxil: unsupported vertex shader output component type");
      for (unsigned row = 0; row < element.rows; row++) {
        auto user = element_attribute_name(element, row);
        if (auto err = user.takeError())
          return err;
        shader->signature_handlers.push_back([=, user = *user, reg = reg + row](SignatureContext &ctx) {
          uint32_t out = ctx.func_signature.DefineOutput(air::OutputVertex{.user = user, .type = *msl_type});
          if (ctx.skip_vertex_output)
            return;
          ctx.epilogue.Add([=](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
            return io.PopulateOutput(reg, mask, ret, out);
          });
        });
      }
      break;
    }
    case SemanticKind::RenderTargetArrayIndex:
    case SemanticKind::ViewPortArrayIndex: {
      bool render_target = element.semantic_kind == SemanticKind::RenderTargetArrayIndex;
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        uint32_t out = ctx.func_signature.DefineOutput(
            render_target ? air::FunctionOutput(air::OutputRenderTargetArrayIndex{})
                          : air::FunctionOutput(air::OutputViewportArrayIndex{})
        );
        if (ctx.skip_vertex_output)
          return;
        ctx.epilogue.Add([=](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
          return io.PopulateOutput(reg, mask, ret, out);
        });
      });
      break;
    }
    case SemanticKind::ClipDistance:
    case SemanticKind::CullDistance: {
      bool clip = element.semantic_kind == SemanticKind::ClipDistance;
      auto &scalars = clip ? shader->clip_distance_scalars : shader->cull_distance_scalars;
      for (unsigned row = 0; row < element.rows; row++) {
        auto user = element_attribute_name(element, row);
        if (auto err = user.takeError())
          return err;
        for (uint32_t component = 0; component < element.cols; component++) {
          scalars.push_back({(uint8_t)(element.start_col + component), (uint8_t)(reg + row)});
        }
        shader->signature_handlers.push_back([=, user = *user, reg = reg + row](SignatureContext &ctx) {
          uint32_t out = ctx.func_signature.DefineOutput(air::OutputVertex{.user = user, .type = air::msl_float4});
          if (ctx.skip_vertex_output)
            return;
          ctx.epilogue.Add([=](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
            return io.PopulateOutput(reg, mask, ret, out);
          });
        });
      }
      break;
    }
    default:
      return unsupported(
          "dxil: unsupported vertex shader output semantic " + std::to_string((uint32_t)element.semantic_kind)
      );
    }
  }
  shader->max_output_register = max_output_register;
  return llvm::Error::success();
}

std::optional<Interpolation>
to_air_interpolation(InterpolationMode mode) {
  switch (mode) {
  case InterpolationMode::Constant:
    return Interpolation::flat;
  case InterpolationMode::Linear:
    return Interpolation::center_perspective;
  case InterpolationMode::LinearCentroid:
    return Interpolation::centroid_perspective;
  case InterpolationMode::LinearNoperspective:
    return Interpolation::center_no_perspective;
  case InterpolationMode::LinearNoperspectiveCentroid:
    return Interpolation::centroid_no_perspective;
  case InterpolationMode::LinearSample:
    return Interpolation::sample_perspective;
  case InterpolationMode::LinearNoperspectiveSample:
    return Interpolation::sample_no_perspective;
  default:
    return std::nullopt;
  }
}

llvm::Error
initialize_pixel_signature(dxmt::dxbc::SM50ShaderInternal *shader) {
  using namespace dxmt::dxbc;

  const EntryPoint &entry = shader->dxil_entry;
  uint32_t input_count = (uint32_t)entry.inputs.size();
  uint32_t max_input_register = 0;
  for (uint32_t i = 0; i < input_count; ++i) {
    const auto &element = entry.inputs[i];

    if (element.semantic_kind == SemanticKind::SampleIndex) {
      using namespace shader::common;
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        uint32_t arg = ctx.func_signature.DefineInput(air::InputSampleIndex{});
        ctx.prologue.Add([=](IOHelper &io) -> llvm::Error {
          return io.InitializeInputAttribute(InputAttribute::SampleIndex, arg);
        });
      });
      continue;
    }

    if (element.start_row < 0 || element.start_col < 0)
      return unsupported("dxil: unsupported pixel shader input layout");
    uint32_t reg = (uint32_t)element.start_row;
    uint32_t mask = element_register_mask(element);
    max_input_register = std::max(max_input_register, reg + element.rows);

    switch (element.semantic_kind) {
    case SemanticKind::Position: {
      auto interpolation = to_air_interpolation(element.interpolation);
      if (!interpolation)
        return unsupported("dxil: unsupported interpolation mode");
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        uint32_t arg = ctx.func_signature.DefineInput(air::InputPosition{.interpolation = *interpolation});
        ctx.prologue.Add([=](IOHelper &io) -> llvm::Error { return io.InitializeInput(arg, reg, mask, true); });
      });
      break;
    }
    case SemanticKind::Arbitrary: {
      auto msl_type = to_signature_msl_type(element.component_type);
      auto interpolation = to_air_interpolation(element.interpolation);
      if (!msl_type)
        return unsupported("dxil: unsupported pixel shader input component type");
      if (!interpolation)
        return unsupported("dxil: unsupported interpolation mode");
      for (unsigned row = 0; row < element.rows; row++) {
        auto user = element_attribute_name(element, row);
        if (auto err = user.takeError())
          return err;
        shader->signature_handlers.push_back([=, user = *user, reg = reg + row](SignatureContext &ctx) {
          uint32_t arg = ctx.func_signature.DefineInput(
              air::InputFragmentStageIn{
                  .user = user, .type = *msl_type, .interpolation = *interpolation, .pull_mode = false
              }
          );
          ctx.prologue.Add([=](IOHelper &io) -> llvm::Error { return io.InitializeInput(arg, reg, mask); });
        });
      }
      break;
    }
    case SemanticKind::IsFrontFace: {
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        uint32_t arg = ctx.func_signature.DefineInput(air::InputFrontFacing{});
        ctx.prologue.Add([=](IOHelper &io) -> llvm::Error { return io.InitializeInput(arg, reg, mask); });
      });
      break;
    }
    case SemanticKind::RenderTargetArrayIndex:
    case SemanticKind::ViewPortArrayIndex: {
      bool render_target = element.semantic_kind == SemanticKind::RenderTargetArrayIndex;
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        uint32_t arg = ctx.func_signature.DefineInput(
            render_target ? air::FunctionInput(air::InputRenderTargetArrayIndex{})
                          : air::FunctionInput(air::InputViewportArrayIndex{})
        );
        ctx.prologue.Add([=](IOHelper &io) -> llvm::Error { return io.InitializeInput(arg, reg, mask); });
      });
      break;
    }
    case SemanticKind::ClipDistance:
    case SemanticKind::CullDistance: {
      auto interpolation = to_air_interpolation(element.interpolation);
      if (!interpolation)
        return unsupported("dxil: unsupported interpolation mode");
      for (unsigned row = 0; row < element.rows; row++) {
        auto user = element_attribute_name(element, row);
        if (auto err = user.takeError())
          return err;
        shader->signature_handlers.push_back([=, user = *user, reg = reg + row](SignatureContext &ctx) {
          uint32_t arg = ctx.func_signature.DefineInput(
              air::InputFragmentStageIn{
                  .user = user, .type = air::msl_float4, .interpolation = *interpolation, .pull_mode = false
              }
          );
          ctx.prologue.Add([=](IOHelper &io) -> llvm::Error { return io.InitializeInput(arg, reg, mask); });
        });
      }
      break;
    }
    default:
      return unsupported(
          "dxil: unsupported pixel shader input semantic " + std::to_string((uint32_t)element.semantic_kind)
      );
    }
  }
  shader->max_input_register = max_input_register;

  uint32_t output_count = (uint32_t)entry.outputs.size();
  uint32_t max_output_register = 0;
  for (uint32_t i = 0; i < output_count; ++i) {
    const auto &element = entry.outputs[i];
    if (element.rows != 1)
      return unsupported("dxil: unsupported pixel shader output layout");
    uint32_t reg = (uint32_t)element.start_row;
    uint32_t mask = element_register_mask(element);
    switch (element.semantic_kind) {
    case SemanticKind::Target: {
      if (element.start_row < 0 || element.start_col < 0 || reg >= 8)
        return unsupported("dxil: unsupported pixel shader output layout");
      auto component_type = to_register_component_type(element.component_type);
      if (!component_type)
        return unsupported("dxil: unsupported pixel shader output component type");
      max_output_register = std::max(max_output_register, reg + 1);
      shader->pso_valid_output_reg_mask |= (1u << reg);
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        auto type = component_type_from_pixel_format(ctx.pixel_formats[reg]);
        if (ctx.dual_source_blending || type == RegisterComponentType::Unknown)
          type = *component_type;
        if (ctx.dual_source_blending && reg > 1)
          return;
        uint32_t out = ctx.func_signature.DefineOutput(
            air::OutputRenderTarget{
                .dual_source_blending = ctx.dual_source_blending, .index = reg, .type = dxbc::to_msl_type(type)
            }
        );
        if (type == RegisterComponentType::Float && (ctx.unorm_output_reg_mask & (1u << reg)))
          ctx.epilogue.Add([=](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
            return io.PopulateOutputFixUnorm(reg, mask, ret, out);
          });
        else
          ctx.epilogue.Add([=](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
            return io.PopulateOutput(reg, mask, ret, out);
          });
      });
      break;
    }
    case SemanticKind::Depth:
    case SemanticKind::DepthLessEqual:
    case SemanticKind::DepthGreaterEqual: {
      air::DepthArgument depth_argument =
          element.semantic_kind == SemanticKind::DepthLessEqual      ? air::DepthArgument::less
          : element.semantic_kind == SemanticKind::DepthGreaterEqual ? air::DepthArgument::greater
                                                                     : air::DepthArgument::any;
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        ctx.prologue.Add([](IOHelper &io) -> llvm::Error { return io.CreateDepthOutputRegister(); });
      });
      shader->signature_handlers.push_back([=](SignatureContext &ctx) {
        if (ctx.disable_depth_output)
          return;
        uint32_t out = ctx.func_signature.DefineOutput(air::OutputDepth{.depth_argument = depth_argument});
        ctx.epilogue.Add([=](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
          return io.PopulateOutputDepth(ret, out);
        });
      });
      break;
    }
    case SemanticKind::Coverage: {
      shader->signature_handlers.push_back([](SignatureContext &ctx) {
        ctx.prologue.Add([](IOHelper &io) -> llvm::Error { return io.CreateCoverageMaskOutputRegister(); });
        auto out = ctx.func_signature.DefineOutput(air::OutputCoverageMask{});
        ctx.epilogue.Add(
            [=, sample_mask = ctx.pso_sample_mask](llvm::Value *ret, IOHelper &io) -> llvm::Expected<llvm::Value *> {
              return io.PopulateOutputCoverageMask(ret, out, sample_mask);
            }
        );
      });
      shader->ps_has_coverage_output = 1;
      break;
    }
    default:
      return unsupported(
          "dxil: unsupported pixel shader output semantic " + std::to_string((uint32_t)element.semantic_kind)
      );
    }
  }
  shader->max_output_register = max_output_register;
  return llvm::Error::success();
}

llvm::Error
initialize_shader(dxmt::dxbc::SM50ShaderInternal *shader, const Info &info) {
  using namespace microsoft;

  if (info.entry_points.size() != 1)
    return llvm::make_error<UnsupportedFeature>("dxil: only a single entry point is supported");
  const EntryPoint &entry = info.entry_points.front();

  shader->dxil_entry = entry;

  const char *stage = info.shader_model.stage.c_str();
  if (info.shader_model.stage == "cs") {
    shader->shader_type = microsoft::D3D11_SB_COMPUTE_SHADER;
  } else if (info.shader_model.stage == "vs") {
    shader->shader_type = microsoft::D3D10_SB_VERTEX_SHADER;
    if (auto err = initialize_vertex_signature(shader))
      return err;
  } else if (info.shader_model.stage == "ps") {
    shader->shader_type = microsoft::D3D10_SB_PIXEL_SHADER;
    if (auto err = initialize_pixel_signature(shader))
      return err;
  } else {
    return llvm::make_error<UnsupportedFeature>("dxil: unsupported shader stage " + std::string(stage));
  }

  auto &func_signature = shader->func_signature;
  for (auto &property : entry.properties) {
    switch (property.tag) {
    case EntryPropertyTag::ShaderFlags: {
      if (property.shader_flags & kShaderFlagForceEarlyDepthStencilTest)
        func_signature.UseEarlyFragmentTests();
      break;
    }
    case EntryPropertyTag::NumThreads: {
      auto n = property.num_threads;
      shader->threadgroup_size[0] = n.x;
      shader->threadgroup_size[1] = n.y;
      shader->threadgroup_size[2] = n.z;
      func_signature.UseMaxWorkgroupSize(n.x * n.y * n.z);
      break;
    }
    default:
      break;
    }
  }

  auto &shader_info = shader->shader_info;
  for (auto &resource : entry.resources) {
    dxbc::ResourceRange range{resource.id, resource.lower_bound, resource.range_size, resource.space};

    switch (resource.resource_class) {
    case ResourceClass::Sampler: {
      dxbc::SamplerInfo sampler{};
      sampler.range = range;
      shader_info.samplerMap[resource.id] = sampler;
      break;
    }
    case ResourceClass::CBV: {
      dxbc::ConstantBufferInfo cbuffer{};
      cbuffer.range = range;
      cbuffer.size_in_vec4 = (resource.cbuffer_size + 15) / 16;
      shader_info.cbufferMap[resource.id] = cbuffer;
      break;
    }
    case ResourceClass::SRV: {
      auto type = to_shader_resource_type(resource.kind);
      if (!type)
        return unsupported("dxil: unsupported shader resource kind");
      auto scaler = resource.kind == ResourceKind::StructuredBuffer || resource.kind == ResourceKind::RawBuffer
                        ? std::optional(dxmt::shader::common::ScalerDataType::Uint)
                        : to_shader_scaler((ComponentType)resource.element_type);
      if (!scaler)
        return unsupported("dxil: unsupported shader resource component type");
      dxbc::ShaderResourceViewInfo srv{};
      srv.range = range;
      srv.scaler_type = *scaler;
      srv.resource_type = *type;
      srv.structure_stride = resource.element_stride;
      shader_info.srvMap[resource.id] = srv;
      break;
    }
    case ResourceClass::UAV: {
      auto type = to_shader_resource_type(resource.kind);
      if (!type)
        return unsupported("dxil: unsupported unordered access resource kind");
      auto scaler = resource.kind == ResourceKind::StructuredBuffer || resource.kind == ResourceKind::RawBuffer
                        ? std::optional(dxmt::shader::common::ScalerDataType::Uint)
                        : to_shader_scaler((ComponentType)resource.element_type);
      if (!scaler)
        return unsupported("dxil: unsupported shader resource component type");
      dxbc::UnorderedAccessViewInfo uav{};
      uav.range = range;
      uav.scaler_type = *scaler;
      uav.resource_type = *type;
      uav.global_coherent = resource.globally_coherent;
      uav.rasterizer_order = resource.rasterizer_ordered;
      uav.with_counter = resource.has_counter;
      uav.structure_stride = resource.element_stride;
      shader_info.uavMap[resource.id] = uav;
      break;
    }
    default:
      break;
    }
  }

  for (auto &bb : *entry.function) {
    for (auto &inst : bb) {
      auto *ci = dyn_cast<CallInst>(&inst);
      if (!ci)
        continue;
      auto *callee = ci->getCalledFunction();
      if (!callee || !callee->getName().startswith("dx.op."))
        continue;
      auto opcode = asU32(ci->getArgOperand(0));
      if (!opcode)
        continue;

      auto record_usage = [&](bool read, bool written, bool sampled, bool compared) -> llvm::Error {
        auto key = resolve_handle(ci->getArgOperand(1));
        if (!key)
          return key.takeError();
        switch ((ResourceClass)key->first) {
        case ResourceClass::SRV: {
          auto found = shader_info.srvMap.find(key->second);
          if (found != shader_info.srvMap.end()) {
            found->second.read |= read;
            found->second.sampled |= sampled;
            found->second.compared |= compared;
          }
          break;
        }
        case ResourceClass::UAV: {
          auto found = shader_info.uavMap.find(key->second);
          if (found != shader_info.uavMap.end()) {
            found->second.read |= read;
            found->second.written |= written;
          }
          break;
        }
        default:
          break;
        }
        return llvm::Error::success();
      };

      switch (*opcode) {
      case kOpCreateHandle: {
        auto key = resolve_handle(ci);
        if (!key)
          return key.takeError();
        break;
      }
      case kOpTextureLoad:
      case kOpBufferLoad:
        if (auto err = record_usage(true, false, false, false))
          return err;
        break;
      case kOpTextureStore:
      case kOpBufferStore:
        if (auto err = record_usage(false, true, false, false))
          return err;
        break;
      case kOpAtomicBinOp:
      case kOpAtomicCmpXchg:
        if (auto err = record_usage(true, true, false, false))
          return err;
        break;
      case kOpSample:
      case kOpSampleBias:
      case kOpSampleLevel:
      case kOpSampleGrad:
      case kOpTextureGather:
        if (auto err = record_usage(false, false, true, false))
          return err;
        break;
      case kOpSampleCmp:
      case kOpSampleCmpLevelZero:
      case kOpTextureGatherCmp:
        if (auto err = record_usage(false, false, true, true))
          return err;
        break;
      case kOpTexture2DMSGetSamplePosition:
        shader_info.use_samplepos = true;
        break;
      case kOpCoverage: {
        using namespace dxmt::dxbc;
        using namespace shader::common;
        shader->signature_handlers.push_back([=](SignatureContext &ctx) {
          uint32_t arg = ctx.func_signature.DefineInput(air::InputInputCoverage{});
          ctx.prologue.Add([=](IOHelper &io) -> llvm::Error {
            return io.InitializeInputAttribute(InputAttribute::CoverageMask, arg);
          });
        });
        break;
      }
      default:
        break;
      }
    }
  }

  return llvm::Error::success();
}

int
initialize(
    const void *pDxilBlob, size_t DxilBlobSize, sm50_shader_t *ppShader, MTL_SHADER_REFLECTION *pRefl,
    sm50_error_t *ppError
) {
  auto errorObj = std::make_unique<dxmt::dxbc::SM50ErrorInternal>();
  llvm::raw_svector_ostream errorOut(errorObj->buf);

  auto parsed = parse_dxil_blob(llvm::StringRef((const char *)pDxilBlob, DxilBlobSize));
  if (!parsed) {
    errorOut << llvm::toString(parsed.takeError());
    SM50_RETURN_ERROR(errorObj, ppError, 1);
  }
  auto shader = std::make_unique<dxmt::dxbc::SM50ShaderInternal>();
  shader->dxil_context = std::move(parsed->context);
  shader->dxil = std::move(parsed->module);

  if (auto err = initialize_shader(shader.get(), parsed->info)) {
    errorOut << llvm::toString(std::move(err));
    SM50_RETURN_ERROR(errorObj, ppError, 1);
  }

  if (pRefl) {
    *pRefl = {};
    pRefl->ConstanttBufferTableBindIndex = ~0u;
    pRefl->ArgumentBufferBindIndex = ~0u;
    pRefl->NumOutputElement = shader->max_output_register;
    switch (shader->shader_type) {
    case microsoft::D3D11_SB_COMPUTE_SHADER:
      pRefl->ThreadgroupSize[0] = shader->threadgroup_size[0];
      pRefl->ThreadgroupSize[1] = shader->threadgroup_size[1];
      pRefl->ThreadgroupSize[2] = shader->threadgroup_size[2];
      break;
    case microsoft::D3D10_SB_PIXEL_SHADER:
      pRefl->PixelShader.ValidRenderTargets = shader->pso_valid_output_reg_mask;
      pRefl->PixelShader.HasCoverageOutput = shader->ps_has_coverage_output;
      break;
    default:
      break;
    }
  }

  *ppShader = (sm50_shader_t)shader.release();
  return 0;
}

llvm::Expected<llvm::BasicBlock *>
convert_basicblocks(const EntryPoint &entry, dxmt::dxbc::context &ctx, llvm::BasicBlock *return_bb) {
  auto saved_block = ctx.builder.GetInsertBlock();
  Converter converter(entry, ctx);
  auto result = converter.Run(return_bb);
  ctx.builder.SetInsertPoint(saved_block);
  return result;
}

} // namespace dxmt::dxil

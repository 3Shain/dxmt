#include "dxil_converter.hpp"
#include "dxbc_converter.hpp"
#include "dxil_metadata.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include "airconv_error.hpp"
#include "nt/dxil_converter_base.hpp"

#include <memory>
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

// constants for resolving resource handles
constexpr uint32_t kOpCreateHandle = 57;
constexpr uint32_t kOpTextureLoad = 66;
constexpr uint32_t kOpTextureStore = 67;
constexpr uint32_t kOpBufferLoad = 68;
constexpr uint32_t kOpBufferStore = 69;
constexpr uint32_t kOpAtomicBinOp = 78;
constexpr uint32_t kOpAtomicCmpXchg = 79;
constexpr uint32_t kOpSample = 60;
constexpr uint32_t kOpSampleBias = 61;
constexpr uint32_t kOpSampleLevel = 62;
constexpr uint32_t kOpSampleGrad = 63;
constexpr uint32_t kOpSampleCmp = 64;
constexpr uint32_t kOpSampleCmpLevelZero = 65;

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
  } else {
    return llvm::make_error<UnsupportedFeature>("dxil: unsupported shader stage " + std::string(stage));
  }

  auto &func_signature = shader->func_signature;
  if (shader->shader_type == microsoft::D3D11_SB_COMPUTE_SHADER) {
    for (auto &property : entry.properties) {
      if (property.tag != EntryPropertyTag::NumThreads)
        continue;
      auto n = property.num_threads;
      shader->threadgroup_size[0] = n.x;
      shader->threadgroup_size[1] = n.y;
      shader->threadgroup_size[2] = n.z;
      func_signature.UseMaxWorkgroupSize(n.x * n.y * n.z);
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
        if (auto err = record_usage(false, false, true, false))
          return err;
        break;
      case kOpSampleCmp:
      case kOpSampleCmpLevelZero:
        if (auto err = record_usage(false, false, true, true))
          return err;
        break;
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
    default:
      break;
    }
  }

  *ppShader = (sm50_shader_t)shader.release();
  return 0;
}

llvm::Expected<llvm::BasicBlock *>
convert_basicblocks(const EntryPoint & entry, dxmt::dxbc::context &ctx, llvm::BasicBlock *return_bb) {
  auto saved_block = ctx.builder.GetInsertBlock();
  Converter converter(entry, ctx);
  auto result = converter.Run(return_bb);
  ctx.builder.SetInsertPoint(saved_block);
  return result;
}

} // namespace dxmt::dxil

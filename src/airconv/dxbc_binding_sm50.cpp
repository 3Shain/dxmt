
#include "dxbc_converter.hpp"
#include "nt/dxbc_binding_map.hpp"
#include "shader_common.hpp"
#include "llvm/IR/Function.h"
#include <memory>

namespace dxmt::dxbc {

using namespace llvm::air;

class SM50BindingMap : public BindingMap {
public:
  llvm::Value *
  GetArgument(llvm::air::AIRBuilder &AIR, uint32_t TableIndex, uint32_t Index) {
    auto Fn = AIR.builder.GetInsertBlock()->getParent();
    auto Arg = Fn->getArg(TableIndex);
    auto TyArg = llvm::cast<llvm::PointerType>(Arg->getType())->getNonOpaquePointerElementType();
    auto TyStruct = llvm::cast<llvm::StructType>(TyArg);
    return AIR.builder.CreateLoad(TyStruct->getElementType(Index), AIR.builder.CreateStructGEP(TyStruct, Arg, Index));
  };

  virtual llvm::Optional<ConstantBufferDescriptor>
  GetConstantBuffer(llvm::air::AIRBuilder &Builder, RangeId Range, llvm::Value *Index) {
    if (~ConstantBufferTableIndex == 0)
      return {};
    auto Iter = ConstantBuffers.find(Range);
    if (Iter == ConstantBuffers.end())
      return {};
    auto Pointer = GetArgument(Builder, ConstantBufferTableIndex, Iter->second.arg_index);
    return ConstantBufferDescriptor{Pointer, nullptr};
  }

  virtual llvm::Optional<SamplerDescriptor>
  GetSampler(llvm::air::AIRBuilder &Builder, RangeId Range, llvm::Value *Index) {
    if (~BindingTableIndex == 0)
      return {};
    auto Iter = Samplers.find(Range);
    if (Iter == Samplers.end())
      return {};
    auto Sampler = GetArgument(Builder, BindingTableIndex, Iter->second.arg_index);
    auto CubeSampler = GetArgument(Builder, BindingTableIndex, Iter->second.arg_cube_index);
    auto Metadata = GetArgument(Builder, BindingTableIndex, Iter->second.arg_metadata_index);
    return SamplerDescriptor{Sampler, CubeSampler, Metadata};
  }

  virtual llvm::Optional<TextureDescirptor>
  GetSRVTexture(llvm::air::AIRBuilder &Builder, RangeId Range, llvm::Value *Index) {
    if (~BindingTableIndex == 0)
      return {};
    auto Iter = SRVs.find(Range);
    if (Iter == SRVs.end())
      return {};
    if (Iter->second.resource_type == shader::common::ResourceType::NonApplicable)
      return {};
    auto Handle = GetArgument(Builder, BindingTableIndex, Iter->second.arg_index);
    auto Metadata = GetArgument(Builder, BindingTableIndex, Iter->second.arg_metadata_index);

    // assert(Iter->second.resource_type != shader::common::ResourceType::NonApplicable);
    auto ResourceKindLogical = air::to_air_resource_type(Iter->second.resource_type, Iter->second.compared);
    auto ResourceKind = air::lowering_texture_1d_to_2d(ResourceKindLogical);
    auto SampleType = air::to_air_sample_type(Iter->second.scaler_type, Iter->second.compared);
    auto MemoryAccess =
        Iter->second.sampled ? Texture::MemoryAccess::access_sample : Texture::MemoryAccess::access_read;

    return TextureDescirptor{Handle, Metadata, false, ResourceKind, ResourceKindLogical, MemoryAccess, SampleType};
  }

  virtual llvm::Optional<TextureDescirptor>
  GetUAVTexture(llvm::air::AIRBuilder &Builder, RangeId Range, llvm::Value *Index) {
    if (~BindingTableIndex == 0)
      return {};
    auto Iter = UAVs.find(Range);
    if (Iter == UAVs.end())
      return {};
    if (Iter->second.resource_type == shader::common::ResourceType::NonApplicable)
      return {};
    auto Handle = GetArgument(Builder, BindingTableIndex, Iter->second.arg_index);
    auto Metadata = GetArgument(Builder, BindingTableIndex, Iter->second.arg_metadata_index);

    // assert(Iter->second.resource_type != shader::common::ResourceType::NonApplicable);
    auto ResourceKindLogical = air::to_air_resource_type(Iter->second.resource_type);
    auto ResourceKind = air::lowering_texture_1d_to_2d(ResourceKindLogical);
    auto SampleType = air::to_air_sample_type(Iter->second.scaler_type);
    auto MemoryAccess = Iter->second.written ? (Iter->second.read ? Texture::MemoryAccess::acesss_readwrite
                                                                  : Texture::MemoryAccess::access_write)
                                             : Texture::MemoryAccess::access_read;

    return TextureDescirptor{Handle,       Metadata,  Iter->second.global_coherent, ResourceKind, ResourceKindLogical,
                             MemoryAccess, SampleType};
  }

  virtual llvm::Optional<BufferDescriptor>
  GetSRVBuffer(llvm::air::AIRBuilder &Builder, RangeId Range, llvm::Value *Index) {
    if (~BindingTableIndex == 0)
      return {};
    auto Iter = SRVs.find(Range);
    if (Iter == SRVs.end())
      return {};
    if (Iter->second.resource_type != shader::common::ResourceType::NonApplicable)
      return {};
    auto Handle = GetArgument(Builder, BindingTableIndex, Iter->second.arg_index);
    auto Metadata = GetArgument(Builder, BindingTableIndex, Iter->second.arg_metadata_index);
    return BufferDescriptor{Handle, Metadata, Iter->second.structure_stride, false};
  }

  virtual llvm::Optional<BufferDescriptor>
  GetUAVBuffer(llvm::air::AIRBuilder &Builder, RangeId Range, llvm::Value *Index) {
    if (~BindingTableIndex == 0)
      return {};
    auto Iter = UAVs.find(Range);
    if (Iter == UAVs.end())
      return {};
    if (Iter->second.resource_type != shader::common::ResourceType::NonApplicable)
      return {};
    auto Handle = GetArgument(Builder, BindingTableIndex, Iter->second.arg_index);
    auto Metadata = GetArgument(Builder, BindingTableIndex, Iter->second.arg_metadata_index);
    return BufferDescriptor{Handle, Metadata, Iter->second.structure_stride, Iter->second.global_coherent};
  }
  virtual llvm::Optional<CounterDescriptor>
  GetUAVCounter(llvm::air::AIRBuilder &Builder, RangeId Range, llvm::Value *Index) {
    if (~BindingTableIndex == 0)
      return {};
    auto Iter = UAVs.find(Range);
    if (Iter == UAVs.end())
      return {};
    if (Iter->second.resource_type != shader::common::ResourceType::NonApplicable)
      return {};
    auto Handle = GetArgument(Builder, BindingTableIndex, Iter->second.arg_counter_index);
    return CounterDescriptor{Handle};
  }

  uint32_t ConstantBufferTableIndex = ~0u;
  uint32_t BindingTableIndex = ~0u;

  std::map<RangeId, ConstantBufferInfo> ConstantBuffers;
  std::map<RangeId, SamplerInfo> Samplers;
  std::map<RangeId, ShaderResourceViewInfo> SRVs;
  std::map<RangeId, UnorderedAccessViewInfo> UAVs;
};

std::unique_ptr<BindingMap>
setup_binding_table2(
    const ShaderInfo *shader_info, air::FunctionSignatureBuilder &func_signature, llvm::Module &module,
    uint32_t argbuffer_constant_slot, uint32_t argbuffer_slot
) {
  auto binding_map = std::make_unique<SM50BindingMap>();

  if (!shader_info->binding_table.Empty()) {
    auto [type, metadata] = shader_info->binding_table.Build(module.getContext(), module.getDataLayout());
    std::string arg_name = "binding_table";
    if (argbuffer_slot != SM50_BINDING_INDEX_ARGUMENT_TABLE)
      arg_name += std::to_string(argbuffer_slot);
    binding_map->BindingTableIndex = func_signature.DefineInput(air::ArgumentBindingIndirectBuffer{
        .location_index = argbuffer_slot,
        .array_size = 1,
        .memory_access = air::MemoryAccess::read,
        .address_space = air::AddressSpace::constant,
        .struct_type = type,
        .struct_type_info = metadata,
        .arg_name = arg_name,
    });
  }
  if (!shader_info->binding_table_cbuffer.Empty()) {
    auto [type, metadata] = shader_info->binding_table_cbuffer.Build(module.getContext(), module.getDataLayout());
    std::string arg_name = "cbuffer_table";
    if (argbuffer_constant_slot != SM50_BINDING_INDEX_CONSTANT_BUFFER)
      arg_name += std::to_string(argbuffer_constant_slot);
    binding_map->ConstantBufferTableIndex = func_signature.DefineInput(air::ArgumentBindingIndirectBuffer{
        .location_index = argbuffer_constant_slot,
        .array_size = 1,
        .memory_access = air::MemoryAccess::read,
        .address_space = air::AddressSpace::constant,
        .struct_type = type,
        .struct_type_info = metadata,
        .arg_name = arg_name,
    });
  }

  binding_map->ConstantBuffers = shader_info->cbufferMap;
  binding_map->Samplers = shader_info->samplerMap;
  binding_map->SRVs = shader_info->srvMap;
  binding_map->UAVs = shader_info->uavMap;

  return binding_map;
};

} // namespace dxmt::dxbc
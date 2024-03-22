#pragma once

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include "adt.hpp"
#include <variant>

namespace dxmt::air {

enum class MemoryAccess { read, write, read_write, sample };

enum class AddressSpace : uint32_t { unknown, device, constant, threadgroup };

// I use kind to not confuse with type which often refers to component type
enum class TextureKind {
  texture_1d,
  texture_1d_array,
  texture_2d,
  texture_2d_array,
  texture_2d_ms,
  texture_2d_ms_array,
  texture_3d,
  texture_cube,
  texture_cube_array,
  texture_buffer, // 10
  depth_2d,
  depth_2d_array,
  depth_2d_ms,
  depth_2d_ms_array,
  depth_cube,
  depth_cube_array // 16
};

enum class Interpolation {
  center_perspective,
  center_no_perspective,
  centroid_perspective,
  centroid_no_perspective,
  sample_perspective,
  sample_no_perspective,
  flat
};

struct MSLFloat {
  std::string get_name() { return "float"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    return llvm::Type::getFloatTy(context);
  };
};

struct MSLInt {
  std::string get_name() { return "int"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    return llvm::Type::getInt32Ty(context);
  };
};

struct MSLUint {
  std::string get_name() { return "uint"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    return llvm::Type::getInt32Ty(context);
  };
};

struct MSLBool {
  std::string get_name() { return "bool"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    return llvm::Type::getInt1Ty(context);
  };
};

struct MSLSampler {
  std::string get_name() { return "sampler"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    return llvm::Type::getInt32Ty(context);
  };
};

constexpr auto msl_bool = MSLBool{};
constexpr auto msl_int = MSLInt{};
constexpr auto msl_uint = MSLUint{};
constexpr auto msl_float = MSLFloat{};

using MSLScalerType =
    std::variant<MSLFloat, MSLInt, MSLUint, MSLBool>; // incomplete list

constexpr auto msl_sampler = MSLSampler{};

struct MSLVector {
  uint32_t dimension;
  MSLScalerType scaler;
  std::string get_name() {
    auto scaler_name =
        std::visit([](auto scaler) { return scaler.get_name(); }, scaler);
    if (dimension == 1)
      return scaler_name;
    return scaler_name + std::to_string(dimension);
  };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    auto scaler_type =
        std::visit([&](auto x) { return x.get_llvm_type(context); }, scaler);
    if (dimension == 1) {
      return scaler_type;
    }
    return llvm::FixedVectorType::get(scaler_type, dimension);
  };
};

constexpr auto msl_int2 = MSLVector{2, msl_int};
constexpr auto msl_uint2 = MSLVector{2, msl_uint};
constexpr auto msl_float2 = MSLVector{2, msl_float};
constexpr auto msl_int3 = MSLVector{3, msl_int};
constexpr auto msl_uint3 = MSLVector{3, msl_uint};
constexpr auto msl_float3 = MSLVector{3, msl_float};
constexpr auto msl_int4 = MSLVector{4, msl_int};
constexpr auto msl_uint4 = MSLVector{4, msl_uint};
constexpr auto msl_float4 = MSLVector{4, msl_float};

struct MSLTexture {
  MSLScalerType component_type;
  MemoryAccess memory_access;
  TextureKind resource_kind;

  std::string get_name() {
    auto component = std::visit([](auto scaler) { return scaler.get_name(); },
                                component_type);
    auto access = [](auto access) -> std::string {
      switch (access) {
      case MemoryAccess::read:
        return "read";
      case MemoryAccess::read_write:
        return "read_write";
      case MemoryAccess::write:
        return "write";
      case MemoryAccess::sample:
        return "sample";
      }
      assert(0 && "unhandled memory access");
    }(memory_access);

    switch (resource_kind) {
    case TextureKind::texture_1d:
      return "texture1d<" + component + "," + access + ">";
    case TextureKind::texture_1d_array:
      return "texture1d_array<" + component + "," + access + ">";
    case TextureKind::texture_2d:
      return "texture2d<" + component + "," + access + ">";
    case TextureKind::texture_2d_array:
      return "texture2d_array<" + component + "," + access + ">";
    case TextureKind::texture_2d_ms:
      return "texture2d_ms<" + component + "," + access + ">";
    case TextureKind::texture_2d_ms_array:
      return "texture2d_ms_array<" + component + "," + access + ">";
    case TextureKind::texture_3d:
      return "texture3d<" + component + "," + access + ">";
    case TextureKind::texture_buffer:
      return "texture_buffer<" + component + "," + access + ">";
    case TextureKind::texture_cube:
      return "texturecube<" + component + "," + access + ">";
    case TextureKind::texture_cube_array:
      return "texturecube_array<" + component + "," + access + ">";
    case TextureKind::depth_2d:
      return "depth2d<" + component + "," + access + ">";
    case TextureKind::depth_2d_array:
      return "depth2d_array<" + component + "," + access + ">";
    case TextureKind::depth_2d_ms:
      return "depth2d_ms<" + component + "," + access + ">";
    case TextureKind::depth_2d_ms_array:
      return "depth2d_ms_array<" + component + "," + access + ">";
    case TextureKind::depth_cube:
      return "depthcube<" + component + "," + access + ">";
    case TextureKind::depth_cube_array:
      return "depthcube_array<" + component + "," + access + ">";
    default:
      assert(0 && "unhandled resource kind");
    }
  };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    return llvm::Type::getInt32Ty(context);
  };
};

// I mean does metal really care about it?
struct MSLWhateverStruct {
  std::string name;
  llvm::Type *type;
  std::string get_name() { return name; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) { return type; };
};

using MSLScalerOrVectorType =
    template_concat_t<MSLScalerType, std::variant<MSLVector>>;

using MSLRepresentableType =
    template_concat_t<MSLScalerOrVectorType,
                      std::variant<MSLSampler, MSLTexture, MSLWhateverStruct>>;

struct MSLPointer {
  MSLRepresentableType pointee;
  AddressSpace address_space;
  std::string get_name() {
    // fking weird
    return std::visit([](auto x) { return x.get_name(); }, pointee);
  };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    auto pointee_type =
        std::visit([&](auto x) { return x.get_llvm_type(context); }, pointee);
    return pointee_type->getPointerTo((uint32_t)address_space);
  };
};

struct MSLStaticArray {
  uint32_t array_size;
  MSLRepresentableType element_type;
  std::string get_name() {
    return "array<" +
           std::visit([](auto scaler) { return scaler.get_name(); },
                      element_type) +
           "," + std::to_string(array_size) + ">";
  };
};

using MSLRepresentableTypeWithArray = template_concat_t<
    std::variant<MSLStaticArray, MSLPointer /* why are you here? */>>;

constexpr auto get_name = [](auto msl_type) {
  return std::visit([](auto msl_type) { return msl_type.get_name(); },
                    msl_type);
};

constexpr auto get_llvm_type = [](auto msl_type, llvm::LLVMContext &context) {
  return std::visit([&](auto x) { return x.get_llvm_type(context); }, msl_type);
};

struct ArgumentBindingBuffer {
  std::optional<uint32_t> buffer_size; // unbounded if not represented
  uint32_t location_index;
  uint32_t array_size;
  MemoryAccess memory_access;
  AddressSpace address_space;
  // std::string arg_type_name;
  MSLRepresentableType type;
  std::string arg_name;
};

struct ArgumentBindingSampler {
  uint32_t location_index;
  uint32_t array_size;
  std::string arg_name;
};

struct ArgumentBindingTexture {
  uint32_t location_index;
  uint32_t array_size;
  MemoryAccess memory_access;
  MSLTexture type;
  std::string arg_name;
};

/* is this in fact argument buffer? */
struct ArgumentBindingIndirectBuffer {
  uint32_t location_index;
  uint32_t array_size;
  MemoryAccess memory_access;
  AddressSpace address_space;
  llvm::StructType *struct_type;
  llvm::Metadata *struct_type_info;
  std::string arg_name;
};

struct ArgumentBindingIndirectConstant {
  uint32_t location_index;
  uint32_t array_size;
  MSLRepresentableTypeWithArray type;
  std::string arg_name;
};

using ArgumentBufferArguments =
    std::variant<ArgumentBindingBuffer, ArgumentBindingIndirectBuffer,
                 ArgumentBindingTexture, ArgumentBindingIndirectConstant,
                 ArgumentBindingSampler>;

using FunctionArguments =
    std::variant<ArgumentBindingBuffer, ArgumentBindingIndirectBuffer,
                 ArgumentBindingTexture, ArgumentBindingSampler>;

class ArgumentBufferBuilder {
  /* the return value is the element index of structure (as well as in argument
   * buffer) */
public:
  uint32_t DefineBuffer(std::string name, AddressSpace addressp_space,
                        MemoryAccess access, MSLRepresentableType type);
  uint32_t DefineTexture(std::string name, TextureKind kind,
                         MemoryAccess access, MSLScalerType scaler_type);
  uint32_t DefineSampler(std::string name);
  uint32_t DefineInteger32(std::string name);
  uint32_t DefineFloat32(std::string name);

  auto Build(llvm::LLVMContext &context, llvm::Module &module)
      -> std::tuple<llvm::StructType *, llvm::MDNode>;

private:
  std::unordered_map<std::string, ArgumentBufferArguments> fieldsType;
};

template <typename I, typename O> class FunctionSignatureBuilder {
public:
  uint32_t DefineInput(const I &input);
  uint32_t DefineOutput(const O &output);

  auto CreateFunction(llvm::LLVMContext &context, llvm::Module &module)
      -> llvm::Function *;

private:
  std::vector<I> inputs;
  std::vector<O> outputs;
};

struct InputVertexStageIn {
  uint32_t attribute;
  MSLScalerOrVectorType type;
};

struct OutputVertex {
  std::string user;
  MSLScalerOrVectorType type;
};

struct InputFragmentStageIn {
  std::string user;
  MSLScalerOrVectorType type;
  Interpolation interpolation;
};

struct OutputRenderTarget {
  uint32_t index;
  MSLScalerOrVectorType type;
};

struct InputVertexID {};

using VertexInput =
    template_concat_t<FunctionArguments,
                      std::variant<InputVertexID, InputVertexStageIn>>;
using VertexOutput = std::variant<OutputVertex>;

struct InputPrimitiveID {};
struct InputViewportArrayIndex {};
struct InputRenderTargetArrayIndex {};
struct InputFrontFacing {};

struct InputPosition {
  Interpolation interpolation;
};

using FragmentInput = template_concat_t<
    FunctionArguments,
    std::variant<InputPrimitiveID, InputViewportArrayIndex,
                 InputRenderTargetArrayIndex, InputFrontFacing, InputPosition,
                 InputFragmentStageIn>>;
using FragmentOutput = std::variant<OutputRenderTarget>;

using KernelInput = template_concat_t<FunctionArguments>;

class VertexFunctionSignatureBuilder
    // am I using inheritance? No!
    : public FunctionSignatureBuilder<VertexInput, VertexOutput> {
  uint32_t DefineVertexStageIn(uint32_t attribute, MSLScalerOrVectorType type);
  uint32_t DefineVertexOutput(std::string user, MSLScalerOrVectorType type);
};

class FragmentFunctionSignatureBuilder
    : public FunctionSignatureBuilder<FragmentInput, FragmentOutput> {
  uint32_t DefineFragmentStageIn(std::string user, MSLScalerOrVectorType type);
  uint32_t DefineRenderTarget(uint32_t index, MSLScalerOrVectorType type);
};

class KernelFunctionSignatureBuilder
    : public FunctionSignatureBuilder<KernelInput, std::variant<>> {
  uint32_t DefineKernelStageIn(std::string user, MSLScalerOrVectorType type);
};

} // namespace dxmt::air
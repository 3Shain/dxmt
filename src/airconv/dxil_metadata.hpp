#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Error.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dxmt::dxil {

enum class ComponentType : uint8_t {
  Invalid = 0,
  I1,
  I16,
  U16,
  I32,
  U32,
  I64,
  U64,
  F16,
  F32,
  F64,
  SNormF16,
  UNormF16,
  SNormF32,
  UNormF32,
  SNormF64,
  UNormF64,
  PackedS8x32,
  PackedU8x32,
  LastEntry,
};

enum class ResourceKind : uint32_t {
  Invalid = 0,
  Texture1D,
  Texture2D,
  Texture2DMS,
  Texture3D,
  TextureCube,
  Texture1DArray,
  Texture2DArray,
  Texture2DMSArray,
  TextureCubeArray,
  TypedBuffer,
  RawBuffer,
  StructuredBuffer,
  CBuffer,
  Sampler,
  TBuffer,
  RTAccelerationStructure,
  FeedbackTexture2D,
  FeedbackTexture2DArray,
  NumEntries,
};

enum class ResourceClass : uint8_t {
  SRV = 0,
  UAV = 1,
  CBV = 2,
  Sampler = 3,
};

enum class ResourceTag : uint32_t {
  TypedBufferElementType = 0,
  StructuredBufferElementStride = 1,
  SamplerFeedbackKind = 2,
};

enum class SignatureElementTag : uint32_t {
  OutputStream = 0,
  GlobalSymbol = 1,
  DynIdxCompMask = 2,
  UsageCompMask = 3,
};

enum class SemanticKind : uint32_t {
  Arbitrary = 0,
  VertexID,
  InstanceID,
  Position,
  RenderTargetArrayIndex,
  ViewPortArrayIndex,
  ClipDistance,
  CullDistance,
  OutputControlPointID,
  DomainLocation,
  PrimitiveID,
  GSInstanceID,
  SampleIndex,
  IsFrontFace,
  Coverage,
  InnerCoverage,
  Target,
  Depth,
  DepthLessEqual,
  DepthGreaterEqual,
  StencilRef,
  DispatchThreadID,
  GroupID,
  GroupIndex,
  GroupThreadID,
  TessFactor,
  InsideTessFactor,
  ViewID,
  Barycentrics,
  ShadingRate,
  CullPrimitive,
  Invalid,
};

enum class InterpolationMode : uint8_t {
  Undefined = 0,
  Constant,
  Linear,
  LinearCentroid,
  LinearNoperspective,
  LinearNoperspectiveCentroid,
  LinearSample,
  LinearNoperspectiveSample,
  Invalid,
};

struct SignatureElement {
  uint32_t id = 0;
  std::string name;
  ComponentType component_type = ComponentType::Invalid;
  SemanticKind semantic_kind = SemanticKind::Arbitrary;
  InterpolationMode interpolation = InterpolationMode::Undefined;
  std::vector<uint32_t> semantic_indices;
  uint32_t rows = 0;
  uint32_t cols = 0;
  int32_t start_row = 0;
  int32_t start_col = 0;
  uint32_t usage_mask = 0;
};

enum class EntryPropertyTag : uint32_t {
  ShaderFlags = 0,
  GSState = 1,
  DSState = 2,
  HSState = 3,
  NumThreads = 4,
  AutoBindingSpace = 5,
  RayPayloadSize = 6,
  RayAttribSize = 7,
  ShaderKind = 8,
  MSState = 9,
  ASState = 10,
  WaveSize = 11,
  EntryRootSig = 12,
};

constexpr uint64_t kShaderFlagDisableShaderOptimization = 1 << 0;
constexpr uint64_t kShaderFlagDisableMathRefactoring = 1 << 1;
constexpr uint64_t kShaderFlagForceEarlyDepthStencilTest = 1 << 3;

struct NumThreads {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
};

struct Property {
  EntryPropertyTag tag = EntryPropertyTag::NumThreads;
  union {
    uint64_t shader_flags;
    NumThreads num_threads;
  };
};

struct Resource {
  uint32_t id = 0;
  ResourceClass resource_class = ResourceClass::SRV;
  uint32_t space = 0;
  uint32_t lower_bound = 0;
  uint32_t range_size = 0;
  ResourceKind kind = ResourceKind::Invalid;
  bool globally_coherent = false;
  bool has_counter = false;
  bool rasterizer_ordered = false;
  uint32_t sample_count = 0;
  uint32_t cbuffer_size = 0;
  uint32_t element_type = 0;
  uint32_t element_stride = 0;
};

struct EntryPoint {
  const llvm::Function *function = nullptr;
  std::vector<SignatureElement> inputs;
  std::vector<SignatureElement> outputs;
  std::vector<Resource> resources;
  std::vector<Property> properties;
};

llvm::Expected<EntryPoint> parse_entry_point_for(const llvm::Function &function);

struct ShaderModel {
  std::string stage;
  uint32_t major = 0;
  uint32_t minor = 0;
};

struct Info {
  ShaderModel shader_model;
  std::vector<EntryPoint> entry_points;
};

struct ParsedModule {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
  Info info;
};

llvm::Expected<ParsedModule> parse_dxil_blob(llvm::StringRef blob);

} // namespace dxmt::dxil

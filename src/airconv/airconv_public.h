#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#ifndef __AIRCONV_H
#define __AIRCONV_H

#ifdef __cplusplus
enum class ShaderType {
  Vertex,
  /* Metal: fragment function */
  Pixel,
  /* Metal: kernel function */
  Compute,
  /* Not present in Metal */
  Hull,
  /* Metal: post-vertex function */
  Domain,
  /* Not present in Metal */
  Geometry,
  Mesh,
  /* Metal: object function */
  Amplification,
};

enum class SM50BindingType : uint32_t {
  ConstantBuffer,
  Sampler,
  SRV,
  UAV,
};
#else
typedef uint32_t ShaderType;
typedef uint32_t SM50BindingType;
#endif

enum MTL_SM50_SHADER_ARGUMENT_FLAG : uint32_t {
  MTL_SM50_SHADER_ARGUMENT_BUFFER = 1 << 0,
  MTL_SM50_SHADER_ARGUMENT_TEXTURE = 1 << 1,
  MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH = 1 << 2,
  MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER = 1 << 3,
  MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP = 1 << 4,
  MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET = 1 << 5,
  MTL_SM50_SHADER_ARGUMENT_TEXTURE_ARRAY = 1 << 6,
  MTL_SM50_SHADER_ARGUMENT_READ_ACCESS = 1 << 10,
  MTL_SM50_SHADER_ARGUMENT_WRITE_ACCESS = 1 << 11,
};

struct MTL_SM50_SHADER_ARGUMENT {
  SM50BindingType Type;
  /**
  bind point of it's corresponding resource space
  constant buffer:    cb1 -> 1
  srv:                t10 -> 10
  uav:                u0  -> 0
  sampler:            s2  -> 2
  */
  uint32_t SM50BindingSlot;
  enum MTL_SM50_SHADER_ARGUMENT_FLAG Flags;
  uint32_t StructurePtrOffset;
};

enum MTL_TESSELLATOR_OUTPUT_PRIMITIVE {
  MTL_TESSELLATOR_OUTPUT_POINT = 1,
  MTL_TESSELLATOR_OUTPUT_LINE = 2,
  MTL_TESSELLATOR_OUTPUT_TRIANGLE_CW = 3,
  MTL_TESSELLATOR_TRIANGLE_CCW = 4
};

struct MTL_TESSELLATOR_REFLECTION {
  uint32_t Partition;
  float MaxFactor;
  enum MTL_TESSELLATOR_OUTPUT_PRIMITIVE OutputPrimitive;
};

struct MTL_GEOMETRY_SHADER_PASS_THROUGH {
  uint8_t RenderTargetArrayIndexReg;
  uint8_t RenderTargetArrayIndexComponent;
  uint8_t ViewportArrayIndexReg;
  uint8_t ViewportArrayIndexComponent;
};

struct MTL_GEOMETRY_SHADER_REFLECTION {
  union {
    struct MTL_GEOMETRY_SHADER_PASS_THROUGH Data;
    uint32_t GSPassThrough;
  };
  uint32_t Primitive;
};

struct MTL_SHADER_REFLECTION {
  uint32_t ConstanttBufferTableBindIndex;
  uint32_t ArgumentBufferBindIndex;
  uint32_t NumConstantBuffers;
  uint32_t NumArguments;
  struct MTL_SM50_SHADER_ARGUMENT *ConstantBuffers;
  struct MTL_SM50_SHADER_ARGUMENT *Arguments;
  union {
    uint32_t ThreadgroupSize[3];
    struct MTL_TESSELLATOR_REFLECTION Tessellator;
    struct MTL_GEOMETRY_SHADER_REFLECTION GeometryShader;
  };
  uint16_t ConstantBufferSlotMask;
  uint16_t SamplerSlotMask;
  uint64_t UAVSlotMask;
  uint64_t SRVSlotMaskHi;
  uint64_t SRVSlotMaskLo;
  uint32_t NumOutputElement;
  uint32_t NumPatchConstantOutputScalar;
  uint32_t ThreadsPerPatch;
  uint32_t ArgumentTableQwords;
};

struct MTL_SHADER_BITCODE {
  char *Data;
  size_t Size;
  // add additional metadata here
};

typedef struct __SM50Shader SM50Shader;
typedef struct __SM50CompiledBitcode SM50CompiledBitcode;
typedef struct __SM50Error SM50Error;

#ifdef _WIN32
#ifdef WIN_EXPORT
#define AIRCONV_API __attribute__((sysv_abi)) __declspec(dllexport)
#else
#define AIRCONV_API __attribute__((sysv_abi)) __declspec(dllimport)
#endif
#else
#define AIRCONV_API __attribute__((sysv_abi))
#endif

#if __METALCPP__
#include "Metal/MTLArgumentEncoder.hpp"
typedef ::MTL::ArgumentEncoder *ArgumentEncoder_t;
typedef ::MTL::Device *Device_t;
#else
typedef struct __MTLArgumentEncoder *ArgumentEncoder_t;
typedef struct __MTLDevice *Device_t;
#endif

#ifdef __cplusplus

inline uint32_t
GetArgumentIndex(SM50BindingType Type, uint32_t SM50BindingSlot) {
  switch (Type) {
  case SM50BindingType::ConstantBuffer:
    return SM50BindingSlot;
  case SM50BindingType::Sampler:
    return SM50BindingSlot + 32;
  case SM50BindingType::SRV:
    return SM50BindingSlot * 3 + 128;
  case SM50BindingType::UAV:
    return SM50BindingSlot * 3 + 512;
  }
};

inline uint32_t GetArgumentIndex(struct MTL_SM50_SHADER_ARGUMENT &Argument) {
  return GetArgumentIndex(Argument.Type, Argument.SM50BindingSlot);
};

extern "C" {
#endif

enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE {
  SM50_SHADER_COMPILATION_INPUT_SIGN_MASK = 0,
  SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT = 1,
  SM50_SHADER_DEBUG_IDENTITY = 2,
  SM50_SHADER_PSO_PIXEL_SHADER = 3,
  SM50_SHADER_IA_INPUT_LAYOUT = 4,
  SM50_SHADER_GS_PASS_THROUGH = 5,
};

struct SM50_SHADER_COMPILATION_ARGUMENT_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
};

struct SM50_SHADER_COMPILATION_INPUT_SIGN_MASK_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint64_t sign_mask;
};

struct SM50_STREAM_OUTPUT_ELEMENT {
  uint32_t reg_id;
  uint32_t component;
  uint32_t output_slot;
  uint32_t offset;
};

struct SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint32_t num_output_slots;
  uint32_t num_elements;
  uint32_t strides[4];
  struct SM50_STREAM_OUTPUT_ELEMENT *elements;
};

struct SM50_SHADER_DEBUG_IDENTITY_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint64_t id;
};

struct SM50_SHADER_PSO_PIXEL_SHADER_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint32_t sample_mask;
  bool dual_source_blending;
  bool disable_depth_output;
  uint32_t unorm_output_reg_mask;
};

struct SM50_IA_INPUT_ELEMENT {
  uint32_t reg;
  uint32_t slot;
  uint32_t aligned_byte_offset;
  /** MTLAttributeFormat */
  uint32_t format;
  uint32_t step_function: 1;
  uint32_t step_rate: 31;
};

enum SM50_INDEX_BUFFER_FORAMT {
  SM50_INDEX_BUFFER_FORMAT_NONE = 0,
  SM50_INDEX_BUFFER_FORMAT_UINT16 = 1,
  SM50_INDEX_BUFFER_FORMAT_UINT32 = 2,
};

struct SM50_SHADER_IA_INPUT_LAYOUT_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  enum SM50_INDEX_BUFFER_FORAMT index_buffer_format;
  uint32_t slot_mask;
  uint32_t num_elements;
  struct SM50_IA_INPUT_ELEMENT *elements;
};

struct SM50_SHADER_GS_PASS_THROUGH_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  union {
    struct MTL_GEOMETRY_SHADER_PASS_THROUGH Data;
    uint32_t DataEncoded;
  };
  bool RasterizationDisabled;
};

AIRCONV_API int SM50Initialize(
  const void *pBytecode, size_t BytecodeSize, SM50Shader **ppShader,
  struct MTL_SHADER_REFLECTION *pRefl, SM50Error **ppError
);
AIRCONV_API void SM50Destroy(SM50Shader *pShader);
AIRCONV_API int SM50Compile(
  SM50Shader *pShader, struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
);
AIRCONV_API void SM50GetCompiledBitcode(
  SM50CompiledBitcode *pBitcode, struct MTL_SHADER_BITCODE *pData
);
AIRCONV_API void SM50DestroyBitcode(SM50CompiledBitcode *pBitcode);
AIRCONV_API const char *SM50GetErrorMesssage(SM50Error *pError);
AIRCONV_API void SM50FreeError(SM50Error *pError);

AIRCONV_API int SM50CompileTessellationPipelineVertex(
  SM50Shader *pVertexShader, SM50Shader *pHullShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pVertexShaderArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
);
AIRCONV_API int SM50CompileTessellationPipelineHull(
  SM50Shader *pVertexShader, SM50Shader *pHullShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pHullShaderArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
);
AIRCONV_API int SM50CompileTessellationPipelineDomain(
  SM50Shader *pHullShader, SM50Shader *pDomainShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pDomainShaderArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
);

AIRCONV_API int SM50CompileGeometryPipelineVertex(
  SM50Shader *pVertexShader, SM50Shader *pGeometryShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pVertexShaderArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
);
AIRCONV_API int SM50CompileGeometryPipelineGeometry(
  SM50Shader *pVertexShader, SM50Shader *pGeometryShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pGeometryShaderArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
);

#ifdef __cplusplus
};
#endif

#endif

#pragma once

#include "stddef.h"
#include "stdint.h"

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

struct MTL_SHADER_REFLECTION {
  uint32_t ConstanttBufferTableBindIndex;
  uint32_t ArgumentBufferBindIndex;
  uint32_t NumConstantBuffers;
  uint32_t NumArguments;
  struct MTL_SM50_SHADER_ARGUMENT *ConstantBuffers;
  struct MTL_SM50_SHADER_ARGUMENT *Arguments;
  union {
    uint32_t ThreadgroupSize[3];
  };
  uint16_t ConstantBufferSlotMask;
  uint16_t SamplerSlotMask;
  uint64_t UAVSlotMask;
  uint64_t SRVSlotMaskHi;
  uint64_t SRVSlotMaskLo;
};

struct MTL_SHADER_BITCODE {
  char *Data;
  size_t Size;
  // add additional metadata here
};

typedef struct __SM50Shader SM50Shader;
typedef struct __SM50CompiledBitcode SM50CompiledBitcode;
typedef struct __SM50Error SM50Error;

#define AIRCONV_EXPORT __attribute__((sysv_abi))

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
  SM50_SHADER_PSO_SAMPLE_MASK = 3,
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

struct SM50_SHADER_PSO_SAMPLE_MASK_DATA {
  void *next;
  enum SM50_SHADER_COMPILATION_ARGUMENT_TYPE type;
  uint32_t sample_mask;
};

AIRCONV_EXPORT int SM50Initialize(
  const void *pBytecode, size_t BytecodeSize, SM50Shader **ppShader,
  struct MTL_SHADER_REFLECTION *pRefl, SM50Error **ppError
);
AIRCONV_EXPORT void SM50Destroy(SM50Shader *pShader);
AIRCONV_EXPORT int SM50Compile(
  SM50Shader *pShader, struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
);
AIRCONV_EXPORT void SM50GetCompiledBitcode(
  SM50CompiledBitcode *pBitcode, struct MTL_SHADER_BITCODE *pData
);
AIRCONV_EXPORT void SM50DestroyBitcode(SM50CompiledBitcode *pBitcode);
AIRCONV_EXPORT const char *SM50GetErrorMesssage(SM50Error *pError);
AIRCONV_EXPORT void SM50FreeError(SM50Error *pError);
#ifdef __cplusplus
};
#endif
#pragma once

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
  SRV_BufferSize, // argument constant
  SRV_MinLOD,     // argument constant
  UAV,
  UAV_BufferSize, // argument constant
  UAVCounter,
};
#else
typedef uint32_t ShaderType;
typedef uint32_t SM50BindingType;
#endif

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
};

struct MTL_SHADER_REFLECTION {
  uint32_t ArgumentBufferBindIndex;
  uint32_t NumArguments;
  struct MTL_SM50_SHADER_ARGUMENT *Arguments;
  union {
    uint32_t ThreadgroupSize[3];
  };
};

struct MTL_SHADER_BITCODE {
  char *Data;
  size_t Size;
  // add additional metadata here
};

typedef struct __SM50Shader SM50Shader;
typedef struct __SM50CompiledBitcode SM50CompiledBitcode;

#define AIRCONV_EXPORT __attribute__((sysv_abi))

#ifdef __cplusplus

inline uint32_t GetArgumentIndex(struct MTL_SM50_SHADER_ARGUMENT Argument) {
  switch (Argument.Type) {
  case SM50BindingType::ConstantBuffer:
    return Argument.SM50BindingSlot;
  case SM50BindingType::Sampler:
    return Argument.SM50BindingSlot + 32;
  case SM50BindingType::SRV:
    return Argument.SM50BindingSlot + 128;
  case SM50BindingType::SRV_BufferSize:
    return Argument.SM50BindingSlot; // TODO
  case SM50BindingType::SRV_MinLOD:
    return Argument.SM50BindingSlot; // TODO
  case SM50BindingType::UAV:
    return Argument.SM50BindingSlot + 256;
  case SM50BindingType::UAV_BufferSize:
    return Argument.SM50BindingSlot; // TODO
  case SM50BindingType::UAVCounter:
    return Argument.SM50BindingSlot; // TODO
    break;
  }
};

extern "C" {
#endif
AIRCONV_EXPORT SM50Shader *SM50Initialize(
  const void *pBytecode, size_t BytecodeSize,
  struct MTL_SHADER_REFLECTION *pRefl
);
AIRCONV_EXPORT void SM50Destroy(SM50Shader *pShader);
AIRCONV_EXPORT SM50CompiledBitcode *
SM50Compile(SM50Shader *pShader, void *pArgs);
AIRCONV_EXPORT void SM50GetCompiledBitcode(
  SM50CompiledBitcode *pBitcode, struct MTL_SHADER_BITCODE *pData
);
AIRCONV_EXPORT void SM50DestroyBitcode(SM50CompiledBitcode *pBitcode);
#ifdef __cplusplus
}
#endif
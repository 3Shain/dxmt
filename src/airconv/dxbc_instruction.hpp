/*
This file wraps "DXBCParser/ShaderBinary.h"
to make DX improvements.
*/
#pragma once
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "enum.h"
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <cstddef>

#include "air_constants.hpp"
#include "DXBCParser/ShaderBinary.h"

namespace dxmt::dxbc {

#define _INLINE inline __attribute__((always_inline))
#define _UNREACHABLE assert(0 && "unreachable");

enum class Op : uint32_t {
  ADD,
  AND,
  BREAK,
  BREAKC,
  CALL,
  CALLC,
  CASE,
  CONTINUE,
  CONTINUEC,
  CUT,
  DEFAULT,
  DERIV_RTX,
  DERIV_RTY,
  DISCARD,
  DIV,
  DP2,
  DP3,
  DP4,
  ELSE,
  EMIT,
  EMITTHENCUT,
  ENDIF,
  ENDLOOP,
  ENDSWITCH,
  eq,
  EXP,
  FRC,
  FTOI,
  FTOU,
  ge,
  IADD,
  IF,
  ieq,
  ige,
  ilt,
  IMAD,
  IMAX,
  IMIN,
  IMUL,
  ine,
  INEG,
  ISHL,
  ISHR,
  ITOF,
  LABEL,
  LD,
  LD_MS,
  LOG,
  LOOP,
  lt,
  MAD,
  MIN,
  MAX,
  CUSTOMDATA,
  mov,
  movc,
  MUL,
  ne,
  NOP,
  NOT,
  OR,
  RESINFO,
  RET,
  RETC,
  ROUND_NE,
  ROUND_NI,
  ROUND_PI,
  ROUND_Z,
  RSQ,
  SAMPLE,
  SAMPLE_C,
  SAMPLE_C_LZ,
  SAMPLE_L,
  SAMPLE_D,
  SAMPLE_B,
  SQRT,
  SWITCH,
  SINCOS,
  UDIV,
  ult,
  uge,
  UMUL,
  UMAD,
  UMAX,
  UMIN,
  USHR,
  UTOF,
  XOR,
  DCL_RESOURCE,        // DCL* opcodes have
  DCL_CONSTANT_BUFFER, // custom operand formats.
  DCL_SAMPLER,
  DCL_INDEX_RANGE,
  DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY,
  DCL_GS_INPUT_PRIMITIVE,
  DCL_MAX_OUTPUT_VERTEX_COUNT,
  DCL_INPUT,
  DCL_INPUT_SGV,
  DCL_INPUT_SIV,
  DCL_INPUT_PS,
  DCL_INPUT_PS_SGV,
  DCL_INPUT_PS_SIV,
  DCL_OUTPUT,
  DCL_OUTPUT_SGV,
  DCL_OUTPUT_SIV,
  DCL_TEMPS,
  DCL_INDEXABLE_TEMP,
  DCL_GLOBAL_FLAGS,

  // -----------------------------------------------

  // This marks the end of D3D10.0 opcodes
  RESERVED0,

  // ---------- DX 10.1 op codes---------------------

  LOD,
  GATHER4,
  SAMPLE_POS,
  SAMPLE_INFO,

  // -----------------------------------------------

  // This marks the end of D3D10.1 opcodes
  RESERVED1,

  // ---------- DX 11 op codes---------------------
  HS_DECLS,               // token marks beginning of HS sub-shader
  HS_CONTROL_POINT_PHASE, // token marks beginning of HS
                          // sub-shader
  HS_FORK_PHASE,          // token marks beginning of HS sub-shader
  HS_JOIN_PHASE,          // token marks beginning of HS sub-shader

  EMIT_STREAM,
  CUT_STREAM,
  EMITTHENCUT_STREAM,
  INTERFACE_CALL,

  BUFINFO,
  DERIV_RTX_COARSE,
  DERIV_RTX_FINE,
  DERIV_RTY_COARSE,
  DERIV_RTY_FINE,
  GATHER4_C,
  GATHER4_PO,
  GATHER4_PO_C,
  RCP,
  F32TOF16,
  F16TOF32,
  UADDC,
  USUBB,
  COUNTBITS,
  FIRSTBIT_HI,
  FIRSTBIT_LO,
  FIRSTBIT_SHI,
  UBFE,
  IBFE,
  BFI,
  BFREV,
  swapc,

  DCL_STREAM,
  DCL_FUNCTION_BODY,
  DCL_FUNCTION_TABLE,
  DCL_INTERFACE,

  DCL_INPUT_CONTROL_POINT_COUNT,
  DCL_OUTPUT_CONTROL_POINT_COUNT,
  DCL_TESS_DOMAIN,
  DCL_TESS_PARTITIONING,
  DCL_TESS_OUTPUT_PRIMITIVE,
  DCL_HS_MAX_TESSFACTOR,
  DCL_HS_FORK_PHASE_INSTANCE_COUNT,
  DCL_HS_JOIN_PHASE_INSTANCE_COUNT,

  DCL_THREAD_GROUP,
  DCL_UNORDERED_ACCESS_VIEW_TYPED,
  DCL_UNORDERED_ACCESS_VIEW_RAW,
  DCL_UNORDERED_ACCESS_VIEW_STRUCTURED,
  DCL_THREAD_GROUP_SHARED_MEMORY_RAW,
  DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED,
  DCL_RESOURCE_RAW,
  DCL_RESOURCE_STRUCTURED,
  LD_UAV_TYPED,
  STORE_UAV_TYPED,
  LD_RAW,
  STORE_RAW,
  LD_STRUCTURED,
  STORE_STRUCTURED,
  ATOMIC_AND,
  ATOMIC_OR,
  ATOMIC_XOR,
  ATOMIC_CMP_STORE,
  ATOMIC_IADD,
  ATOMIC_IMAX,
  ATOMIC_IMIN,
  ATOMIC_UMAX,
  ATOMIC_UMIN,
  IMM_ATOMIC_ALLOC,
  IMM_ATOMIC_CONSUME,
  IMM_ATOMIC_IADD,
  IMM_ATOMIC_AND,
  IMM_ATOMIC_OR,
  IMM_ATOMIC_XOR,
  IMM_ATOMIC_EXCH,
  IMM_ATOMIC_CMP_EXCH,
  IMM_ATOMIC_IMAX,
  IMM_ATOMIC_IMIN,
  IMM_ATOMIC_UMAX,
  IMM_ATOMIC_UMIN,
  SYNC,

  DADD,
  DMAX,
  DMIN,
  DMUL,
  DEQ,
  DGE,
  DLT,
  DNE,
  DMOV,
  DMOVC,
  DTOF,
  FTOD,

  EVAL_SNAPPED,
  EVAL_SAMPLE_INDEX,
  EVAL_CENTROID,

  DCL_GS_INSTANCE_COUNT,

  ABORT,
  DEBUG_BREAK,

  // -----------------------------------------------

  // This marks the end of D3D11.0 opcodes
  RESERVED0_11,

  DDIV,
  DFMA,
  DRCP,

  MSAD,

  DTOI,
  DTOU,
  ITOD,
  UTOD,

  // -----------------------------------------------

  // This marks the end of D3D11.1 opcodes
  RESERVED0__11_1,

  GATHER4_FEEDBACK,
  GATHER4_C_FEEDBACK,
  GATHER4_PO_FEEDBACK,
  GATHER4_PO_C_FEEDBACK,
  LD_FEEDBACK,
  LD_MS_FEEDBACK,
  LD_UAV_TYPED_FEEDBACK,
  LD_RAW_FEEDBACK,
  LD_STRUCTURED_FEEDBACK,
  SAMPLE_L_FEEDBACK,
  SAMPLE_C_LZ_FEEDBACK,

  SAMPLE_CLAMP_FEEDBACK,
  SAMPLE_B_CLAMP_FEEDBACK,
  SAMPLE_D_CLAMP_FEEDBACK,
  SAMPLE_C_CLAMP_FEEDBACK,

  CHECK_ACCESS_FULLY_MAPPED,

  // -----------------------------------------------

  // This marks the end of WDDM 1.3 opcodes
  RESERVED0_WDDM_1_3
};

inline static air::ESampleInterpolation
ToAirInterpolation(D3D10_SB_INTERPOLATION_MODE mode) {
  switch (mode) {
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:
    return air::ESampleInterpolation::sample_no_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_SAMPLE:
    return air::ESampleInterpolation::sample_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:
    return air::ESampleInterpolation::centroid_no_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_CENTROID:
    return air::ESampleInterpolation::centroid_perspective;
  case D3D10_SB_INTERPOLATION_CONSTANT:
    return air::ESampleInterpolation::flat;
  case D3D10_SB_INTERPOLATION_LINEAR:
    return air::ESampleInterpolation::center_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE:
    return air::ESampleInterpolation::center_no_perspective;
  default:
    assert(0 && "Unexpected D3D10_SB_INTERPOLATION_MODE");
  }
}

union Immediate32 {
  uint32_t i32;
  float fp;
};

class OperandIndex {
public:
  OperandIndex(const D3D10ShaderBinary::COperandIndex &index,
               D3D10_SB_OPERAND_INDEX_REPRESENTATION indexType)
      : index(index), indexType(indexType) {}

  template <typename T>
  _INLINE T match(
      const std::function<T(uint32_t)> &ifImmediate32,
      const std::function<T(uint32_t reg, uint8_t component)> &ifRelativeToTemp,
      const std::function<T(uint32_t regFile, uint32_t reg, uint8_t component)>
          &ifRelativeToIndexableTemp,
      const std::function<T(T, uint32_t offset)> addOffset) const {
    switch (indexType) {
    case D3D10_SB_OPERAND_INDEX_IMMEDIATE32:
      return ifImmediate32(index.m_RegIndex);
      break;
    case D3D10_SB_OPERAND_INDEX_RELATIVE: {
      switch (index.m_RelRegType) {
      case D3D10_SB_OPERAND_TYPE_TEMP: {
        return ifRelativeToTemp(index.m_RelIndex, index.m_ComponentName);
      }
      case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
        return ifRelativeToIndexableTemp(index.m_RelIndex, index.m_RelIndex1,
                                         index.m_ComponentName);
      }
      default:
        break;
      }
      break;
    }
    case D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE: {
      switch (index.m_RelRegType) {
      case D3D10_SB_OPERAND_TYPE_TEMP: {
        return addOffset(
            ifRelativeToTemp(index.m_RelIndex, index.m_ComponentName),
            index.m_RegIndex);
      }
      case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
        return addOffset(
            ifRelativeToTemp(index.m_RelIndex, index.m_ComponentName),
            index.m_RegIndex);
      }
      default:
        break;
      }
      break;
    }
    default:
      break;
    }
    _UNREACHABLE;
  };

private:
  const D3D10ShaderBinary::COperandIndex &index;
  D3D10_SB_OPERAND_INDEX_REPRESENTATION indexType;
};

class Operand {
public:
  Operand(const D3D10ShaderBinary::COperandBase &oprd) { operand = &oprd; }

  _INLINE uint32_t reg() const {
    switch (operand->m_IndexDimension) {
    case D3D10_SB_OPERAND_INDEX_1D:
      return operand->m_Index[0].m_RegIndex; // ? are you sure?
      break;
    default:
      break;
    }
    _UNREACHABLE;
  };

  OperandIndex index(uint32_t i) const {
    assert(i < operand->m_IndexDimension && "index out of range");
    return OperandIndex(operand->m_Index[i], operand->m_IndexType[i]);
  }

protected:
  const D3D10ShaderBinary::COperandBase *operand;
};

class SrcOperand : public Operand {
public:
  _INLINE SrcOperand(const D3D10ShaderBinary::COperandBase &oprd)
      : Operand(oprd){};

  _INLINE std::array<uint8_t, 4> swizzle() const {
    if (operand->m_ComponentSelection ==
        D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) {

    } else if (operand->m_ComponentSelection ==
               D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) {

    } else if (operand->m_ComponentSelection ==
               D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) {
    }
    _UNREACHABLE
  };

  bool negative() const {
    return (operand->m_Modifier & D3D10_SB_OPERAND_MODIFIER_NEG) != 0;
  }

  bool absolute() const {
    return (operand->m_Modifier & D3D10_SB_OPERAND_MODIFIER_ABS) != 0;
  }

  bool constant() const {
    return operand->m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32;
  }

  bool genericInput() const {
    return operand->m_Type == D3D10_SB_OPERAND_TYPE_INPUT;
  }

  bool temp() const { return operand->m_Type == D3D10_SB_OPERAND_TYPE_TEMP; }

  bool indexableTemp() const {
    return operand->m_Type == D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP;
  }
};

class DstOperand : public Operand {
public:
  _INLINE DstOperand(const D3D10ShaderBinary::COperandBase &oprd)
      : Operand(oprd){};

  _INLINE uint32_t mask() {
    auto mask = operand->m_WriteMask >> 4;
    assert((mask & 0xf) == mask);
    return mask;
  };

  bool null() const { return operand->m_Type == D3D10_SB_OPERAND_TYPE_NULL; }

  bool genericOutput() const {
    return operand->m_Type == D3D10_SB_OPERAND_TYPE_OUTPUT;
  }

  bool temp() const { return operand->m_Type == D3D10_SB_OPERAND_TYPE_TEMP; }

  bool indexableTemp() const {
    return operand->m_Type == D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP;
  }

  // bool null() const { return operand->m_Type ==
  // D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH; }
};

class Instruciton {
public:
  Instruciton(const D3D10ShaderBinary::CInstruction &inst) {
    instruction = &inst;
  }

  Op opCode() const { return (Op)(instruction->OpCode()); }

  air::ESampleInterpolation interpolation() const {
    if (opCode() == Op::DCL_INPUT_PS) {
      return ToAirInterpolation(instruction->m_InputPSDecl.InterpolationMode);
    }
    if (opCode() == Op::DCL_INPUT_PS) {
      return ToAirInterpolation(
          instruction->m_InputPSDeclSGV.InterpolationMode);
    }
    if (opCode() == Op::DCL_INPUT_PS) {
      return ToAirInterpolation(
          instruction->m_InputPSDeclSIV.InterpolationMode);
    }
    _UNREACHABLE
  }

  DstOperand dst(uint32_t index) const {
    assert(index < instruction->m_NumOperands && "dst index out of range");
    return DstOperand(instruction->m_Operands[index]);
  }

  SrcOperand src(uint32_t index) const {
    assert(index < instruction->m_NumOperands && "src index out of range");
    return SrcOperand(instruction->m_Operands[index]);
  }

  /*
  applies to:
  - constantBuffer
  - sampler
  - resource
  - uav
  */
  uint32_t dclBindingRegister() const {
    if (opCode() == Op::DCL_INDEXABLE_TEMP) {
      return instruction->m_IndexableTempDecl.IndexableTempNumber;
    }
    switch (opCode()) {
    case Op::DCL_CONSTANT_BUFFER:
    case Op::DCL_SAMPLER:
    case Op::DCL_RESOURCE:
    case Op::DCL_RESOURCE_RAW:
    case Op::DCL_RESOURCE_STRUCTURED:
    case Op::DCL_UNORDERED_ACCESS_VIEW_TYPED:
    case Op::DCL_UNORDERED_ACCESS_VIEW_RAW:
    case Op::DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
      switch (instruction->m_Operands[0].m_IndexDimension) {
      case D3D10_SB_OPERAND_INDEX_1D: // SM 5.0-
        return instruction->m_Operands[0].m_Index[0].m_RegIndex;
      case D3D10_SB_OPERAND_INDEX_3D: // SM 5.1
      default:
        break;
      }
      break;
    }
    default:
      break;
    }
    _UNREACHABLE
  }

  /*
  applies to:
  - constantBuffer
  - sampler
  - resource
  - uav
  */
  _INLINE std::tuple<uint32_t, uint32_t> dclBindingRange() {
    switch (instruction->m_Operands[0].m_IndexDimension) {
    case D3D10_SB_OPERAND_INDEX_1D: // SM 5.0-
      return std::make_pair(instruction->m_Operands[0].m_Index[0].m_RegIndex,
                            1);
    case D3D10_SB_OPERAND_INDEX_3D: // SM 5.1
      return std::make_pair(
          instruction->m_Operands[0].m_Index[1].m_RegIndex,
          instruction->m_Operands[0].m_Index[2].m_RegIndex != UINT_MAX
              ? instruction->m_Operands[0].m_Index[2].m_RegIndex -
                    instruction->m_Operands[0].m_Index[1].m_RegIndex + 1
              : UINT_MAX);
    default:
      _UNREACHABLE
    }
  }

  _INLINE uint32_t dclBindingMask() const {
    if (opCode() == Op::DCL_INDEXABLE_TEMP) {
      assert((instruction->m_IndexableTempDecl.Mask & 0xf) ==
             instruction->m_IndexableTempDecl.Mask);
      return instruction->m_IndexableTempDecl.Mask;
    }
    return dst(0).mask();
  }

  _INLINE uint32_t dclFlags() const {
    switch (opCode()) {
    case Op::DCL_UNORDERED_ACCESS_VIEW_RAW:
      return instruction->m_RawUAVDecl.Flags;
    case Op::DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
      return instruction->m_StructuredUAVDecl.Flags;
    case Op::DCL_UNORDERED_ACCESS_VIEW_TYPED:
      return instruction->m_TypedUAVDecl.Flags;
    default:
      _UNREACHABLE
    }
  }

  _INLINE bool dclWithCounter() const {
    return (dclFlags() & D3D11_SB_UAV_HAS_ORDER_PRESERVING_COUNTER) != 0;
  }

  //   _INLINE uint32_t dclType() {
  //     switch (opCode()) {
  //     case Op::DCL_RESOURCE:
  //       // TODO: return type with dimension
  //       return instruction->m_ResourceDecl.ReturnType;
  //     case Op::DCL_UNORDERED_ACCESS_VIEW_TYPED:
  //       return instruction->m_TypedUAVDecl.ReturnType;
  //     default:
  //       _UNREACHABLE
  //     }
  //   }

  _INLINE uint32_t dclConstSize() const {
    switch (opCode()) {
    case Op::DCL_CONSTANT_BUFFER:
      // TODO: return type with dimension
      return instruction->m_ConstantBufferDecl.Size;
    case Op::DCL_TEMPS:
      return instruction->m_TempsDecl.NumTemps;
    case Op::DCL_INDEXABLE_TEMP:
      return instruction->m_IndexableTempDecl.NumRegisters;
    default:
      _UNREACHABLE
    }
  }

  _INLINE uint32_t dclConstStride() const {
    switch (opCode()) {
    case Op::DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
      // TODO: return type with dimension
      return instruction->m_StructuredUAVDecl.ByteStride;
    case Op::DCL_UNORDERED_ACCESS_VIEW_TYPED:
      return 1;
    default:
      _UNREACHABLE
    }
  }

  //   _INLINE uint32_t dclBindingSpace() {}
  air::ETextureType dclResourceType() {
    assert((opCode() == Op::DCL_RESOURCE) ||
           (opCode() == Op::DCL_UNORDERED_ACCESS_VIEW_TYPED));
    static_assert(
        offsetof(D3D10ShaderBinary::CResourceDecl, Dimension) ==
            offsetof(D3D10ShaderBinary::CTypedUAVDeclaration, Dimension),
        "otherwise assuption is false");
    switch (instruction->m_ResourceDecl.Dimension) {
    case D3D10_SB_RESOURCE_DIMENSION_BUFFER:
      return air::ETextureType::texture_buffer;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURE1D:
      return air::ETextureType::texture_1d;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D:
      return air::ETextureType::texture_2d;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMS:
      return air::ETextureType::texture_2d_ms;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURE3D:
      return air::ETextureType::texture_3d;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBE:
      return air::ETextureType::texture_cube;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURE1DARRAY:
      return air::ETextureType::texture_1d_array;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DARRAY:
      return air::ETextureType::texture_2d_array;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
      return air::ETextureType::texture_2d_ms_array;
    case D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBEARRAY:
      return air::ETextureType::texture_cube_array;
    default:
      assert(0 && "Unexpected resource dimension");
    }
  }

  air::ETextureAccessType dclResourceAccessType() {
    assert((opCode() == Op::DCL_RESOURCE) ||
           (opCode() == Op::DCL_UNORDERED_ACCESS_VIEW_TYPED));
    static_assert(
        offsetof(D3D10ShaderBinary::CResourceDecl, ReturnType) ==
            offsetof(D3D10ShaderBinary::CTypedUAVDeclaration, ReturnType),
        "otherwise assuption is false");
    switch (instruction->m_ResourceDecl.ReturnType[0]) {
    case D3D10_SB_RETURN_TYPE_UNORM:
      return air::ETextureAccessType::v4f32;
    case D3D10_SB_RETURN_TYPE_SNORM:
      return air::ETextureAccessType::v4f32;
    case D3D10_SB_RETURN_TYPE_SINT:
      return air::ETextureAccessType::s_v4i32;
    case D3D10_SB_RETURN_TYPE_UINT:
      return air::ETextureAccessType::u_v4i32;
    case D3D10_SB_RETURN_TYPE_FLOAT:
      return air::ETextureAccessType::v4f32;
    default:
      assert(0 && "Unexpected resource return type");
    }
  }

  bool saturate() const { return instruction->m_bSaturate; }

private:
  const D3D10ShaderBinary::CInstruction *instruction;
};

static_assert(sizeof(SrcOperand) == 8, "do you forget to add _INLINE?");
static_assert(sizeof(DstOperand) == 8, "do you forget to add _INLINE?");
static_assert(sizeof(Instruciton) == 8, "do you forget to add _INLINE?");
} // namespace dxmt::dxbc
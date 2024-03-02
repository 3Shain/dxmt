/*
This file wraps "DXBCParser/ShaderBinary.h"
to make DX improvements.
*/
#pragma once
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "enum.h"
#include "DXBCParser/ShaderBinary.h"
#include <array>
#include <cstdint>
#include <utility>

#include "air_constants.hpp"

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
  EQ,
  EXP,
  FRC,
  FTOI,
  FTOU,
  GE,
  IADD,
  IF,
  IEQ,
  IGE,
  ILT,
  IMAD,
  IMAX,
  IMIN,
  IMUL,
  INE,
  INEG,
  ISHL,
  ISHR,
  ITOF,
  LABEL,
  LD,
  LD_MS,
  LOG,
  LOOP,
  LT,
  MAD,
  MIN,
  MAX,
  CUSTOMDATA,
  MOV,
  MOVC,
  MUL,
  NE,
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
  ULT,
  UGE,
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
  SWAPC,

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

class SrcOperand {
public:
  _INLINE SrcOperand(const D3D10ShaderBinary::COperandBase &oprd) {
    operand = &oprd;
  }

  _INLINE std::array<uint8_t, 4> swizzle() {
    if (operand->m_ComponentSelection ==
        D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) {

    } else if (operand->m_ComponentSelection ==
               D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) {

    } else if (operand->m_ComponentSelection ==
               D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) {
    }
    _UNREACHABLE
  };

  _INLINE uint32_t reg(){
      // switch (operand->m_Type) {
      //     case
      // }
  };

private:
  const D3D10ShaderBinary::COperandBase *operand;
};

class DstOperand {
public:
  _INLINE DstOperand(const D3D10ShaderBinary::COperandBase &oprd) {
    operand = &oprd;
  }

  _INLINE uint32_t mask() {
    auto mask = operand->m_WriteMask >> 4;
    assert((mask & 0xf) == mask);
    return mask;
  };

  //   _INLINE register

private:
  const D3D10ShaderBinary::COperandBase *operand;
};

class Instruciton {
public:
  _INLINE Instruciton(const D3D10ShaderBinary::CInstruction &inst) {
    instruction = &inst;
  }

  _INLINE Op opCode() { return (Op)(instruction->OpCode()); }

  _INLINE air::ESampleInterpolation interpolation() {
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

  _INLINE DstOperand dest(uint32_t index) {
    assert(index < instruction->m_NumOperands && "operand oob");
    return DstOperand(instruction->m_Operands[index]);
  }

  _INLINE SrcOperand src(uint32_t index) {
    assert(index < instruction->m_NumOperands && "operand oob");
    return SrcOperand(instruction->m_Operands[index]);
  }

  /*
  applies to:
  - constantBuffer
  - sampler
  - resource
  - uav
  */
  _INLINE uint32_t dclBindingSlot() {
    if (opCode() == Op::DCL_INDEXABLE_TEMP) {
      return instruction->m_IndexableTempDecl.IndexableTempNumber;
    }
    if (opCode() == Op::DCL_CONSTANT_BUFFER) {
      return instruction->m_Operands[0].m_Index[0].m_RegIndex;
    }
    switch (instruction->m_Operands[0].m_IndexDimension) {
    case D3D10_SB_OPERAND_INDEX_1D: // SM 5.0-
      return instruction->m_Operands[0].m_Index[0].m_RegIndex;
    case D3D10_SB_OPERAND_INDEX_3D: // SM 5.1
    default:
      _UNREACHABLE
    }
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

  _INLINE uint32_t dclBindingMask() {
    if (opCode() == Op::DCL_INDEXABLE_TEMP) {
      assert((instruction->m_IndexableTempDecl.Mask & 0xf) ==
             instruction->m_IndexableTempDecl.Mask);
      return instruction->m_IndexableTempDecl.Mask;
    }
    return dest(0).mask();
  }

  _INLINE uint32_t dclFlags() {
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

  _INLINE bool dclWithCounter() {
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

  _INLINE uint32_t dclConstSize() {
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

  _INLINE uint32_t dclConstStride() {
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

private:
  const D3D10ShaderBinary::CInstruction *instruction;
};

static_assert(sizeof(SrcOperand) == 8, "do you forget to add _INLINE?");
static_assert(sizeof(DstOperand) == 8, "do you forget to add _INLINE?");
static_assert(sizeof(Instruciton) == 8, "do you forget to add _INLINE?");
} // namespace dxmt::dxbc
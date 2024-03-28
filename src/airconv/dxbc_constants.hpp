#pragma once

#include <cstdint>

namespace dxmt::dxbc {

enum class ShaderType {
  Pixel = 0,
  Vertex = 1,
  D3D10_SB_GEOMETRY_SHADER = 2,

  // D3D11 Shaders
  D3D11_SB_HULL_SHADER = 3,
  D3D11_SB_DOMAIN_SHADER = 4,
  Compute = 5,

  // Subset of D3D12 Shaders where this field is referenced by runtime
  // Entries from 6-12 are unique to state objects
  // (e.g. library, callable and raytracing shaders)
  D3D12_SB_MESH_SHADER = 13,
  D3D12_SB_AMPLIFICATION_SHADER = 14,
};

enum class RegisterComponentType : uint32_t {
  Unknown,
  Uint = 1,
  Int = 2,
  Float = 3
};

enum class InstructionReturnType { Float = 0, Uint = 1 };

#define _UNREACHABLE assert(0 && "unreachable");

/* anything in CAPITAL is not handled properly yet (rename in progress) */

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

enum class OpClass : uint32_t {
  FLOAT_OP,
  INT_OP,
  UINT_OP,
  bit,
  flow,
  texture,
  declaration,
  ATOMIC_OP,
  MEM_OP,
  DOUBLE_OP,
  FLOAT_TO_DOUBLE_OP,
  DOUBLE_TO_FLOAT_OP,
  DEBUG_OP,
};

enum class Name : uint32_t {
  unknown = 0,
  position = 1,
  clipDistance = 2,
  CULL_DISTANCE = 3,
  renderTargetArrayIndex = 4,
  viewportArrayIndex = 5,
  vertexId = 6,
  primitiveId = 7,
  instanceId = 8,
  isFrontFace = 9,
  sampleIndex = 10,
  // The following are added for D3D11
  FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR = 11,
  FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR = 12,
  FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR = 13,
  FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR = 14,
  FINAL_QUAD_U_INSIDE_TESSFACTOR = 15,
  FINAL_QUAD_V_INSIDE_TESSFACTOR = 16,
  FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR = 17,
  FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR = 18,
  FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR = 19,
  FINAL_TRI_INSIDE_TESSFACTOR = 20,
  FINAL_LINE_DETAIL_TESSFACTOR = 21,
  FINAL_LINE_DENSITY_TESSFACTOR = 22,
  // The following are added for D3D12
  BARYCENTRICS = 23,
  SHADINGRATE = 24,
  CULLPRIMITIVE = 25,
};

enum class OperandType : uint32_t {
  temporary = 0,               // Temporary Register File
  input = 1,                   // General Input Register File
  output = 2,                  // General Output Register File
  indexableTemporary = 3,      // Temporary Register File (indexable)
  immediate32 = 4,             // 32bit/component immediate value(s)
                               // If for example, operand token bits
                               // [01:00]==OPERAND_4_COMPONENT,
                               // this means that the operand type:
                               // IMMEDIATE32
                               // results in 4 additional 32bit
                               // DWORDS present for the operand.
  immediate64 = 5,             // 64bit/comp.imm.val(s)HI:LO
  sampler = 6,                 // Reference to sampler state
  resource = 7,                // Reference to memory resource (e.g. texture)
  constantBuffer = 8,          // Reference to constant buffer
  immediateConstantBuffer = 9, // Reference to immediate constant buffer
  label = 10,                  // Label
  iPrimitiveId = 11,           // Input primitive ID
  oDepth = 12,                 // Output Depth
  _NULL = 13,           // Null register, used to discard results of operations
                        // Below Are operands new in DX 10.1
  RASTERIZER = 14,      // DX10.1 Rasterizer register, used to denote the
                        // depth/stencil and render target resources
  oCoverageMask = 15,   // DX10.1 PS output MSAA coverage mask (scalar)
                        // Below Are operands new in DX 11
  STREAM = 16,          // Reference to GS stream output resource
  FUNCTION_BODY = 17,   // Reference to a function definition
  FUNCTION_TABLE = 18,  // Reference to a set of functions used by a class
  INTERFACE = 19,       // Reference to an interface
  FUNCTION_INPUT = 20,  // Reference to an input parameter to a function
  FUNCTION_OUTPUT = 21, // Reference to an output parameter to a function
  OUTPUT_CONTROL_POINT_ID = 22, // HS Control Point phase input saying which
                                // output control point ID this is
  INPUT_FORK_INSTANCE_ID = 23,  // HS Fork Phase input instance ID
  INPUT_JOIN_INSTANCE_ID = 24,  // HS Join Phase input instance ID
  INPUT_CONTROL_POINT =
      25, // HS Fork+Join, DS phase input control points (array of them)
  OUTPUT_CONTROL_POINT =
      26, // HS Fork+Join phase output control points (array of them)
  INPUT_PATCH_CONSTANT = 27, // DS+HSJoin Input Patch Constants (array of them)
  INPUT_DOMAIN_POINT = 28,   // DS Input Domain point
  THIS_POINTER = 29,         // Reference to an interface this pointer
  unorderedAccessView = 30,  // Reference to UAV u#
  THREAD_GROUP_SHARED_MEMORY = 31, // Reference to Thread Group Shared Memory g#
  iThreadId = 32,                  // Compute Shader Thread ID
  iThreadGroupId = 33,             // Compute Shader Thread Group ID
  iThreadIdInGroup = 34,           // Compute Shader Thread ID In Thread Group
  iCoverageMask = 35,              // Pixel shader coverage mask input
  iThreadIdInGroupFlatten =
      36, // Compute Shader Thread ID In Group Flattened to a 1D value.
  INPUT_GS_INSTANCE_ID = 37, // Input GS instance ID
  oDepthGE =
      38, // Output Depth, forced to be greater than or equal than current depth
  oDepthLE =
      39, // Output Depth, forced to be less than or equal to current depth
  CYCLE_COUNTER = 40, // Cycle counter
  oStencilRef = 41,   // DX11 PS output stencil reference (scalar)
  iCoverage = 42,     // DX11 PS input inner coverage (scalar)
};

} // namespace dxmt::dxbc

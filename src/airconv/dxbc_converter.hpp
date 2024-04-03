#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

#include "shader_common.hpp"
#include "llvm/IR/LLVMContext.h"

namespace dxmt::dxbc {

struct Swizzle {
  union {
    uint8_t x;
    uint8_t r;
  };
  union {
    uint8_t y;
    uint8_t g;
  };
  union {
    uint8_t z;
    uint8_t b;
  };
  union {
    uint8_t w;
    uint8_t a;
  };
  bool operator==(Swizzle const &o) const {
    return r == o.r && g == o.g && b == o.b && a == o.a;
  };
};

constexpr Swizzle swizzle_identity = {0, 1, 2, 3};

#pragma region operand index
struct IndexByTempComponent {
  uint32_t regid;
  uint8_t component;
  uint32_t offset;
};

struct IndexByIndexableTempComponent {
  uint32_t regfile;
  uint32_t regid;
  uint8_t component;
  uint32_t offset;
};

using OperandIndex =
  std::variant<uint32_t, IndexByIndexableTempComponent, IndexByTempComponent>;
#pragma endregion

#pragma region source operand

struct SrcOperandCommon {
  Swizzle swizzle;
  bool abs;
  bool neg;
};

struct SrcOperandImmediate32 {
  SrcOperandCommon _;
  union {
    int32_t ivalue[4];
    uint32_t uvalue[4];
    float fvalue[4];
  };
};

struct SrcOperandTemp {
  SrcOperandCommon _;
  uint32_t regid;
};

struct SrcOperandIndexableTemp {
  SrcOperandCommon _;
  uint32_t regfile;
  OperandIndex regindex;
};

struct SrcOperandInput {
  SrcOperandCommon _;
  uint32_t regid;
};

struct SrcOperandConstantBuffer {
  SrcOperandCommon _;
  uint32_t rangeid;
  OperandIndex rangeindex;
  OperandIndex regindex;
};

struct SrcOperandImmediateConstantBuffer {
  SrcOperandCommon _;
  OperandIndex regindex;
};

#pragma endregion

#pragma region dest operand

struct DstOperandCommon {
  uint32_t mask;
};

struct DstOperandNull {
  DstOperandCommon _;
};

struct DstOperandTemp {
  DstOperandCommon _;
  uint32_t regid;
};

struct DstOperandIndexableTemp {
  DstOperandCommon _;
  uint32_t regfile;
  OperandIndex regindex;
};

struct DstOperandOutput {
  DstOperandCommon _;
  uint32_t regid;
};

struct DstOperandOutputDepth {
  DstOperandCommon _;
};

#pragma endregion

#pragma region instructions
struct InstructionCommon {
  bool saturate;
};

struct DclConstantBuffer {};

struct DclResource {};

struct DclRawResource {};

struct DclStructuredResource {};

struct DclInput {};

struct DclOutput {};

#pragma endregion

using SrcOperand = std::variant<
  SrcOperandImmediate32, SrcOperandTemp, SrcOperandIndexableTemp,
  SrcOperandInput, SrcOperandConstantBuffer, SrcOperandImmediateConstantBuffer>;

struct SrcOperandResource {
  uint32_t range_id;
  OperandIndex index;
  Swizzle read_swizzle; // if appliable?
};

struct SrcOperandSampler {
  uint32_t range_id;
  OperandIndex index;
  uint8_t gather_channel; // if appliable
};

struct SrcOperandUAV {
  uint32_t range_id;
  OperandIndex index;
};

struct SrcOperandTGSM {
  uint32_t id;
  OperandIndex index;
};

using AtomicDstOperandUAV = SrcOperandUAV;

struct AtomicDstOperandTGSM {
  uint32_t id;
};

using DstOperand = std::variant<
  DstOperandNull, DstOperandTemp, DstOperandIndexableTemp, DstOperandOutput,
  DstOperandOutputDepth>;

#pragma mark mov instructions
namespace {
struct InstMov {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src;
};

struct InstMovConditional {
  InstructionCommon _;
  SrcOperand src;
  DstOperand dst;
};

} // namespace

#pragma mark resource instructions
namespace {

struct InstSample {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
  int32_t offsets[3];
  std::optional<SrcOperand> min_lod_clamp;
  std::optional<DstOperand> feedback;
};

struct InstSampleCompare {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource; // swizzle must be .r channel
  SrcOperandSampler src_sampler;
  SrcOperand src_reference;
  int32_t offsets[3];
  std::optional<SrcOperand> min_lod_clamp;
  std::optional<DstOperand> feedback;
  bool level_zero;
};

struct InstSampleBias {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
  SrcOperand src_bias;
  int32_t offsets[3];
};

struct InstSampleDerivative {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
  SrcOperand src_x_derivative;
  SrcOperand src_y_derivative;
  int32_t offsets[3];
};

struct InstSampleLOD {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
  SrcOperand src_lod;
  int32_t offsets[3];
};

struct InstGather {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
  SrcOperand offset;
  std::optional<DstOperand> feedback;
};

struct InstGatherCompare {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
  SrcOperand src_reference; // single component
  SrcOperand offset;
  std::optional<DstOperand> feedback;
};

struct InstBufferInfo {
  DstOperand dst;
  std::variant<SrcOperandResource, SrcOperandUAV> src;
};

struct InstResourceInfo {
  DstOperand dst;
  SrcOperand src_mip_level;
  std::variant<SrcOperandResource, SrcOperandUAV> src_resource;
  enum class M { none, uint, rcp } modifier;
};
struct InstSampleInfo {
  DstOperand dst;
  std::optional<SrcOperandResource> src; // if null, get rasterizer info
  bool uint_result;
};

struct InstSamplePos {
  DstOperand dst;
  std::optional<SrcOperandResource> src; // if null, get rasterizer info
  SrcOperand src_sample_index;
};

struct InstLoad {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
};

struct InstStore {};

} // namespace

struct InstDotProduct {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
  uint8_t dimension;
};

struct InstSinCos {
  InstructionCommon _;
  DstOperand dst_sin;
  DstOperand dst_cos;
  SrcOperand src;
};

enum class FloatComparison { Equal, NotEqual, GreaterEqual, LessThan };

struct InstFloatCompare {
  InstructionCommon _;
  FloatComparison cmp;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
};

enum class IntegerComparison {
  Equal,
  NotEqual,
  SignedLessThan,
  SignedGreaterEqual,
  UnsignedLessThan,
  UnsignedGreaterEqual,
};

struct InstIntegerCompare {
  InstructionCommon _;
  IntegerComparison cmp;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
};

enum class FloatBinaryOp {
  Add,
  Mul,
  Div,
  Min,
  Max,
};

struct InstFloatBinaryOp {
  InstructionCommon _;
  FloatBinaryOp op;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
};

enum class FloatUnaryOp {
  Log2,
  Exp2,
  Rcp,
  Rsq,
  Sqrt,
  Fraction,
  RoundNearestEven,
  RoundNegativeInf,
  RoundPositiveInf,
  RoundZero,
};

struct InstFloatUnaryOp {
  InstructionCommon _;
  FloatUnaryOp op;
  DstOperand dst;
  SrcOperand src;
};

enum class IntegerUnaryOp {
  Neg,
  Not,
  ReverseBits,
  CountBits,
  FirstHiBitSigned,
  FirstHiBit,
  FirstLowBit,
};

struct InstIntegerUnaryOp {
  InstructionCommon _;
  IntegerUnaryOp op;
  DstOperand dst;
  SrcOperand src;
};

struct InstFloatMAD {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
  SrcOperand src2;
};

struct InstIntegerMAD {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
  SrcOperand src2;
  bool is_signed;
};

struct InstMaskedSumOfAbsDiff {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
  SrcOperand src2;
};

enum class IntegerBinaryOp {
  UMin,
  UMax,
  IMin,
  IMax,
  IShl,
  IShr,
  UShr,
  Xor,
  Or,
  And,
  Add,
};

struct InstIntegerBinaryOp {
  InstructionCommon _;
  IntegerBinaryOp op;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
};

enum class IntegerBinaryOpWithTwoDst {
  IMul,
  UAddCarry,
  UDiv,
  UMul,
  USubBorrow,
};

struct InstIntegerBinaryOpWithTwoDst {
  InstructionCommon _;
  IntegerBinaryOpWithTwoDst op;
  union {
    DstOperand dst_hi;
    DstOperand dst_quot;
  };
  union {
    DstOperand dst_low;
    DstOperand dst_rem;
    DstOperand dst_carry; // 1 if a carry is produced, 0 otherwise.
  };
  SrcOperand src0;
  SrcOperand src1;
};

enum class ConversionOp {
  HalfToFloat,
  FloatToHalf,
  FloatToSigned,
  SignedToFloat,
  FloatToUnsigned,
  UnsignedToFloat,
};

struct InstConvert {
  InstructionCommon _;
  ConversionOp op;
  DstOperand dst;
  SrcOperand src;
};

struct InstNop {};

struct InstPixelDiscard {};

struct InstPartialDerivative {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src;
  bool ddy;    // use ddx if false
  bool coarse; // use fine if false
};

struct InstCalcLOD {
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
};

struct InstSync {
  enum class Boundary { global, group } boundary;
  bool threadGroupMemoryFence;
  bool threadGroupExecutionFence;
};

enum class AtomicBinaryOp { And, Or, Xor, Add, IMax, IMin, UMax, UMin };

struct InstAtomicBinOp {
  AtomicBinaryOp op;
  std::variant<AtomicDstOperandUAV, AtomicDstOperandTGSM> dst;
  SrcOperand dst_address;
  SrcOperand src; // select one component
  std::optional<DstOperand>
    dst_original; // store single component, original value
};

struct InstAtomicCmpStore {
  std::variant<AtomicDstOperandUAV, AtomicDstOperandTGSM> dst;
  SrcOperand dst_address;
  SrcOperand src0; // select one component
  SrcOperand src1; // select one component
};

struct InstAtomicImmIncrement {
  DstOperand dst; // store single component, original value
  AtomicDstOperandUAV uav;
};

struct InstAtomicImmDecrement {
  DstOperand dst; // store single component, new value
  AtomicDstOperandUAV uav;
};

struct InstAtomicImmExchange {
  DstOperand dst; // store single component, original value
  std::variant<AtomicDstOperandUAV, AtomicDstOperandTGSM> dst_resource;
  SrcOperand dst_address;
  SrcOperand src; // select one component
};

struct InstAtomicImmCmpExchange {
  DstOperand dst; // store single component, original value,
  // note it's always written
  std::variant<AtomicDstOperandUAV, AtomicDstOperandTGSM> dst_resource;
  SrcOperand dst_address;
  SrcOperand src0; // select one component
  SrcOperand src1; // select one component
};

using Instruction = std::variant<
  /* Generic */
  InstMov, InstMovConditional, InstDotProduct, InstSinCos,               //
  InstConvert,                                                           //
  InstIntegerCompare, InstFloatCompare,                                  //
  InstFloatBinaryOp, InstIntegerBinaryOp, InstIntegerBinaryOpWithTwoDst, //
  InstFloatUnaryOp, InstIntegerUnaryOp,                                  //
  InstFloatMAD, InstIntegerMAD, InstMaskedSumOfAbsDiff,                  //
  InstSample, InstSampleCompare, InstGather, InstGatherCompare,          //
  InstSampleBias, InstSampleDerivative, InstSampleLOD,                   //
  InstSamplePos, InstSampleInfo, InstBufferInfo, InstResourceInfo,       //
  InstLoad, InstStore,                                                   //
  InstNop,
  /* Pixel Shader */
  InstPixelDiscard, InstPartialDerivative, InstCalcLOD,
  /* Atomics */
  InstSync, InstAtomicBinOp, InstAtomicCmpStore,   //
  InstAtomicImmCmpExchange, InstAtomicImmExchange, //
  InstAtomicImmIncrement, InstAtomicImmDecrement>;

#pragma region shader reflection

struct ResourceRange {
  uint32_t range_id;
  uint32_t lower_bound;
  uint32_t size;
  uint32_t space;
};

struct ShaderResourceViewInfo {
  ResourceRange range;
  shader::common::ScalerDataType scaler_type;
  shader::common::ResourceType resource_type;
  bool read;
  bool sampled;
  bool compared; // therefore we use depth texture!

  uint32_t strucure_stride = 0; // =0 for typed and raw
};
struct UnorderedAccessViewInfo {
  ResourceRange range;
  shader::common::ScalerDataType scaler_type;
  shader::common::ResourceType resource_type;
  bool read;
  bool written;
  bool global_coherent;
  bool rasterizer_order;
  bool with_counter;

  uint32_t strucure_stride = 0; // =0 for typed and raw
};
struct ConstantBufferInfo {
  ResourceRange range;
  uint32_t size_in_vec4;
};
struct SamplerInfo {
  ResourceRange range;
};

struct ThreadgroupBufferInfo {
  uint32_t size_in_uint;
  uint32_t size;
  uint32_t stride;
  bool structured;
};

class ShaderInfo {
public:
  std::vector<std::array<uint32_t,4>> immConstantBufferData;
  std::map<uint32_t, ShaderResourceViewInfo> srvMap;
  std::map<uint32_t, UnorderedAccessViewInfo> uavMap;
  std::map<uint32_t, ConstantBufferInfo> cbufferMap;
  std::map<uint32_t, SamplerInfo> samplerMap;
  std::map<uint32_t, ThreadgroupBufferInfo> tgsmMap;
  uint32_t tempRegisterCount;
  std::unordered_map<
    uint32_t, std::pair<uint32_t /* count */, uint32_t /* mask */>>
    indexableTempRegisterCounts;
};

#pragma endregion

#pragma region basicblock

class BasicBlock;

struct BasicBlockCondition {
  SrcOperand operand;
  bool test_nonzero; //
};

struct BasicBlockConditionalBranch {
  BasicBlockCondition cond;
  std::shared_ptr<BasicBlock> true_branch;
  std::shared_ptr<BasicBlock> false_branch;
};

struct BasicBlockUnconditionalBranch {
  std::shared_ptr<BasicBlock> target;
};

struct BasicBlockSwitch {
  std::map<uint32_t, std::shared_ptr<BasicBlock>> cases;
  std::shared_ptr<BasicBlock> case_default;
};

struct BasicBlockReturn {};

struct BasicBlockUndefined {};

using BasicBlockTarget = std::variant<
  BasicBlockConditionalBranch, BasicBlockUnconditionalBranch, BasicBlockSwitch,
  BasicBlockReturn, BasicBlockUndefined>;

class BasicBlock {
public:
  std::vector<Instruction> instructions;
  BasicBlockTarget target = BasicBlockUndefined{};
};

#pragma endregion

struct Reflection {
  bool has_binding_map;
};

Reflection convertDXBC(
  const void *dxbc, uint32_t dxbcSize, llvm::LLVMContext &context,
  llvm::Module &module
);

} // namespace dxmt::dxbc

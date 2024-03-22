#pragma once
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"

#include "air_constants.hpp"

#include <cassert>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <variant>
#include <vector>

#include "shader_common.hpp"

namespace dxmt::dxbc {

inline static air::ESampleInterpolation
ToAirInterpolation(microsoft::D3D10_SB_INTERPOLATION_MODE mode) {
  using namespace microsoft;
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

struct swizzle {
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
  bool operator==(swizzle const &o) const {
    return r == o.r && g == o.g && b == o.b && a == o.a;
  };
};

constexpr swizzle swizzle_identity = {0, 1, 2, 3};

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
  swizzle swizzle;
  bool abs;
  bool neg;
};

struct SrcOperandImmediate32 {
  SrcOperandCommon _;
  union {
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

using SrcOperand =
    std::variant<SrcOperandImmediate32, SrcOperandTemp, SrcOperandIndexableTemp,
                 SrcOperandInput, SrcOperandConstantBuffer,
                 SrcOperandImmediateConstantBuffer>;

struct SrcOperandResource {
  uint32_t range_id;
  OperandIndex index;
  swizzle read_swizzle; // if appliable?
};

struct SrcOperandSampler {
  uint32_t range_id;
  OperandIndex index;
};

struct SrcOperandUAV {
  uint32_t range_id;
  OperandIndex index;
};

struct SrcOperandTGSM {
  uint32_t id;
  OperandIndex index;
};

using DstOperand =
    std::variant<DstOperandNull, DstOperandTemp, DstOperandIndexableTemp,
                 DstOperandOutput, DstOperandOutputDepth>;

struct InstMov {
  InstructionCommon _;
  SrcOperand src;
  DstOperand dst;
};

struct InstMovConditional {
  InstructionCommon _;
  SrcOperand src;
  DstOperand dst;
};

struct InstDotProduct {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src0;
  SrcOperand src1;
};

struct InstSinCos {
  InstructionCommon _;
  DstOperand dst_sin;
  DstOperand dst_cos;
  SrcOperand src;
};

struct InstLoad {};

struct InstStore {};

struct InstSample {};

struct InstGather {};

enum class FloatComparison { Equal, NotEqual, GreaterEqual, LessThan };

struct InstFloatCompare {
  InstructionCommon _;
  FloatComparison cmp;
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
};

struct InstFloatUnaryOp {
  InstructionCommon _;
  FloatUnaryOp op;
  DstOperand dst;
  SrcOperand src;
};

enum class IntegerBinaryOp {
  UMin,
  UMax,
  IMin,
  IMax,
};

struct InstIntegerBinaryOp {
  InstructionCommon _;
  IntegerBinaryOp op;
  DstOperand dst;
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

struct InstPixelDiscard {};

struct InstPartialDerivative {
  InstructionCommon _;
  DstOperand dst;
  SrcOperand src;
  bool ddy;    // use ddx if false
  bool coarse; // use fine if false
};

struct InstCalcLOD {
  // InstructionCommon _;
  DstOperand dst;
  SrcOperand src_address;
  SrcOperandResource src_resource;
  SrcOperandSampler src_sampler;
};

using Instruction = std::variant<
    /* Generic */
    InstMov, InstMovConditional, InstDotProduct, InstSinCos, //
    InstConvert,                                             //
    InstIntegerCompare, InstFloatCompare,                    //
    InstFloatBinaryOp, InstIntegerBinaryOp,                  //
    InstFloatUnaryOp,                                        //
    InstSample, InstLoad, InstStore,                         //
    /* Pixel Shader */
    InstPixelDiscard, InstPartialDerivative, InstCalcLOD>;

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
};
struct UnorderedAccessViewInfo {
  ResourceRange range;
  shader::common::ScalerDataType scaler_type;
  shader::common::ResourceType resource_type;
  bool read;
  bool written;
  bool sampled;
  bool global_coherent;
  bool rasterizer_order;
  bool with_counter;
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
  std::vector<uint32_t> immConstantBufferData;
  std::map<uint32_t, ShaderResourceViewInfo> srvMap;
  std::map<uint32_t, UnorderedAccessViewInfo> uavMap;
  std::map<uint32_t, ConstantBufferInfo> cbufferMap;
  std::map<uint32_t, SamplerInfo> samplerMap;
  std::map<uint32_t, ThreadgroupBufferInfo> tgsmMap;
  uint32_t tempRegisterCount;
  std::unordered_map<uint32_t,
                     std::pair<uint32_t /* count */, uint32_t /* mask */>>
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

using BasicBlockTarget =
    std::variant<BasicBlockConditionalBranch, BasicBlockUnconditionalBranch,
                 BasicBlockSwitch, BasicBlockReturn, BasicBlockUndefined>;

class BasicBlock {
public:
  std::vector<Instruction> instructions;
  BasicBlockTarget target = BasicBlockUndefined{};
};

#pragma endregion

} // namespace dxmt::dxbc

#include "air_operation.hpp"
#include "air_builder.hpp"
#include "air_constants.hpp"
#include "air_type.hpp"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>
#include <utility>

using namespace llvm;

namespace dxmt::air {

AirOp::AirOp(AirType &types, AirBuilder &builder, llvm::LLVMContext &context,
             llvm::Module &module)
    : types(types), builder(builder), context(context), module(module) {
  commonAttributes = AttributeList::get(
      context,
      {std::make_pair<unsigned int, Attribute>(
           ~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)),
       std::make_pair<unsigned int, Attribute>(
           ~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)),
       std::make_pair<unsigned int, Attribute>(
           ~0U, Attribute::get(context, Attribute::AttrKind::ReadNone))});
}

FunctionCallee AirOp::GetDotProduct() {
  //  auto attribtues = {Attribute::get(context,
  // Attribute::AttrKind::NoUnwind),
  //                    Attribute::get(context,
  //                    Attribute::AttrKind::WillReturn),
  //                    Attribute::get(context, Attribute::AttrKind::Memory)};
  auto fn = module.getOrInsertFunction(
      "air.dot.v4f32",
      FunctionType::get(types._float, {types._float4, types._float4},
                        false),
      commonAttributes);
  return fn;
}

Twine AirOp::GetOverloadSymbolSegment(air::EType overload) {
  switch (overload) {
  case air::EType::float4:
    return "v4f32";
  case air::EType::float3:
    return "v3f32";
  case air::EType::float2:
    return "v2f32";
  case air::EType::_float:
    return "f32";
    //   case air::EType:::
    // return "v4f32";
  case air::EType::uint4:
    return "v4i32";
    // assembly name
  default:
    llvm::outs() << overload;
    assert(0 && "Unexpected path");
    break;
  }
}

Value *AirOp::CreateFloatUnaryOp(air::EFloatUnaryOp op, llvm::Value *src) {
  // check: ensure src is float4
  // special case
  if (op == +EFloatUnaryOp::_rcp) {
    return builder.builder.CreateFDiv(
        builder.CreateConstantVector(
            (std::array<float, 4>){1.0f, 1.0f, 1.0f, 1.0f}),
        src);
  }
  // real implementation start
  auto fastMath = builder.builder.getFastMathFlags().isFast();
  std::stringstream fn_name("air.");
  if (fastMath)
    fn_name << "fast_";
  fn_name << op._to_string();
  fn_name << ".v4f32"; // assumption

  auto fnCallee = (module.getOrInsertFunction(
      fn_name.str(),
      FunctionType::get(types._float4, {types._float4}, false),
      AttributeList::get(context, commonAttributes)));
  return builder.builder.CreateCall(fnCallee, {src});
};

Value *AirOp::CreateFloatBinaryOp(air::EFloatBinaryOp op, llvm::Value *src0,
                                  llvm::Value *src1) {
  // check: ensure srcs are float4
  // special case
  if (op == +EFloatBinaryOp::_add) {
    return builder.builder.CreateFAdd(src0, src1);
  }
  if (op == +EFloatBinaryOp::_div) {
    return builder.builder.CreateFDiv(src0, src1);
  }
  if (op == +EFloatBinaryOp::_mul) {
    return builder.builder.CreateFMul(src0, src1);
  }
  // real implementation start
  auto fastMath = builder.builder.getFastMathFlags().isFast();
  std::stringstream fn_name("air.");
  if (fastMath)
    fn_name << "fast_";
  fn_name << op._to_string();
  fn_name << ".v4f32"; // assumption

  auto fnCallee = (module.getOrInsertFunction(
      fn_name.str(),
      FunctionType::get(types._float4, {types._float4, types._float4},
                        false),
      commonAttributes));
  return builder.builder.CreateCall(fnCallee, {src0, src1});
};

Value *AirOp::CreateBitwiseUnaryOp(air::EBitwiseUnaryOp op, llvm::Value *src) {
  // check: ensure src is int4
  // special case
  if (op == +EBitwiseUnaryOp::_not) {
    return builder.builder.CreateNot(src);
  }
  if (op == +EBitwiseUnaryOp::__clz) {
    return builder.builder.CreateCall(
        module.getOrInsertFunction(
            "air.clz.v4i32",
            FunctionType::get(types._int4, {types._int4, types._bool},
                              false),
            commonAttributes),
        {src, ConstantInt::get(types._bool, 0)});
  }
  if (op == +EBitwiseUnaryOp::__ctz) {
    return builder.builder.CreateCall(
        module.getOrInsertFunction(
            "air.ctz.v4i32",
            FunctionType::get(types._int4, {types._int4, types._bool},
                              false),
            commonAttributes),
        {src, ConstantInt::get(types._bool, 0)});
  }
  // real implementation start
  std::stringstream fn_name("air.");
  fn_name << op._to_string();
  fn_name << ".v4i32"; // assumption

  auto fnCallee = (module.getOrInsertFunction(
      fn_name.str(),
      FunctionType::get(types._int4, {types._int4}, false),
      commonAttributes));
  return builder.builder.CreateCall(fnCallee, {src});
};

} // namespace dxmt::air
#include "air_operation.hpp"
#include "air_builder.hpp"
#include "air_constants.hpp"
#include "air_type.hpp"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace dxmt::air {

AirOp::AirOp(AirType &types, AirBuilder &builder, llvm::LLVMContext &context,
             llvm::Module &module)
    : types(types), builder(builder), context(context), module(module) {}

FunctionCallee AirOp::GetDotProduct() {
  //  auto attribtues = {Attribute::get(context,
  // Attribute::AttrKind::NoUnwind),
  //                    Attribute::get(context,
  //                    Attribute::AttrKind::WillReturn),
  //                    Attribute::get(context, Attribute::AttrKind::Memory)};
  auto fn = (module.getOrInsertFunction(
      "air.dot.v4f32",
      FunctionType::get(types.tyFloat, {types.tyFloatV4, types.tyFloatV4},
                        false)));
  // cast<Function>(fn.getCallee())->setCallingConv(CallingConv::C);
  // cast<Function>(fn.getCallee())
  //     ->setAttributes(AttributeList::get(
  //         context,
  //         {{0, Attribute::get(context, Attribute::AttrKind::NoUnwind)}}));
  // auto fnType = FunctionType::get(types.tyFloat,
  //                                 {types.tyFloatV4, types.tyFloatV4}, false);
  // auto func =
  //     Function::Create(fnType, Function::ExternalLinkage, 0, "air.dot.v4f32",
  //     &module);
  auto func = cast<Function>(fn.getCallee());
  // func->addFnAttr(Attribute::AttrKind::NoUnwind);

  func->addFnAttr(Attribute::AttrKind::NoUnwind);
  func->addFnAttr(Attribute::AttrKind::WillReturn);
  func->addFnAttr(Attribute::AttrKind::ReadNone);
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

} // namespace dxmt::air
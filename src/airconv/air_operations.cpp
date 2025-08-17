#include "air_operations.hpp"

namespace dxmt::air {

ReaderIO<AIRBuilderContext, AIRBuilderContext> get_context() {
  return ReaderIO<AIRBuilderContext, AIRBuilderContext>(
    [](struct AIRBuilderContext ctx) { return ctx; }
  );
};

AIRBuilderResult get_int(uint32_t value) {
  return make_op([=](auto ctx) { return ctx.builder.getInt32(value); });
};

AIRBuilderResult get_float(float value) {
  return make_op([=](auto ctx) {
    return llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value});
  });
};

AIRBuilderResult call_unpack_impl(
  std::string op, pvalue src, llvm::Type *src_type, llvm::Type *dst_type
) {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto att = AttributeList::get(
    context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
              {~0U, Attribute::get(context, Attribute::AttrKind::MustProgress)},
              {~0U, Attribute::get(context, Attribute::AttrKind::NoFree)},
              {~0U, Attribute::get(context, Attribute::AttrKind::NoSync)},
              {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
  );
  auto fn = (module.getOrInsertFunction(
    "air.unpack." + op, llvm::FunctionType::get(dst_type, {src_type}, false),
    att
  ));
  co_return ctx.builder.CreateCall(fn, {src});
};

AIRBuilderResult load_from_device_buffer(
  llvm::Type *value_type, pvalue base_addr, pvalue aligned_offest,
  uint32_t element_offset, uint32_t align
) {
  auto ctx = co_yield get_context();
  co_return ctx.builder.CreateAlignedLoad(
    value_type,
    ctx.builder.CreateConstGEP1_32(
      value_type,
      ctx.builder.CreateBitCast(
        ctx.builder.CreateGEP(ctx.types._byte, base_addr, {aligned_offest}),
        value_type->getPointerTo((uint32_t)AddressSpace::device)
      ),
      element_offset
    ),
    llvm::MaybeAlign(align)
  );
};

AIRBuilderResult unpack_fvec4_from_addr(
  MTLAttributeFormat format, pvalue base_addr, pvalue aligned_offest
) {
  auto ctx = co_yield get_context();
  auto &types = ctx.types;
  auto &builder = ctx.builder;

  std::string op;
  llvm::Type *src_type = nullptr;
  uint32_t align = 0;
  llvm::Type *dst_type = nullptr;
  switch (format) {
  case MTLAttributeFormat::UCharNormalized:
    op = "unorm1x8.f32";
    src_type = types._byte;
    align = 1;
    dst_type = types._float;
    break;
  case MTLAttributeFormat::UChar2Normalized:
    op = "unorm2x8.v2f32";
    src_type = types._short;
    align = 1;
    dst_type = types._float2;
    break;
  case MTLAttributeFormat::UChar4Normalized:
  case MTLAttributeFormat::UChar4Normalized_BGRA:
    op = "unorm4x8.v4f32";
    src_type = types._int;
    align = 1;
    dst_type = types._float4;
    break;
  case MTLAttributeFormat::CharNormalized:
    op = "snorm1x8.f32";
    src_type = types._byte;
    align = 1;
    dst_type = types._float;
    break;
  case MTLAttributeFormat::Char2Normalized:
    op = "snorm2x8.v2f32";
    src_type = types._short;
    align = 1;
    dst_type = types._float2;
    break;
  case MTLAttributeFormat::Char4Normalized:
    op = "snorm4x8.v4f32";
    src_type = types._int;
    align = 1;
    dst_type = types._float4;
    break;
  case MTLAttributeFormat::UShortNormalized:
    op = "unorm1x16.f32";
    src_type = types._short;
    align = 2;
    dst_type = types._float;
    break;
  case MTLAttributeFormat::UShort2Normalized:
    op = "unorm2x16.v2f32";
    src_type = types._int;
    align = 2;
    dst_type = types._float2;
    break;
  case MTLAttributeFormat::UShort4Normalized:
    op = "unorm4x16.v4f32";
    src_type = types._long;
    align = 2;
    dst_type = types._float4;
    break;
  case MTLAttributeFormat::ShortNormalized:
    op = "snorm1x16.f32";
    src_type = types._short;
    align = 2;
    dst_type = types._float;
    break;
  case MTLAttributeFormat::Short2Normalized:
    op = "snorm2x16.v2f32";
    src_type = types._int;
    align = 2;
    dst_type = types._float2;
    break;
  case MTLAttributeFormat::Short4Normalized:
    op = "snorm4x16.v4f32";
    src_type = types._long;
    align = 2;
    dst_type = types._float4;
    break;
  case MTLAttributeFormat::UInt1010102Normalized:
    op = "unorm.rgb10a2.v4f32";
    src_type = types._int;
    align = 4;
    dst_type = types._float4;
    break;
  case MTLAttributeFormat::Int1010102Normalized:
    assert(0 && "unused");
    // op = "unorm.rgb10a2.v4f32";
    // src_type = types._int;
    // align = 4;
    // dst_type = types._float4;
    break;
  case MTLAttributeFormat::FloatRG11B10:
    op = "unorm.rg11b10f.v3f32";
    src_type = types._int;
    align = 4;
    dst_type = types._float3;
    break;
  case MTLAttributeFormat::FloatRGB9E5:
    op = "unorm.rgb9e5.v3f32";
    src_type = types._int;
    align = 4;
    dst_type = types._float3;
    break;
  default:
    break;
  }
  assert(src_type && dst_type);
  pvalue ret = co_yield call_unpack_impl(
    op,
    co_yield load_from_device_buffer(
      src_type, base_addr, aligned_offest, 0, align
    ),
    src_type, dst_type
  );
  if (format == MTLAttributeFormat::UChar4Normalized_BGRA) {
    ret = builder.CreateShuffleVector(ret, {2, 1, 0, 3});
  }
  if (dst_type == types._float) {
    ret = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._float4), ret, (int)0
    );
    ret = builder.CreateInsertElement(ret, co_yield get_float(0), (int)1);
    ret = builder.CreateInsertElement(ret, co_yield get_float(0), (int)2);
    ret = builder.CreateInsertElement(ret, co_yield get_float(1), (int)3);
  } else if (dst_type == types._float2) {
    ret = builder.CreateShuffleVector(ret, {0, 1, -1, -1});
    ret = builder.CreateInsertElement(ret, co_yield get_float(0), (int)2);
    ret = builder.CreateInsertElement(ret, co_yield get_float(1), (int)3);
  } else if (dst_type == types._float3) {
    ret = builder.CreateShuffleVector(ret, {0, 1, 2, -1});
    ret = builder.CreateInsertElement(ret, co_yield get_float(1), (int)3);
  }
  co_return ret;
};

AIRBuilderResult pull_vec4_from_addr_checked(
  MTLAttributeFormat format, pvalue base_addr, pvalue byte_offset
) {
  auto ctx = co_yield get_context();
  auto &types = ctx.types;
  auto &builder = ctx.builder;

  pvalue value = nullptr;

  switch (format) {
  case MTLAttributeFormat::Char:
    value = co_yield load_from_device_buffer(
      types._byte, base_addr, byte_offset, 0, 1
    );
    value = builder.CreateSExt(value, types._int);
    value = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._int4), value, (int)0
    );
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)1);
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::Char2:
    value = co_yield load_from_device_buffer(
      types._char2, base_addr, byte_offset, 0, 1
    );
    value = builder.CreateSExt(value, types._int2);
    value = builder.CreateShuffleVector(value, {0, 1, -1, -1});
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::Char4:
    value = co_yield load_from_device_buffer(
      types._char4, base_addr, byte_offset, 0, 1
    );
    value = builder.CreateSExt(value, types._int4);
    break;
  case MTLAttributeFormat::UChar:
    value = co_yield load_from_device_buffer(
      types._byte, base_addr, byte_offset, 0, 1
    );
    value = builder.CreateZExt(value, types._int);
    value = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._int4), value, (int)0
    );
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)1);
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::UChar2:
    value = co_yield load_from_device_buffer(
      types._char2, base_addr, byte_offset, 0, 1
    );
    value = builder.CreateZExt(value, types._int2);
    value = builder.CreateShuffleVector(value, {0, 1, -1, -1});
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::UChar4:
    value = co_yield load_from_device_buffer(
      types._char4, base_addr, byte_offset, 0, 1
    );
    value = builder.CreateZExt(value, types._int4);
    break;
  case MTLAttributeFormat::Short:
    value = co_yield load_from_device_buffer(
      types._short, base_addr, byte_offset, 0, 2
    );
    value = builder.CreateSExt(value, types._int);
    value = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._int4), value, (int)0
    );
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)1);
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::Short2:
    value = co_yield load_from_device_buffer(
      types._short2, base_addr, byte_offset, 0, 2
    );
    value = builder.CreateSExt(value, types._int2);
    value = builder.CreateShuffleVector(value, {0, 1, -1, -1});
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::Short4:
    value = co_yield load_from_device_buffer(
      types._short4, base_addr, byte_offset, 0, 2
    );
    value = builder.CreateSExt(value, types._int4);
    break;
  case MTLAttributeFormat::UShort:
    value = co_yield load_from_device_buffer(
      types._short, base_addr, byte_offset, 0, 2
    );
    value = builder.CreateZExt(value, types._int);
    value = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._int4), value, (int)0
    );
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)1);
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::UShort2:
    value = co_yield load_from_device_buffer(
      types._short2, base_addr, byte_offset, 0, 2
    );
    value = builder.CreateZExt(value, types._int2);
    value = builder.CreateShuffleVector(value, {0, 1, -1, -1});
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::UShort4:
    value = co_yield load_from_device_buffer(
      types._short4, base_addr, byte_offset, 0, 2
    );
    value = builder.CreateZExt(value, types._int4);
    break;
  case MTLAttributeFormat::Half:
    value = co_yield load_from_device_buffer(
      types._half, base_addr, byte_offset, 0, 2
    );
    value = ctx.air.CreateConvertToFloat(value);
    value = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._float4), value, (int)0
    );
    value = builder.CreateInsertElement(value, co_yield get_float(0), (int)1);
    value = builder.CreateInsertElement(value, co_yield get_float(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_float(1), (int)3);
    break;
  case MTLAttributeFormat::Half2:
    value = co_yield load_from_device_buffer(
      types._half2, base_addr, byte_offset, 0, 2
    );
    value = ctx.air.CreateConvertToFloat(value);
    value = builder.CreateShuffleVector(value, {0, 1, -1, -1});
    value = builder.CreateInsertElement(value, co_yield get_float(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_float(1), (int)3);
    break;
  case MTLAttributeFormat::Half4:
    value = co_yield load_from_device_buffer(
      types._half4, base_addr, byte_offset, 0, 2
    );
    value = ctx.air.CreateConvertToFloat(value);
    break;
  case MTLAttributeFormat::Float:
    value = co_yield load_from_device_buffer(
      types._float, base_addr, byte_offset, 0, 4
    );
    value = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._float4), value, (int)0
    );
    value = builder.CreateInsertElement(value, co_yield get_float(0), (int)1);
    value = builder.CreateInsertElement(value, co_yield get_float(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_float(1), (int)3);
    break;
  case MTLAttributeFormat::Float2:
    value = co_yield load_from_device_buffer(
      types._float2, base_addr, byte_offset, 0, 4
    );
    value = builder.CreateShuffleVector(value, {0, 1, -1, -1});
    value = builder.CreateInsertElement(value, co_yield get_float(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_float(1), (int)3);
    break;
  case MTLAttributeFormat::Float3:
    value = co_yield load_from_device_buffer(
      types._float3, base_addr, byte_offset, 0, 4
    );
    value = builder.CreateShuffleVector(value, {0, 1, 2, -1});
    value = builder.CreateInsertElement(value, co_yield get_float(1), (int)3);
    break;
  case MTLAttributeFormat::Float4:
    value = co_yield load_from_device_buffer(
      types._float4, base_addr, byte_offset, 0, 4
    );
    break;
  case MTLAttributeFormat::UInt:
  case MTLAttributeFormat::Int:
    value = co_yield load_from_device_buffer(
      types._int, base_addr, byte_offset, 0, 4
    );
    value = builder.CreateInsertElement(
      llvm::PoisonValue::get(types._int4), value, (int)0
    );
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)1);
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::UInt2:
  case MTLAttributeFormat::Int2:
    value = co_yield load_from_device_buffer(
      types._int2, base_addr, byte_offset, 0, 4
    );
    value = builder.CreateShuffleVector(value, {0, 1, -1, -1});
    value = builder.CreateInsertElement(value, co_yield get_int(0), (int)2);
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::UInt3:
  case MTLAttributeFormat::Int3:
    value = co_yield load_from_device_buffer(
      types._int3, base_addr, byte_offset, 0, 4
    );
    value = builder.CreateShuffleVector(value, {0, 1, 2, -1});
    value = builder.CreateInsertElement(value, co_yield get_int(1), (int)3);
    break;
  case MTLAttributeFormat::UInt4:
  case MTLAttributeFormat::Int4:
    value = co_yield load_from_device_buffer(
      types._int4, base_addr, byte_offset, 0, 4
    );
    break;
  case MTLAttributeFormat::UCharNormalized:
  case MTLAttributeFormat::UChar2Normalized:
  case MTLAttributeFormat::UChar4Normalized:
  case MTLAttributeFormat::UChar4Normalized_BGRA:
  case MTLAttributeFormat::CharNormalized:
  case MTLAttributeFormat::Char2Normalized:
  case MTLAttributeFormat::Char4Normalized:
  case MTLAttributeFormat::UShortNormalized:
  case MTLAttributeFormat::UShort2Normalized:
  case MTLAttributeFormat::UShort4Normalized:
  case MTLAttributeFormat::ShortNormalized:
  case MTLAttributeFormat::Short2Normalized:
  case MTLAttributeFormat::Short4Normalized:
  case MTLAttributeFormat::FloatRG11B10:
  case MTLAttributeFormat::FloatRGB9E5:
  case MTLAttributeFormat::Int1010102Normalized:
  case MTLAttributeFormat::UInt1010102Normalized:
    value = co_yield unpack_fvec4_from_addr(format, base_addr, byte_offset);
    break;
  case MTLAttributeFormat::Invalid:
    break;
  }
  assert(value);

  co_return value;
};

AIRBuilderResult pull_vec4_from_addr(
  MTLAttributeFormat format, pvalue base_addr, pvalue byte_offset
) {
  return make_op([=](struct AIRBuilderContext ctx) -> llvm::Expected<pvalue> {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &builder = ctx.builder;

    auto current = builder.GetInsertBlock();

    auto is_null_binding = builder.CreateICmpEQ(
      builder.CreatePtrToInt(base_addr, ctx.types._long), builder.getInt64(0)
    );

    auto pull_vertex =
      llvm::BasicBlock::Create(context, "", current->getParent());
    auto continuation =
      llvm::BasicBlock::Create(context, "", current->getParent());

    builder.CreateCondBr(is_null_binding, continuation, pull_vertex);
    builder.SetInsertPoint(pull_vertex);

    auto ret =
      pull_vec4_from_addr_checked(format, base_addr, byte_offset).build(ctx);
    if (auto err = ret.takeError()) {
      return err;
    }
    auto value = ret.get();

    builder.CreateBr(continuation);
    builder.SetInsertPoint(continuation);

    auto phi = builder.CreatePHI(value->getType(), 2);
    phi->addIncoming(ConstantAggregateZero::get(value->getType()), current);
    phi->addIncoming(value, pull_vertex);

    return phi;
  });
};

}; // namespace dxmt::air
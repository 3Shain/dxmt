#include "dxil_to_msl.hpp"
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <cstdlib>
#include <algorithm>

#define DXTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxil_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt::dxil {

enum DXIntrinsicOpcode {
  DXOP_LoadInput = 4,
  DXOP_StoreOutput = 5,
  DXOP_CreateHandle = 57,
  DXOP_CBufferLoadLegacy = 59,
  DXOP_ThreadId = 93,
  DXOP_GroupId = 94,
  DXOP_ThreadIDInGroup = 95,
  DXOP_FlattenedThreadIDInGroup = 96,
  DXOP_BufferLoad = 68,
  DXOP_BufferStore = 69,
  DXOP_TextureLoad = 66,
  DXOP_TextureStore = 67,
  DXOP_TextureGather = 73,
  DXOP_TextureSample = 60,
  DXOP_TextureSampleCmp = 63,
  DXOP_Barrier = 80,
  DXOP_Unary = 13,
  DXOP_Binary = 14,
  DXOP_Tertiary = 15,
  DXOP_Dot2 = 54,
  DXOP_Dot3 = 55,
  DXOP_Dot4 = 56,
  DXOP_MakeDouble = 101,
  DXOP_SplitDouble = 102,
  DXOP_RawBufferLoad = 1025,
  DXOP_RawBufferStore = 1026,
  DXOP_AtomicBinOp = 78,
  DXOP_AtomicCompareExchange = 79,
  DXOP_DerivCoarseX = 83,
  DXOP_DerivCoarseY = 84,
  DXOP_DerivFineX = 85,
  DXOP_DerivFineY = 86,
  DXOP_CalcLOD = 81,
  DXOP_Texture2DMSGetSamplePosition = 97,
  DXOPRenderTargetGetSamplePosition = 98,
  DXOP_NumPrimitives = 109,
  DXOP_NumOutputVertices = 110,
};

enum DXILMathOpcode {
  DXILOP_FAbs = 6,
  DXILOP_Saturate = 7,
  DXILOP_IsNaN = 8,
  DXILOP_IsInf = 9,
  DXILOP_IsFinite = 10,
  DXILOP_Cos = 12,
  DXILOP_Sin = 13,
  DXILOP_Tan = 14,
  DXILOP_Acos = 15,
  DXILOP_Asin = 16,
  DXILOP_Atan = 17,
  DXILOP_Exp = 21,
  DXILOP_Frc = 22,
  DXILOP_Log = 23,
  DXILOP_Sqrt = 24,
  DXILOP_Rsqrt = 25,
  DXILOP_Round_ne = 26,
  DXILOP_Round_ni = 27,
  DXILOP_Round_pi = 28,
  DXILOP_Round_z = 29,
  DXILOP_FMax = 35,
  DXILOP_FMin = 36,
  DXILOP_IMax = 37,
  DXILOP_IMin = 38,
  DXILOP_UMax = 39,
  DXILOP_UMin = 40,
  DXILOP_FMad = 46,
  DXILOP_Fma = 47,
  DXILOP_IMad = 48,
  DXILOP_UMad = 49,
};

static const char *kMetalHeader = R"(#include <metal_stdlib>
using namespace metal;

)";

static std::string escapeName(const std::string &s) {
  if (s.empty()) return "_";
  std::string r;
  for (char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_')
      r += c;
    else
      r += '_';
  }
  if (!r.empty() && r[0] >= '0' && r[0] <= '9')
    r = "_" + r;
  return r;
}

static const char *componentSuffix(uint32_t component) {
  switch (component & 3) {
  case 0: return ".x";
  case 1: return ".y";
  case 2: return ".z";
  default: return ".w";
  }
}

static std::string varyingField(const char *base, uint32_t signature_id) {
  switch (signature_id) {
  case 0: return std::string(base) + ".position";
  case 1: return std::string(base) + ".v0";
  case 2: return std::string(base) + ".v1";
  case 3: return std::string(base) + ".v2";
  case 4: return std::string(base) + ".v3";
  case 5: return std::string(base) + ".v4";
  case 6: return std::string(base) + ".v5";
  case 7: return std::string(base) + ".v6";
  case 8: return std::string(base) + ".v7";
  default: return std::string(base) + ".v0";
  }
}

static bool parseUnsignedLiteral(const std::string &text, uint32_t &value) {
  if (text.empty())
    return false;
  char *end = nullptr;
  unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
  if (!end || *end != '\0')
    return false;
  value = (uint32_t)parsed;
  return true;
}

static bool startsWith(const std::string &text, const char *prefix) {
  return text.rfind(prefix, 0) == 0;
}

static std::string resolveBindingName(const std::string &handle, const char *target_prefix) {
  const char *prefixes[] = {"srv", "uav", "cbuf", "buf", "tex", "samp"};
  for (auto *prefix : prefixes) {
    if (startsWith(handle, prefix)) {
      const char *suffix = handle.c_str() + std::strlen(prefix);
      return std::string(target_prefix) + suffix;
    }
  }
  return handle;
}

static bool isKnownDXIntrinsic(uint32_t intrinsic_id) {
  switch (intrinsic_id) {
  case DXOP_LoadInput:
  case DXOP_StoreOutput:
  case DXOP_CreateHandle:
  case DXOP_CBufferLoadLegacy:
  case DXOP_ThreadId:
  case DXOP_GroupId:
  case DXOP_ThreadIDInGroup:
  case DXOP_FlattenedThreadIDInGroup:
  case DXOP_BufferLoad:
  case DXOP_BufferStore:
  case DXOP_TextureLoad:
  case DXOP_TextureStore:
  case DXOP_TextureGather:
  case DXOP_TextureSample:
  case DXOP_TextureSampleCmp:
  case DXOP_Barrier:
  case DXOP_Unary:
  case DXOP_Binary:
  case DXOP_Tertiary:
  case DXOP_Dot2:
  case DXOP_Dot3:
  case DXOP_Dot4:
  case DXOP_MakeDouble:
  case DXOP_SplitDouble:
  case DXOP_RawBufferLoad:
  case DXOP_RawBufferStore:
  case DXOP_AtomicBinOp:
  case DXOP_AtomicCompareExchange:
  case DXOP_DerivCoarseX:
  case DXOP_DerivCoarseY:
  case DXOP_DerivFineX:
  case DXOP_DerivFineY:
  case DXOP_CalcLOD:
  case DXOP_Texture2DMSGetSamplePosition:
  case DXOPRenderTargetGetSamplePosition:
  case DXOP_NumPrimitives:
  case DXOP_NumOutputVertices:
    return true;
  default:
    return false;
  }
}

std::string DXILToMSL::getTypeName(const LLVMType &t, const LLVMModule &mod) {
  switch (t.kind) {
  case LLVMType::Void: return "void";
  case LLVMType::Float: return "float";
  case LLVMType::Double: return "float64_t";
  case LLVMType::Integer:
    if (t.bit_width == 1) return "bool";
    if (t.bit_width == 8) return "char";
    if (t.bit_width == 16) return "short";
    if (t.bit_width == 32) return "int";
    if (t.bit_width == 64) return "long";
    return "int";
  case LLVMType::Pointer: return "device char*";
  case LLVMType::Struct: return "char" + std::to_string((uint64_t)&t % 997);
  case LLVMType::Array: return "array<char," + std::to_string(t.bit_width) + ">";
  case LLVMType::Vector: {
    if (t.subtypes.empty())
      return "float4";
    return getTypeName(t.subtypes[0], mod) + std::to_string(t.bit_width);
  }
  case LLVMType::Function: return "void";
  }
  return "int";
}

std::string DXILToMSL::getVectorTypeName(const LLVMType &elem, uint32_t count, const LLVMModule &mod) {
  return getTypeName(elem, mod) + std::to_string(count);
}

uint32_t DXILToMSL::getTypeSize(const LLVMType &t, const LLVMModule &mod) {
  switch (t.kind) {
  case LLVMType::Void: return 0;
  case LLVMType::Float: return 4;
  case LLVMType::Double: return 8;
  case LLVMType::Integer: return (t.bit_width + 7) / 8;
  case LLVMType::Pointer: return 8;
  case LLVMType::Struct: {
    uint32_t s = 0;
    for (auto &st : t.subtypes)
      s += getTypeSize(st, mod);
    return s;
  }
  case LLVMType::Array: return t.bit_width * (t.subtypes.empty() ? 4 : getTypeSize(t.subtypes[0], mod));
  case LLVMType::Vector: return t.bit_width * 4;
  case LLVMType::Function: return 0;
  }
  return 4;
}

std::string DXILToMSL::emitValue(uint32_t idx) {
  if (idx == 0xFFFFFFFF) return "undef";
  return "v" + std::to_string(idx);
}

std::string DXILToMSL::emitConstant(const std::vector<uint64_t> &ops, uint32_t type_id, const LLVMModule &mod) {
  if (type_id >= mod.types.size())
    return "0";
  auto &t = mod.types[type_id];
  if (ops.empty())
    return "0";
  switch (t.kind) {
  case LLVMType::Integer:
    if (t.bit_width == 1) return ops[0] ? "true" : "false";
    if (t.bit_width <= 32) return std::to_string((int32_t)ops[0]);
    return std::to_string((int64_t)ops[0]);
  case LLVMType::Float: {
    float f;
    uint32_t u = (uint32_t)ops[0];
    memcpy(&f, &u, 4);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.9g", (double)f);
    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E'))
      strcat(buf, ".0");
    return std::string(buf) + "f";
  }
  case LLVMType::Double: {
    double d;
    uint64_t u = ops[0];
    memcpy(&d, &u, 8);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", d);
    return std::string(buf);
  }
  default:
    return "0";
  }
}

void DXILToMSL::emitBindings(EmitContext &ctx) {
  auto &os = ctx.os;

  if (ctx.shader.kind == DxilShaderKind::Compute) {
    os << "  uint3 dtid [[thread_position_in_grid]];\n";
    os << "  uint3 gtid [[thread_position_in_threadgroup]];\n";
    os << "  uint3 ggid [[threadgroup_position_in_grid]];\n";
    os << "  uint3 gsz [[threads_per_threadgroup]];\n";
    ctx.uses_thread_id = true;
    ctx.uses_group_id = true;
    ctx.uses_group_thread_id = true;
    ctx.uses_group_size = true;
  }

  os << "\n";
}

void DXILToMSL::emitFunctionPrologue(EmitContext &ctx) {
  auto &os = ctx.os;
  os << kMetalHeader;

  os << "struct input_v {\n";
  os << "  float4 position [[position]];\n";
  os << "  float4 v0 [[user(locn0)]]; float4 v1 [[user(locn1)]];\n";
  os << "  float4 v2 [[user(locn2)]]; float4 v3 [[user(locn3)]];\n";
  os << "  float4 v4 [[user(locn4)]]; float4 v5 [[user(locn5)]];\n";
  os << "  float4 v6 [[user(locn6)]]; float4 v7 [[user(locn7)]];\n";
  os << "  float2 uv0 [[user(locn8)]]; float2 uv1 [[user(locn9)]];\n";
  os << "  float2 uv2 [[user(locn10)]]; float2 uv3 [[user(locn11)]];\n";
  os << "  float4 color0 [[user(locn12)]]; float4 color1 [[user(locn13)]];\n";
  os << "  float4 color2 [[user(locn14)]]; float4 color3 [[user(locn15)]];\n";
  os << "};\n\n";

  os << "struct output_v {\n";
  os << "  float4 position [[position]];\n";
  os << "  float4 v0 [[user(locn0)]]; float4 v1 [[user(locn1)]];\n";
  os << "  float4 v2 [[user(locn2)]]; float4 v3 [[user(locn3)]];\n";
  os << "  float4 v4 [[user(locn4)]]; float4 v5 [[user(locn5)]];\n";
  os << "  float4 v6 [[user(locn6)]]; float4 v7 [[user(locn7)]];\n";
  os << "  float2 uv0 [[user(locn8)]]; float2 uv1 [[user(locn9)]];\n";
  os << "  float2 uv2 [[user(locn10)]]; float2 uv3 [[user(locn11)]];\n";
  os << "  float4 color0 [[user(locn12)]]; float4 color1 [[user(locn13)]];\n";
  os << "  float4 color2 [[user(locn14)]]; float4 color3 [[user(locn15)]];\n";
  os << "};\n\n";

  if (ctx.shader.kind == DxilShaderKind::Compute) {
    os << "kernel void cs_main(\n";
    os << "  device char* buf0 [[buffer(0)]],\n";
    os << "  device char* buf1 [[buffer(1)]],\n";
    os << "  device char* buf2 [[buffer(2)]],\n";
    os << "  device char* buf3 [[buffer(3)]],\n";
    os << "  device char* buf4 [[buffer(4)]],\n";
    os << "  device char* buf5 [[buffer(5)]],\n";
    os << "  device char* buf6 [[buffer(6)]],\n";
    os << "  device char* buf7 [[buffer(7)]],\n";
    os << "  texture2d<float, access::read_write> tex0 [[texture(0)]],\n";
    os << "  texture2d<float, access::read_write> tex1 [[texture(1)]],\n";
    os << "  texture2d<float, access::read_write> tex2 [[texture(2)]],\n";
    os << "  texture2d<float, access::read_write> tex3 [[texture(3)]],\n";
    os << "  texture2d<float, access::read_write> tex4 [[texture(4)]],\n";
    os << "  texture2d<float, access::read_write> tex5 [[texture(5)]],\n";
    os << "  texture2d<float, access::read_write> tex6 [[texture(6)]],\n";
    os << "  texture2d<float, access::read_write> tex7 [[texture(7)]],\n";
    os << "  sampler samp0 [[sampler(0)]],\n";
    os << "  sampler samp1 [[sampler(1)]],\n";
    os << "  sampler samp2 [[sampler(2)]],\n";
    os << "  sampler samp3 [[sampler(3)]],\n";
    os << "  uint3 dtid [[thread_position_in_grid]],\n";
    os << "  uint3 gtid [[thread_position_in_threadgroup]],\n";
    os << "  uint3 ggid [[threadgroup_position_in_grid]],\n";
    os << "  uint3 gsz [[threads_per_threadgroup]]\n";
    os << ") {\n";
  } else if (ctx.shader.kind == DxilShaderKind::Vertex) {
    os << "vertex output_v vs_main(\n";
    os << "  uint vid [[vertex_id]],\n";
    os << "  device char* buf0 [[buffer(0)]],\n";
    os << "  device char* buf1 [[buffer(1)]],\n";
    os << "  device char* buf2 [[buffer(2)]],\n";
    os << "  device char* buf3 [[buffer(3)]],\n";
    os << "  device char* buf4 [[buffer(4)]],\n";
    os << "  device char* buf5 [[buffer(5)]],\n";
    os << "  device char* buf6 [[buffer(6)]],\n";
    os << "  device char* buf7 [[buffer(7)]]\n";
    os << ") {\n";
    os << "  output_v out = {};\n";
  } else if (ctx.shader.kind == DxilShaderKind::Pixel) {
    os << "fragment float4 ps_main(\n";
    os << "  input_v in [[stage_in]],\n";
    os << "  device char* buf0 [[buffer(0)]],\n";
    os << "  device char* buf1 [[buffer(1)]],\n";
    os << "  device char* buf2 [[buffer(2)]],\n";
    os << "  device char* buf3 [[buffer(3)]],\n";
    os << "  device char* buf4 [[buffer(4)]],\n";
    os << "  device char* buf5 [[buffer(5)]],\n";
    os << "  device char* buf6 [[buffer(6)]],\n";
    os << "  device char* buf7 [[buffer(7)]],\n";
    os << "  texture2d<float, access::sample> tex0 [[texture(0)]],\n";
    os << "  texture2d<float, access::sample> tex1 [[texture(1)]],\n";
    os << "  texture2d<float, access::sample> tex2 [[texture(2)]],\n";
    os << "  texture2d<float, access::sample> tex3 [[texture(3)]],\n";
    os << "  texture2d<float, access::sample> tex4 [[texture(4)]],\n";
    os << "  texture2d<float, access::sample> tex5 [[texture(5)]],\n";
    os << "  texture2d<float, access::sample> tex6 [[texture(6)]],\n";
    os << "  texture2d<float, access::sample> tex7 [[texture(7)]],\n";
    os << "  sampler samp0 [[sampler(0)]],\n";
    os << "  sampler samp1 [[sampler(1)]],\n";
    os << "  sampler samp2 [[sampler(2)]],\n";
    os << "  sampler samp3 [[sampler(3)]]\n";
    os << ") {\n";
    os << "  float4 result = float4(0,0,0,1);\n";
  } else {
    os << "kernel void unknown_main() {\n";
  }
}

std::string DXILToMSL::translateDXIntrinsic(EmitContext &ctx, uint32_t intrinsic_id,
                                              const std::vector<uint32_t> &args) {
  auto valueArg = [&](size_t arg, const char *fallback) -> std::string {
    if (arg >= args.size())
      return fallback;
    uint32_t idx = args[arg];
    if (idx < ctx.value_table.size() && !ctx.value_table[idx].empty())
      return ctx.value_table[idx];
    return fallback;
  };

  auto literalArg = [&](size_t arg, uint32_t fallback, const char *label) -> uint32_t {
    std::string text = valueArg(arg, "");
    uint32_t value = 0;
    if (parseUnsignedLiteral(text, value))
      return value;
    DXTRACE("DXIL intrinsic %u: %s is not a literal: %s",
            intrinsic_id, label, text.empty() ? "<missing>" : text.c_str());
    return fallback;
  };

  switch (intrinsic_id) {
  case DXOP_CreateHandle: {
    if (args.size() < 4) return "0";
    uint32_t resource_class = literalArg(0, 0, "resource class");
    uint32_t range_id = literalArg(1, 0, "range id");
    uint32_t index = literalArg(2, 0, "resource index");
    bool non_uniform = literalArg(3, 0, "non-uniform index") != 0;
    (void)non_uniform;
    ctx.next_binding++;
    std::string res_name;
    if (resource_class == 0) {
      res_name = "srv" + std::to_string(range_id);
    } else if (resource_class == 1) {
      res_name = "uav" + std::to_string(range_id);
    } else if (resource_class == 2) {
      res_name = "cbuf" + std::to_string(range_id);
    } else if (resource_class == 3) {
      res_name = "samp" + std::to_string(range_id);
    } else {
      res_name = "buf" + std::to_string(range_id);
    }
    DXTRACE("DXIL CreateHandle: class=%u range=%u index=%u -> %s", resource_class, range_id, index, res_name.c_str());
    return res_name;
  }

  case DXOP_ThreadId: {
    ctx.uses_thread_id = true;
    if (!args.empty()) {
      uint32_t component = literalArg(0, 0, "thread id component");
      if (component == 0) return "(int)dtid.x";
      if (component == 1) return "(int)dtid.y";
      if (component == 2) return "(int)dtid.z";
    }
    return "(int)dtid.x";
  }

  case DXOP_GroupId: {
    ctx.uses_group_id = true;
    if (!args.empty()) {
      uint32_t component = literalArg(0, 0, "group id component");
      if (component == 0) return "(int)ggid.x";
      if (component == 1) return "(int)ggid.y";
      if (component == 2) return "(int)ggid.z";
    }
    return "(int)ggid.x";
  }

  case DXOP_ThreadIDInGroup: {
    ctx.uses_group_thread_id = true;
    if (!args.empty()) {
      uint32_t component = literalArg(0, 0, "group thread id component");
      if (component == 0) return "(int)gtid.x";
      if (component == 1) return "(int)gtid.y";
      if (component == 2) return "(int)gtid.z";
    }
    return "(int)gtid.x";
  }

  case DXOP_CBufferLoadLegacy: {
    if (args.size() < 2) return "float4(0)";
    auto handle = resolveBindingName(valueArg(0, "cbuf0"), "buf");
    auto reg_idx = valueArg(1, "0");
    return "(reinterpret_cast<device float4&>(" + handle + "[(" + reg_idx + ")*64]))";
  }

  case DXOP_BufferLoad: {
    if (args.size() < 3) return "float4(0)";
    auto handle = resolveBindingName(valueArg(0, "srv0"), "buf");
    auto index = valueArg(1, "0");
    return "(reinterpret_cast<device float4&>(" + handle + "[(" + index + ")*16]))";
  }

  case DXOP_TextureLoad: {
    if (args.size() < 3) return "float4(0)";
    auto handle = resolveBindingName(valueArg(0, "srv0"), "tex");
    auto coord_x = valueArg(2, "0");
    auto coord_y = valueArg(3, "0");
    auto coord = "uint2(" + coord_x + ", " + coord_y + ")";
    return handle + ".read(" + coord + ")";
  }

  case DXOP_TextureSample: {
    if (args.size() < 4) return "float4(0)";
    auto handle = resolveBindingName(valueArg(0, "srv0"), "tex");
    auto sampler = resolveBindingName(valueArg(1, "samp0"), "samp");
    auto coord_x = valueArg(2, "0.0");
    auto coord_y = valueArg(3, "0.0");
    auto coord = "float2(" + coord_x + ", " + coord_y + ")";
    return handle + ".sample(" + sampler + ", " + coord + ")";
  }

  case DXOP_Barrier: {
    return "threadgroup_barrier(mem_flags::mem_threadgroup)";
  }

  case DXOP_Unary: {
    if (args.size() < 2) return "0";
    uint32_t op = literalArg(0, 0xFFFFFFFFu, "unary opcode");
    auto x = valueArg(1, "0.0");
    switch (op) {
    case DXILOP_FAbs: return "abs(" + x + ")";
    case DXILOP_Saturate: return "clamp(" + x + ", 0.0, 1.0)";
    case DXILOP_IsNaN: return "isnan(" + x + ")";
    case DXILOP_IsInf: return "isinf(" + x + ")";
    case DXILOP_IsFinite: return "isfinite(" + x + ")";
    case DXILOP_Cos: return "cos(" + x + ")";
    case DXILOP_Sin: return "sin(" + x + ")";
    case DXILOP_Tan: return "tan(" + x + ")";
    case DXILOP_Acos: return "acos(" + x + ")";
    case DXILOP_Asin: return "asin(" + x + ")";
    case DXILOP_Atan: return "atan(" + x + ")";
    case DXILOP_Exp: return "exp2(" + x + ")";
    case DXILOP_Frc: return "fract(" + x + ")";
    case DXILOP_Log: return "log2(" + x + ")";
    case DXILOP_Sqrt: return "sqrt(" + x + ")";
    case DXILOP_Rsqrt: return "rsqrt(" + x + ")";
    case DXILOP_Round_ne: return "rint(" + x + ")";
    case DXILOP_Round_ni: return "floor(" + x + ")";
    case DXILOP_Round_pi: return "ceil(" + x + ")";
    case DXILOP_Round_z: return "trunc(" + x + ")";
    default:
      ctx.unsupported_intrinsics++;
      DXTRACE("DXIL unknown unary opcode: %u", op);
      return x;
    }
  }

  case DXOP_Binary: {
    if (args.size() < 3) return "0";
    uint32_t op = literalArg(0, 0xFFFFFFFFu, "binary opcode");
    auto a = valueArg(1, "0");
    auto b = valueArg(2, "0");
    switch (op) {
    case DXILOP_FMax:
    case DXILOP_IMax: return "max(" + a + ", " + b + ")";
    case DXILOP_FMin:
    case DXILOP_IMin: return "min(" + a + ", " + b + ")";
    case DXILOP_UMax: return "max((uint)(" + a + "), (uint)(" + b + "))";
    case DXILOP_UMin: return "min((uint)(" + a + "), (uint)(" + b + "))";
    default:
      ctx.unsupported_intrinsics++;
      DXTRACE("DXIL unknown binary opcode: %u", op);
      return a;
    }
  }

  case DXOP_Tertiary: {
    if (args.size() < 4) return "0";
    uint32_t op = literalArg(0, 0xFFFFFFFFu, "tertiary opcode");
    auto a = valueArg(1, "0");
    auto b = valueArg(2, "0");
    auto c = valueArg(3, "0");
    switch (op) {
    case DXILOP_FMad:
    case DXILOP_Fma: return "fma(" + a + ", " + b + ", " + c + ")";
    case DXILOP_IMad:
    case DXILOP_UMad: return "((" + a + ") * (" + b + ") + (" + c + "))";
    default:
      ctx.unsupported_intrinsics++;
      DXTRACE("DXIL unknown tertiary opcode: %u", op);
      return a;
    }
  }

  case DXOP_Dot2: {
    if (args.size() < 4) return "0.0";
    auto ax = valueArg(0, "0.0");
    auto ay = valueArg(1, "0.0");
    auto bx = valueArg(2, "0.0");
    auto by = valueArg(3, "0.0");
    return "((" + ax + ")*(" + bx + ") + (" + ay + ")*(" + by + "))";
  }

  case DXOP_Dot3: {
    if (args.size() < 6) return "0.0";
    auto ax = valueArg(0, "0.0");
    auto ay = valueArg(1, "0.0");
    auto az = valueArg(2, "0.0");
    auto bx = valueArg(3, "0.0");
    auto by = valueArg(4, "0.0");
    auto bz = valueArg(5, "0.0");
    return "((" + ax + ")*(" + bx + ") + (" + ay + ")*(" + by + ") + (" + az + ")*(" + bz + "))";
  }

  case DXOP_Dot4: {
    if (args.size() < 8) return "0.0";
    auto ax = valueArg(0, "0.0");
    auto ay = valueArg(1, "0.0");
    auto az = valueArg(2, "0.0");
    auto aw = valueArg(3, "0.0");
    auto bx = valueArg(4, "0.0");
    auto by = valueArg(5, "0.0");
    auto bz = valueArg(6, "0.0");
    auto bw = valueArg(7, "0.0");
    return "((" + ax + ")*(" + bx + ") + (" + ay + ")*(" + by + ") + (" + az + ")*(" + bz + ") + (" + aw + ")*(" + bw + "))";
  }

  case DXOP_LoadInput: {
    if (args.size() < 3) return "0.0";
    uint32_t input_id = literalArg(0, 0, "input id");
    uint32_t component = literalArg(2, 0, "input component");
    if (ctx.shader.kind == DxilShaderKind::Pixel) {
      return varyingField("in", input_id) + componentSuffix(component);
    }
    DXTRACE("DXIL LoadInput fallback: shader_kind=%u input_id=%u component=%u",
            (uint32_t)ctx.shader.kind, input_id, component);
    return "0.0";
  }

  case DXOP_StoreOutput: {
    if (args.size() < 4) return "";
    uint32_t output_id = literalArg(0, 0, "output id");
    uint32_t component = literalArg(2, 0, "output component");
    auto val = valueArg(3, "float4(0)");

    if (ctx.shader.kind == DxilShaderKind::Vertex) {
      return varyingField("out", output_id) + componentSuffix(component) + " = " + val;
    }
    if (ctx.shader.kind == DxilShaderKind::Pixel) {
      if (output_id > 0) {
        DXTRACE("DXIL StoreOutput MRT fallback: output_id=%u component=%u", output_id, component);
      }
      return std::string("result") + componentSuffix(component) + " = " + val;
    }
    DXTRACE("DXIL StoreOutput fallback: shader_kind=%u output_id=%u component=%u",
            (uint32_t)ctx.shader.kind, output_id, component);
    return "";
  }

  default:
    ctx.unsupported_intrinsics++;
    DXTRACE("DXIL unknown intrinsic: %u", intrinsic_id);
    break;
  }

  return "0 /* unknown dx intrinsic " + std::to_string(intrinsic_id) + " */";
}

void DXILToMSL::emitInstruction(EmitContext &ctx, const LLVMInstruction &inst, uint32_t &value_counter) {
  auto &os = ctx.os;
  std::string result = emitValue(value_counter);

  auto getValue = [&](uint32_t idx) -> std::string {
    if (idx < ctx.value_table.size() && !ctx.value_table[idx].empty())
      return ctx.value_table[idx];
    return emitValue(idx);
  };

  auto ensureValueTable = [&](uint32_t needed) {
    if (ctx.value_table.size() <= needed)
      ctx.value_table.resize(needed + 1);
  };

  switch (inst.opcode) {
  case LLVMInstruction::Ret:
    if (ctx.shader.kind == DxilShaderKind::Vertex) {
      os << "  return out;\n";
    } else if (ctx.shader.kind == DxilShaderKind::Pixel) {
      os << "  return result;\n";
    } else {
      os << "  return;\n";
    }
    break;

  case LLVMInstruction::Call: {
    if (inst.operands.empty())
      break;
    uint32_t callee = inst.operands[0];

    std::vector<uint32_t> call_args;
    for (size_t i = 2; i < inst.operands.size(); i++)
      call_args.push_back(inst.operands[i]);

    uint32_t intrinsic_id = 0;
    bool has_intrinsic_literal = false;
    if (call_args.size() > 0) {
      std::string id_str = getValue(call_args[0]);
      has_intrinsic_literal = parseUnsignedLiteral(id_str, intrinsic_id);
    }

    bool named_dxop = callee < ctx.value_table.size() && ctx.value_table[callee].substr(0, 5) == "dx.op";
    if (named_dxop && !has_intrinsic_literal) {
      ctx.unsupported_intrinsics++;
      std::string id_str = call_args.empty() ? "<missing>" : getValue(call_args[0]);
      DXTRACE("DXIL intrinsic id is not a literal: %s", id_str.c_str());
      os << "  // dx.op call without literal intrinsic id\n";
      ensureValueTable(value_counter);
      ctx.value_table[value_counter] = result;
    } else if (has_intrinsic_literal && (named_dxop || isKnownDXIntrinsic(intrinsic_id))) {
      std::vector<uint32_t> remaining_args(call_args.begin() + 1, call_args.end());

      std::string translated = translateDXIntrinsic(ctx, intrinsic_id, remaining_args);

      if (inst.type_id == 0) {
        if (!translated.empty())
          os << "  " << translated << ";\n";
      } else if (translated.find('=') == std::string::npos) {
        ensureValueTable(value_counter);
        if (!translated.empty() && translated[0] != ' ') {
          os << "  auto " << result << " = " << translated << ";\n";
          ctx.value_table[value_counter] = result;
        } else if (!translated.empty()) {
          os << "  " << translated << ";\n";
        }
      } else {
        os << "  " << translated << ";\n";
      }
    } else {
      os << "  // call " << getValue(callee) << "(";
      for (size_t i = 0; i < call_args.size(); i++) {
        if (i) os << ", ";
        os << getValue(call_args[i]);
      }
      os << ")\n";
      ensureValueTable(value_counter);
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::Add: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " + " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Sub: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " - " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Mul: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " * " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::UDiv: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = (" << getValue(inst.operands[0]) << ") / (" << getValue(inst.operands[1]) << ");\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::SDiv: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = (" << getValue(inst.operands[0]) << ") / (" << getValue(inst.operands[1]) << ");\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FAdd: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " + " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FSub: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " - " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FMul: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " * " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FDiv: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " / " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FRem: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = fmod(" << getValue(inst.operands[0]) << ", " << getValue(inst.operands[1]) << ");\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::And: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " & " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Or: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " | " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Xor: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " ^ " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Shl: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " << " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::LShr: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = (uint)(" << getValue(inst.operands[0]) << ") >> " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::AShr: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = (int)(" << getValue(inst.operands[0]) << ") >> " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::BitCast: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 1) {
      os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // bitcast\n";
    }
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::ZExt: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 1) {
      os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // zext\n";
    }
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::SExt: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 1) {
      os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // sext\n";
    }
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Trunc: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 1) {
      os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // trunc\n";
    }
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FPToUI: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // fptoui\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FPToSI: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // fptosi\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::UIToFP: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // uitofp\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::SIToFP: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // sitofp\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FPTrunc: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // fptrunc\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::FPExt: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << "; // fpext\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::PtrToInt: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = reinterpret_cast<uintptr_t>(" << getValue(inst.operands[0]) << ");\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::IntToPtr: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = reinterpret_cast<device char*>(static_cast<uintptr_t>(" << getValue(inst.operands[0]) << "));\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::ICmp: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 3) {
      auto pred = inst.operands[0];
      auto lhs = getValue(inst.operands[1]);
      auto rhs = getValue(inst.operands[2]);
      std::string op;
      switch (pred) {
      case 32: op = "=="; break;
      case 33: op = "!="; break;
      case 34: op = ">"; break;
      case 35: op = ">="; break;
      case 36: op = "<"; break;
      case 37: op = "<="; break;
      default: op = "=="; break;
      }
      os << "  bool " << result << " = " << lhs << " " << op << " " << rhs << ";\n";
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::FCmp: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 3) {
      auto pred = inst.operands[0];
      auto lhs = getValue(inst.operands[1]);
      auto rhs = getValue(inst.operands[2]);
      std::string op;
      switch (pred) {
      case 0: os << "  bool " << result << " = false;\n"; break;
      case 1: os << "  bool " << result << " = true;\n"; break;
      case 2: os << "  bool " << result << " = isunordered(" << lhs << ", " << rhs << ");\n"; break;
      case 3: os << "  bool " << result << " = (" << lhs << " == " << rhs << ");\n"; break;
      case 4: os << "  bool " << result << " = (" << lhs << " != " << rhs << ");\n"; break;
      case 5: os << "  bool " << result << " = (" << lhs << " > " << rhs << ");\n"; break;
      case 6: os << "  bool " << result << " = (" << lhs << " >= " << rhs << ");\n"; break;
      case 7: os << "  bool " << result << " = (" << lhs << " < " << rhs << ");\n"; break;
      case 8: os << "  bool " << result << " = (" << lhs << " <= " << rhs << ");\n"; break;
      default: os << "  bool " << result << " = false;\n"; break;
      }
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::Select: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 3) {
      os << "  auto " << result << " = " << getValue(inst.operands[0]) << " ? " << getValue(inst.operands[1]) << " : " << getValue(inst.operands[2]) << ";\n";
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::Load: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 1) {
      os << "  auto " << result << " = 0; // load from " << getValue(inst.operands[0]) << "\n";
    }
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Store: {
    if (inst.operands.size() >= 2) {
      os << "  reinterpret_cast<device decltype(" << getValue(inst.operands[1]) << ")&>(" << getValue(inst.operands[0]) << ") = " << getValue(inst.operands[1]) << ";\n";
    }
    break;
  }

  case LLVMInstruction::GEP:
  case LLVMInstruction::GetElementPtr: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 2) {
      auto base = getValue(inst.operands[0]);
      std::string offset = "0";
      if (inst.operands.size() >= 2)
        offset = getValue(inst.operands[1]);
      for (size_t i = 2; i < inst.operands.size(); i++) {
        offset = "(" + offset + " + " + getValue(inst.operands[i]) + ")";
      }
      os << "  device char* " << result << " = (" << base << " + (" << offset << "));\n";
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::Alloca: {
    ensureValueTable(value_counter);
    os << "  thread char* " << result << " = (thread char*)alloca(256);\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::PHI: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = 0; // phi\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Br: {
    if (inst.operands.size() == 1) {
      // unconditional branch
    } else if (inst.operands.size() >= 3) {
      auto cond = getValue(inst.operands[0]);
      os << "  if (" << cond << ") {\n  // br true\n  } else {\n  // br false\n  }\n";
    }
    break;
  }

  case LLVMInstruction::Switch: {
    os << "  // switch\n";
    break;
  }

  case LLVMInstruction::ExtractValue: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 2) {
      auto agg = getValue(inst.operands[0]);
      auto idx = inst.operands[1];
      os << "  auto " << result << " = (" << agg << "); // extractvalue idx=" << idx << "\n";
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::InsertValue: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << (inst.operands.size() >= 1 ? getValue(inst.operands[0]) : "0") << "; // insertvalue\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::ExtractElement: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 2) {
      os << "  auto " << result << " = " << getValue(inst.operands[0]) << "[" << getValue(inst.operands[1]) << "];\n";
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::InsertElement: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << (inst.operands.size() >= 1 ? getValue(inst.operands[0]) : "float4(0)") << "; // insertelement\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::ShuffleVector: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << (inst.operands.size() >= 1 ? getValue(inst.operands[0]) : "float4(0)") << "; // shufflevector\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Unreachable:
    os << "  // unreachable\n";
    break;

  case LLVMInstruction::FNeg: {
    ensureValueTable(value_counter);
    if (inst.operands.size() >= 1) {
      os << "  auto " << result << " = -(" << getValue(inst.operands[0]) << ");\n";
      ctx.value_table[value_counter] = result;
    }
    value_counter++;
    break;
  }

  case LLVMInstruction::URem:
  case LLVMInstruction::SRem: {
    ensureValueTable(value_counter);
    os << "  auto " << result << " = " << getValue(inst.operands[0]) << " % " << getValue(inst.operands[1]) << ";\n";
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }

  case LLVMInstruction::Invoke: {
    os << "  // invoke\n";
    break;
  }

  default:
    ctx.unsupported_opcodes++;
    DXTRACE("DXIL unhandled opcode: %d type=%u operands=%zu", (int)inst.opcode,
            inst.type_id, inst.operands.size());
    os << "  // unhandled opcode " << (int)inst.opcode << "\n";
    ensureValueTable(value_counter);
    ctx.value_table[value_counter] = result;
    value_counter++;
    break;
  }
}

std::optional<MSLShader> DXILToMSL::convert(const LLVMModule &module,
                                              const DxilParsedShader &shader) {
  DXTRACE("DXILToMSL::convert: kind=%u sm=%u.%u functions=%zu types=%zu",
          (uint32_t)shader.kind, shader.shader_model.major, shader.shader_model.minor,
          module.functions.size(), module.types.size());

  std::ostringstream os;
  EmitContext ctx{os, module, shader, {}, {}, 0, false, false, false, false};

  emitFunctionPrologue(ctx);

  ctx.value_table.resize(256);

  if (!module.functions.empty()) {
    for (size_t i = 0; i < module.constants.size(); i++) {
      uint32_t val_idx = (uint32_t)i;
      if (val_idx < ctx.value_table.size()) {
        ctx.value_table[val_idx] = module.constants[i].constant_data.empty()
          ? "const_" + std::to_string(i)
          : module.constants[i].constant_data;
      }
    }

    auto &fn = module.functions.back();
    DXTRACE("DXILToMSL: entry function has %zu blocks", fn.blocks.size());

    uint32_t value_counter = (uint32_t)module.constants.size();

    for (auto &block : fn.blocks) {
      for (auto &inst : block.instructions) {
        emitInstruction(ctx, inst, value_counter);
      }
    }
  } else {
    os << "  // No functions parsed from DXIL bitcode\n";
    DXTRACE("DXILToMSL: no functions in module");
  }

  os << "}\n";

  MSLShader result;
  result.source = os.str();
  result.entry_point = shader.entry_point;
  result.tg_size[0] = 1;
  result.tg_size[1] = 1;
  result.tg_size[2] = 1;

  DXTRACE("DXILToMSL: generated %zu bytes of MSL unsupported_intrinsics=%u unsupported_opcodes=%u",
          result.source.size(), ctx.unsupported_intrinsics, ctx.unsupported_opcodes);

  return result;
}

}

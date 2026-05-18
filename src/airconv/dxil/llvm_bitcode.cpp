#include "llvm_bitcode.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <bitset>

#define DXTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxil_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt::dxil {

class BitstreamReader {
public:
  BitstreamReader(const uint8_t *data, uint32_t size)
    : m_data(data), m_size(size), m_offset(0), m_cur_byte(0), m_bits_left(0) {}

  uint32_t read(uint32_t num_bits) {
    uint32_t result = 0;
    uint32_t bits_read = 0;
    while (bits_read < num_bits) {
      if (m_bits_left == 0) {
        if (m_offset >= m_size) return 0;
        m_cur_byte = m_data[m_offset++];
        m_bits_left = 8;
      }
      uint32_t to_read = std::min(num_bits - bits_read, m_bits_left);
      result |= (uint32_t)(m_cur_byte & ((1 << to_read) - 1)) << bits_read;
      m_cur_byte >>= to_read;
      m_bits_left -= to_read;
      bits_read += to_read;
    }
    return result;
  }

  uint64_t read64(uint32_t num_bits) {
    if (num_bits <= 32) return read(num_bits);
    uint64_t lo = read(32);
    uint64_t hi = read(num_bits - 32);
    return lo | (hi << 32);
  }

  uint32_t readVBR(uint32_t width) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint32_t chunk;
    do {
      chunk = read(width);
      result |= (chunk & ((1u << (width - 1)) - 1)) << shift;
      shift += width - 1;
    } while (chunk & (1u << (width - 1)));
    return result;
  }

  uint64_t readVBR64(uint32_t width) {
    uint64_t result = 0;
    uint64_t shift = 0;
    uint32_t chunk;
    do {
      chunk = read(width);
      result |= (uint64_t)(chunk & ((1u << (width - 1)) - 1)) << shift;
      shift += width - 1;
    } while (chunk & (1u << (width - 1)));
    return result;
  }

  void align32() {
    while (m_offset % 4 != 0 && m_offset < m_size)
      m_offset++;
    m_bits_left = 0;
  }

  uint32_t tell() const { return m_offset * 8 - m_bits_left; }
  void seek(uint32_t bit_pos) {
    m_offset = bit_pos / 8;
    m_bits_left = 0;
    uint32_t skip = bit_pos % 8;
    if (skip) read(skip);
  }

  bool atEnd() const { return m_offset >= m_size && m_bits_left == 0; }

private:
  const uint8_t *m_data;
  uint32_t m_size;
  uint32_t m_offset;
  uint8_t m_cur_byte;
  uint32_t m_bits_left;
};

static constexpr uint32_t kEnterSubBlock = 1;
static constexpr uint32_t kEndBlock = 0;
static constexpr uint32_t kDefineAbbrev = 2;
static constexpr uint32_t kUnabbrevRecord = 3;

static constexpr uint32_t kBlockID_Module = 8;
static constexpr uint32_t kBlockID_BlockInfo = 0;
static constexpr uint32_t kBlockID_ValueSymTab = 14;
static constexpr uint32_t kBlockID_Function = 12;
static constexpr uint32_t kBlockID_Type = 17;
static constexpr uint32_t kBlockID_Constants = 11;

static constexpr uint32_t kTypeCode_Void = 2;
static constexpr uint32_t kTypeCode_Float = 3;
static constexpr uint32_t kTypeCode_Double = 4;
static constexpr uint32_t kTypeCode_Integer = 7;
static constexpr uint32_t kTypeCode_Pointer = 8;
static constexpr uint32_t kTypeCode_Struct = 10;
static constexpr uint32_t kTypeCode_Array = 11;
static constexpr uint32_t kTypeCode_Vector = 12;
static constexpr uint32_t kTypeCode_Function = 9;

static constexpr uint32_t kModuleCode_Function = 8;
static constexpr uint32_t kModuleCode_GlobalVar = 7;
static constexpr uint32_t kModuleCode_VSTOffset = 19;

static constexpr uint32_t kFuncCode_DeclareBlocks = 1;
static constexpr uint32_t kFuncCode_InstRet = 10;
static constexpr uint32_t kFuncCode_InstBr = 11;
static constexpr uint32_t kFuncCode_InstCall = 34;
static constexpr uint32_t kFuncCode_InstPHI = 4;
static constexpr uint32_t kFuncCode_InstBinop = 2;
static constexpr uint32_t kFuncCode_InstCast = 3;
static constexpr uint32_t kFuncCode_InstGEP = 26;
static constexpr uint32_t kFuncCode_InstLoad = 20;
static constexpr uint32_t kFuncCode_InstStore = 44;
static constexpr uint32_t kFuncCode_InstExtractVal = 55;
static constexpr uint32_t kFuncCode_InstInsertVal = 56;
static constexpr uint32_t kFuncCode_InstSelect = 17;
static constexpr uint32_t kFuncCode_InstICmp = 45;
static constexpr uint32_t kFuncCode_InstFCmp = 46;
static constexpr uint32_t kFuncCode_InstUnreachable = 15;
static constexpr uint32_t kFuncCode_InstAlloca = 19;
static constexpr uint32_t kFuncCode_InstExtractElt = 57;
static constexpr uint32_t kFuncCode_InstInsertElt = 58;
static constexpr uint32_t kFuncCode_InstShuffleVec = 59;
static constexpr uint32_t kFuncCode_InstSwitch = 12;
static constexpr uint32_t kFuncCode_InstInvoke = 13;

static constexpr uint32_t kConstantsCode_SetType = 1;
static constexpr uint32_t kConstantsCode_Null = 2;
static constexpr uint32_t kConstantsCode_Undefined = 3;
static constexpr uint32_t kConstantsCode_Integer = 4;
static constexpr uint32_t kConstantsCode_Float = 6;
static constexpr uint32_t kConstantsCode_Aggregate = 7;
static constexpr uint32_t kConstantsCode_String = 8;
static constexpr uint32_t kConstantsCode_Cast = 11;
static constexpr uint32_t kConstantsCode_GEP = 12;
static constexpr uint32_t kConstantsCode_Data = 15;

static int64_t decodeSignedVBR(uint64_t value) {
  if ((value & 1) == 0)
    return (int64_t)(value >> 1);
  if (value != 1)
    return -(int64_t)(value >> 1);
  return INT64_MIN;
}

struct Abbrev {
  std::vector<std::pair<uint32_t, uint64_t>> ops;
};

struct BlockInfo {
  uint32_t block_id = 0;
  std::vector<Abbrev> abbrevs;
};

struct ParseContext {
  BitstreamReader &reader;
  LLVMModule &module;
  std::vector<Abbrev> cur_abbrevs;
  std::vector<BlockInfo> block_infos;
};

static std::optional<uint32_t> readBlockHeader(BitstreamReader &r) {
  r.align32();
  uint32_t block_id = r.readVBR(8);
  uint32_t new_abbrev_len = r.readVBR(4);
  uint32_t block_len = r.read(32);
  (void)block_len;
  return new_abbrev_len;
}

static std::vector<uint64_t> readUnabbrevRecord(BitstreamReader &r) {
  uint32_t code = r.readVBR(6);
  uint32_t num_ops = r.readVBR(6);
  std::vector<uint64_t> ops;
  ops.push_back(code);
  for (uint32_t i = 0; i < num_ops; i++) {
    ops.push_back(r.readVBR64(6));
  }
  return ops;
}

static bool parseTypeBlock(ParseContext &ctx) {
  auto abbrev_len = readBlockHeader(ctx.reader);
  if (!abbrev_len) return false;

  while (!ctx.reader.atEnd()) {
    uint32_t code = ctx.reader.read(*abbrev_len);
    if (code == kEndBlock) {
      ctx.reader.align32();
      return true;
    }
    if (code == kEnterSubBlock || code == kDefineAbbrev)
      continue;

    std::vector<uint64_t> ops;
    if (code == kUnabbrevRecord) {
      ops = readUnabbrevRecord(ctx.reader);
    } else {
      continue;
    }

    uint32_t rec_code = (uint32_t)ops[0];
    LLVMType t;
    t.kind = LLVMType::Void;

    switch (rec_code) {
    case kTypeCode_Void:
      t.kind = LLVMType::Void;
      ctx.module.types.push_back(t);
      break;
    case kTypeCode_Float:
      t.kind = LLVMType::Float;
      t.bit_width = 32;
      ctx.module.types.push_back(t);
      break;
    case kTypeCode_Double:
      t.kind = LLVMType::Double;
      t.bit_width = 64;
      ctx.module.types.push_back(t);
      break;
    case kTypeCode_Integer: {
      t.kind = LLVMType::Integer;
      t.bit_width = ops.size() > 1 ? (uint32_t)ops[1] : 32;
      ctx.module.types.push_back(t);
      break;
    }
    case kTypeCode_Pointer: {
      t.kind = LLVMType::Pointer;
      if (ops.size() > 1)
        t.subtypes.push_back({LLVMType::Void, 0, {}});
      ctx.module.types.push_back(t);
      break;
    }
    case kTypeCode_Struct: {
      t.kind = LLVMType::Struct;
      for (size_t i = 1; i < ops.size(); i++)
        t.subtypes.push_back({LLVMType::Void, 0, {}});
      ctx.module.types.push_back(t);
      break;
    }
    case kTypeCode_Array: {
      t.kind = LLVMType::Array;
      t.bit_width = ops.size() > 1 ? (uint32_t)ops[1] : 0;
      ctx.module.types.push_back(t);
      break;
    }
    case kTypeCode_Vector: {
      t.kind = LLVMType::Vector;
      t.bit_width = ops.size() > 1 ? (uint32_t)ops[1] : 0;
      ctx.module.types.push_back(t);
      break;
    }
    case kTypeCode_Function: {
      t.kind = LLVMType::Function;
      if (ops.size() > 1)
        t.subtypes.push_back({LLVMType::Void, 0, {}});
      ctx.module.types.push_back(t);
      break;
    }
    default:
      break;
    }
  }
  return false;
}

static bool parseConstantsBlock(ParseContext &ctx) {
  auto abbrev_len = readBlockHeader(ctx.reader);
  if (!abbrev_len) return false;

  uint32_t cur_type = 0;
  while (!ctx.reader.atEnd()) {
    uint32_t code = ctx.reader.read(*abbrev_len);
    if (code == kEndBlock) {
      ctx.reader.align32();
      return true;
    }
    if (code == kEnterSubBlock || code == kDefineAbbrev)
      continue;

    std::vector<uint64_t> ops;
    if (code == kUnabbrevRecord) {
      ops = readUnabbrevRecord(ctx.reader);
    } else {
      continue;
    }

    uint32_t rec_code = (uint32_t)ops[0];
    switch (rec_code) {
    case kConstantsCode_SetType:
      if (ops.size() > 1) cur_type = (uint32_t)ops[1];
      break;
    case kConstantsCode_Integer:
    case kConstantsCode_Float:
    case kConstantsCode_Null:
    case kConstantsCode_Undefined: {
      LLVMValue v;
      v.kind = LLVMValue::Constant;
      v.type_id = cur_type;
      v.id = (uint32_t)ctx.module.constants.size();
      if (rec_code == kConstantsCode_Integer && ops.size() > 1) {
        v.constant_data = std::to_string(decodeSignedVBR(ops[1]));
      } else if (rec_code == kConstantsCode_Float && ops.size() > 1) {
        if (cur_type < ctx.module.types.size() &&
            ctx.module.types[cur_type].kind == LLVMType::Float) {
          float f;
          uint32_t raw = (uint32_t)ops[1];
          memcpy(&f, &raw, sizeof(f));
          char buf[64];
          snprintf(buf, sizeof(buf), "%.9gf", (double)f);
          v.constant_data = buf;
        } else if (cur_type < ctx.module.types.size() &&
                   ctx.module.types[cur_type].kind == LLVMType::Double) {
          double d;
          uint64_t raw = ops[1];
          memcpy(&d, &raw, sizeof(d));
          char buf[64];
          snprintf(buf, sizeof(buf), "%.17g", d);
          v.constant_data = buf;
        }
      } else if (rec_code == kConstantsCode_Null) {
        v.constant_data = "0";
      } else if (rec_code == kConstantsCode_Undefined) {
        v.constant_data = "0";
      }
      ctx.module.constants.push_back(v);
      break;
    }
    default:
      break;
    }
  }
  return false;
}

static bool parseFunctionBlock(ParseContext &ctx, LLVMFunction &fn) {
  auto abbrev_len = readBlockHeader(ctx.reader);
  if (!abbrev_len) return false;

  uint32_t cur_block = 0;
  uint32_t value_id = 0;

  while (!ctx.reader.atEnd()) {
    uint32_t code = ctx.reader.read(*abbrev_len);
    if (code == kEndBlock) {
      ctx.reader.align32();
      return true;
    }
    if (code == kEnterSubBlock) {
      continue;
    }
    if (code == kDefineAbbrev)
      continue;

    std::vector<uint64_t> ops;
    if (code == kUnabbrevRecord) {
      ops = readUnabbrevRecord(ctx.reader);
    } else {
      continue;
    }

    uint32_t rec_code = (uint32_t)ops[0];

    switch (rec_code) {
    case kFuncCode_DeclareBlocks:
      fn.blocks.resize(ops.size() > 1 ? (size_t)ops[1] : 0);
      cur_block = 0;
      break;
    case kFuncCode_InstRet:
      if (cur_block < fn.blocks.size()) {
        LLVMInstruction inst;
        inst.opcode = LLVMInstruction::Ret;
        fn.blocks[cur_block].instructions.push_back(inst);
      }
      break;
    case kFuncCode_InstCall: {
      if (cur_block < fn.blocks.size()) {
        LLVMInstruction inst;
        inst.opcode = LLVMInstruction::Call;
        if (ops.size() > 1) inst.type_id = (uint32_t)ops[1];
        for (size_t i = 2; i < ops.size(); i++)
          inst.operands.push_back((uint32_t)ops[i]);
        fn.blocks[cur_block].instructions.push_back(inst);
      }
      break;
    }
    case kFuncCode_InstBinop: {
      if (cur_block < fn.blocks.size()) {
        LLVMInstruction inst;
        inst.opcode = LLVMInstruction::Add;
        if (ops.size() > 1) inst.type_id = (uint32_t)ops[1];
        for (size_t i = 2; i < ops.size(); i++)
          inst.operands.push_back((uint32_t)ops[i]);
        fn.blocks[cur_block].instructions.push_back(inst);
      }
      break;
    }
    case kFuncCode_InstCast: {
      if (cur_block < fn.blocks.size()) {
        LLVMInstruction inst;
        inst.opcode = LLVMInstruction::BitCast;
        if (ops.size() > 1) inst.type_id = (uint32_t)ops[1];
        for (size_t i = 2; i < ops.size(); i++)
          inst.operands.push_back((uint32_t)ops[i]);
        fn.blocks[cur_block].instructions.push_back(inst);
      }
      break;
    }
    case kFuncCode_InstGEP: {
      if (cur_block < fn.blocks.size()) {
        LLVMInstruction inst;
        inst.opcode = LLVMInstruction::GetElementPtr;
        for (size_t i = 1; i < ops.size(); i++)
          inst.operands.push_back((uint32_t)ops[i]);
        fn.blocks[cur_block].instructions.push_back(inst);
      }
      break;
    }
    case kFuncCode_InstLoad: {
      if (cur_block < fn.blocks.size()) {
        LLVMInstruction inst;
        inst.opcode = LLVMInstruction::Load;
        if (ops.size() > 1) inst.type_id = (uint32_t)ops[1];
        for (size_t i = 2; i < ops.size(); i++)
          inst.operands.push_back((uint32_t)ops[i]);
        fn.blocks[cur_block].instructions.push_back(inst);
      }
      break;
    }
    case kFuncCode_InstStore: {
      if (cur_block < fn.blocks.size()) {
        LLVMInstruction inst;
        inst.opcode = LLVMInstruction::Store;
        for (size_t i = 1; i < ops.size(); i++)
          inst.operands.push_back((uint32_t)ops[i]);
        fn.blocks[cur_block].instructions.push_back(inst);
      }
      break;
    }
    case kFuncCode_InstExtractVal:
    case kFuncCode_InstInsertVal:
    case kFuncCode_InstSelect:
    case kFuncCode_InstICmp:
    case kFuncCode_InstFCmp:
    case kFuncCode_InstUnreachable:
    case kFuncCode_InstAlloca:
    case kFuncCode_InstExtractElt:
    case kFuncCode_InstInsertElt:
    case kFuncCode_InstShuffleVec:
    case kFuncCode_InstSwitch:
    case kFuncCode_InstInvoke:
    case kFuncCode_InstPHI:
    case kFuncCode_InstBr:
    default:
      break;
    }
  }
  return false;
}

std::optional<LLVMModule> BitcodeReader::parse(const uint8_t *data, uint32_t size) {
  LLVMModule module;

  DXTRACE("BitcodeReader::parse size=%u", size);
  if (size >= 8) {
    DXTRACE("  bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
      data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
  }

  BitstreamReader reader(data, size);

  uint32_t magic = reader.read(32);
  if (magic != 0xDEC04342)
    return std::nullopt;

  // DXIL bitcode: after magic, the bitstream starts directly
  // No wrapper header — seek past the 4-byte magic
  reader.seek(32);

  ParseContext ctx{reader, module, {}, {}};

  uint32_t bc_abbrev = reader.read(2);
  DXTRACE("  bc_abbrev=%u at bit %u", bc_abbrev, reader.tell());
  if (bc_abbrev != kEnterSubBlock)
    return std::nullopt;

  auto abbrev_len = readBlockHeader(reader);
  DXTRACE("  abbrev_len=%u", abbrev_len.value_or(0));
  if (!abbrev_len) return std::nullopt;

  std::vector<uint32_t> pending_fn_types;

  while (!reader.atEnd()) {
    uint32_t code = reader.read(*abbrev_len);
    if (code == kEndBlock) {
      reader.align32();
      break;
    }
    if (code == kDefineAbbrev)
      continue;

    if (code == kEnterSubBlock) {
      uint32_t block_id = reader.readVBR(8);
      uint32_t new_abbrev_len = reader.readVBR(4);
      uint32_t block_len = reader.read(32);

      switch (block_id) {
      case kBlockID_Type: {
        ParseContext type_ctx{reader, module, {}, {}};
        parseTypeBlock(type_ctx);
        break;
      }
      case kBlockID_Constants: {
        ParseContext const_ctx{reader, module, {}, {}};
        parseConstantsBlock(const_ctx);
        break;
      }
      case kBlockID_Function: {
        if (!pending_fn_types.empty()) {
          uint32_t fn_type = pending_fn_types.back();
          pending_fn_types.pop_back();
          LLVMFunction fn;
          fn.type_id = fn_type;
          fn.is_declaration = false;
          ParseContext func_ctx{reader, module, {}, {}};
          parseFunctionBlock(func_ctx, fn);
          module.functions.push_back(fn);
        } else {
          reader.align32();
          reader.seek(reader.tell() + block_len * 8);
        }
        break;
      }
      case kBlockID_ValueSymTab:
      case kBlockID_BlockInfo:
      default:
        reader.align32();
        reader.seek(reader.tell() + block_len * 8);
        break;
      }
      continue;
    }

    if (code == kUnabbrevRecord) {
      auto ops = readUnabbrevRecord(reader);
      uint32_t rec_code = (uint32_t)ops[0];

      if (rec_code == kModuleCode_Function) {
        pending_fn_types.push_back(ops.size() > 1 ? (uint32_t)ops[1] : 0);
      }
    }
  }

  return module;
}

}

#include "dxil_metadata.hpp"

#include "dxil_metadata_reader.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include <iterator>

using namespace llvm;

namespace dxmt::dxil {

uint32_t
read_u32le(const unsigned char *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

Expected<StringRef>
extract_dxil_bitcode(StringRef chunk) {
  const unsigned char *q = reinterpret_cast<const unsigned char *>(chunk.data());
  if (chunk.size() < 24)
    return metadata_error("truncated DXIL program header");
  if (read_u32le(q + 8) != 0x4C495844)
    return metadata_error("bad DXIL program header magic");
  uint32_t bitcode_offset = read_u32le(q + 16);
  uint32_t bitcode_size = read_u32le(q + 20);
  uint32_t start = 8 + bitcode_offset;
  if (start + bitcode_size > chunk.size())
    return metadata_error("DXIL bitcode range out of bounds");
  return chunk.substr(start, bitcode_size);
}

Expected<SignatureElement>
parse_signature_element(MetadataReader r) {
  using Indices = std::vector<uint32_t>;
  auto element =
      r.slice(0, 10)
          .asMappedTuple<
              uint32_t, std::string, uint32_t, uint32_t, Indices, uint32_t, uint32_t, uint32_t, int32_t, int32_t>(
              [](uint32_t id, std::string name, uint32_t component_type, uint32_t semantic_kind, Indices indices,
                 uint32_t interpolation, uint32_t rows, uint32_t cols, int32_t start_row, int32_t start_col) {
                SignatureElement out;
                out.id = id;
                out.name = std::move(name);
                std::transform(out.name.begin(), out.name.end(), out.name.begin(), ::toupper);
                out.component_type = static_cast<ComponentType>(component_type);
                out.semantic_kind = static_cast<SemanticKind>(semantic_kind);
                out.interpolation = static_cast<InterpolationMode>(interpolation);
                out.semantic_indices = std::move(indices);
                out.rows = rows;
                out.cols = cols;
                out.start_row = start_row;
                out.start_col = start_col;
                return out;
              }
          );
  if (!element)
    return element.takeError();

  MetadataReader tags = r.operand(10);
  if (!tags.isNull()) {
    auto pair_count = tags.operandCount();
    if (!pair_count)
      return pair_count.takeError();
    for (unsigned i = 0; i + 1 < *pair_count; i += 2) {
      auto tag = tags.operand(i).asUint();
      if (!tag)
        return tag.takeError();
      auto value = tags.operand(i + 1).asUint();
      if (!value)
        return value.takeError();
      if (static_cast<SignatureElementTag>(*tag) == SignatureElementTag::UsageCompMask)
        element->usage_mask = *value;
    }
  }
  return std::move(*element);
}

Expected<std::vector<SignatureElement>>
parse_signature(MetadataReader r) {
  std::vector<SignatureElement> out;
  if (r.isNull())
    return out;
  auto count = r.operandCount();
  if (!count)
    return count.takeError();
  out.reserve(*count);
  for (unsigned i = 0; i < *count; ++i) {
    auto element = parse_signature_element(r.operand(i));
    if (!element)
      return element.takeError();
    out.push_back(std::move(*element));
  }
  return out;
}

Expected<Resource>
parse_resource(MetadataReader r, ResourceClass resource_class) {
  auto res = r.slice(0, 6).asMappedTuple<uint32_t, Ignore, Ignore, uint32_t, uint32_t, uint32_t>(
      [resource_class](uint32_t id, Ignore, Ignore, uint32_t space, uint32_t lower_bound, uint32_t range_size) {
        Resource out;
        out.resource_class = resource_class;
        out.id = id;
        out.space = space;
        out.lower_bound = lower_bound;
        out.range_size = range_size;
        return out;
      }
  );
  if (!res)
    return res.takeError();

  MetadataReader tags;
  switch (resource_class) {
  case ResourceClass::SRV: {
    auto shape = r.slice(6, 2).asMappedTuple<uint32_t, uint32_t>([](uint32_t kind, uint32_t sample_count) {
      return std::make_pair(static_cast<ResourceKind>(kind), sample_count);
    });
    if (!shape)
      return shape.takeError();
    std::tie(res->kind, res->sample_count) = *shape;
    tags = r.operand(8);
    break;
  }
  case ResourceClass::UAV: {
    auto shape = r.slice(6, 4).asMappedTuple<uint32_t, bool, bool, bool>([](uint32_t kind, bool globally_coherent,
                                                                            bool has_counter, bool rasterizer_ordered) {
      return std::make_tuple(static_cast<ResourceKind>(kind), globally_coherent, has_counter, rasterizer_ordered);
    });
    if (!shape)
      return shape.takeError();
    std::tie(res->kind, res->globally_coherent, res->has_counter, res->rasterizer_ordered) = *shape;
    tags = r.operand(10);
    break;
  }
  case ResourceClass::CBV: {
    res->kind = ResourceKind::CBuffer;
    auto cbuffer_size = r.operand(6).asUint();
    if (!cbuffer_size)
      return cbuffer_size.takeError();
    res->cbuffer_size = *cbuffer_size;
    tags = r.operand(7);
    break;
  }
  case ResourceClass::Sampler: {
    res->kind = ResourceKind::Sampler;
    auto sampler_kind = r.operand(6).asUint();
    if (!sampler_kind)
      return sampler_kind.takeError();
    res->element_type = *sampler_kind;
    tags = r.operand(7);
    break;
  }
  }

  if (!tags.isNull()) {
    auto pair_count = tags.operandCount();
    if (!pair_count)
      return pair_count.takeError();
    for (unsigned i = 0; i + 1 < *pair_count; i += 2) {
      auto tag = tags.operand(i).asUint();
      if (!tag)
        return tag.takeError();
      auto value = tags.operand(i + 1).asUint();
      if (!value)
        return value.takeError();
      switch (static_cast<ResourceTag>(*tag)) {
      case ResourceTag::TypedBufferElementType:
        res->element_type = *value;
        break;
      case ResourceTag::StructuredBufferElementStride:
        res->element_stride = *value;
        break;
      default:
        break;
      }
    }
  }
  return std::move(*res);
}

Expected<std::vector<Resource>>
parse_resource_list(MetadataReader r, ResourceClass resource_class) {
  std::vector<Resource> out;
  if (r.isNull())
    return out;
  auto count = r.operandCount();
  if (!count)
    return count.takeError();
  out.reserve(*count);
  for (unsigned i = 0; i < *count; ++i) {
    auto resource = parse_resource(r.operand(i), resource_class);
    if (!resource)
      return resource.takeError();
    out.push_back(std::move(*resource));
  }
  return out;
}

Expected<std::vector<Resource>>
parse_resource_group(MetadataReader r) {
  std::vector<Resource> out;
  if (r.isNull())
    return out;
  const ResourceClass classes[] = {ResourceClass::SRV, ResourceClass::UAV, ResourceClass::CBV, ResourceClass::Sampler};
  auto count = r.operandCount();
  if (!count)
    return count.takeError();
  for (unsigned i = 0; i < *count && i < 4; ++i) {
    auto list = parse_resource_list(r.operand(i), classes[i]);
    if (!list)
      return list.takeError();
    out.insert(out.end(), std::make_move_iterator(list->begin()), std::make_move_iterator(list->end()));
  }
  return out;
}

Expected<NumThreads>
parse_num_threads(MetadataReader r) {
  auto num_threads = r.slice(0, 3).asMappedTuple<uint32_t, uint32_t, uint32_t>([](uint32_t x, uint32_t y, uint32_t z) {
    return NumThreads{x, y, z};
  });
  if (!num_threads)
    return num_threads.takeError();
  return *num_threads;
}

Expected<Property>
parse_property(EntryPropertyTag tag, MetadataReader r) {
  Property property{};
  property.tag = tag;
  switch (tag) {
  case EntryPropertyTag::ShaderFlags: {
    auto flags = r.asUint64();
    if (!flags)
      return flags.takeError();
    property.shader_flags = *flags;
    break;
  }
  case EntryPropertyTag::NumThreads: {
    auto num_threads = parse_num_threads(std::move(r));
    if (!num_threads)
      return num_threads.takeError();
    property.num_threads = *num_threads;
    break;
  }
  case EntryPropertyTag::GSState: {
    auto state = r.slice(0, 5).asMappedTuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>(
        [](uint32_t primitive, uint32_t max_vertex_count, uint32_t active_stream_mask, uint32_t topology,
           uint32_t instance_count) {
          return GSState{primitive, max_vertex_count, active_stream_mask, topology, instance_count};
        }
    );
    if (!state)
      return state.takeError();
    property.gs_state = *state;
    break;
  }
  default:
    break;
  }
  return property;
}

Expected<EntryPoint> parse_entry_point(MetadataReader r);

Expected<EntryPoint>
parse_entry_point_for(const llvm::Function &function) {
  auto *module = function.getParent();
  if (!module)
    return metadata_error("function is not attached to a module");
  auto *named = module->getNamedMetadata("dx.entryPoints");
  if (!named)
    return metadata_error("dx.entryPoints is missing");
  for (unsigned i = 0; i < named->getNumOperands(); ++i) {
    auto entry = parse_entry_point(MetadataReader::of(named->getOperand(i)));
    if (!entry)
      return entry.takeError();
    if (entry->function == &function)
      return std::move(*entry);
  }
  return metadata_error("function is not an entry point");
}

Expected<EntryPoint>
parse_entry_point(MetadataReader r) {
  r = r.assertOperandCount(5);
  EntryPoint ep;
  auto function = r.operand(0).asFunction();
  if (!function)
    return function.takeError();
  ep.function = *function;

  MetadataReader signatures = r.operand(2);
  if (!signatures.isNull()) {
    auto inputs = parse_signature(signatures.operand(0));
    if (!inputs)
      return inputs.takeError();
    ep.inputs = std::move(*inputs);
    auto outputs = parse_signature(signatures.operand(1));
    if (!outputs)
      return outputs.takeError();
    ep.outputs = std::move(*outputs);
  }

  auto resources = parse_resource_group(r.operand(3));
  if (!resources)
    return resources.takeError();
  ep.resources = std::move(*resources);

  MetadataReader props = r.operand(4);
  if (!props.isNull()) {
    auto prop_count = props.operandCount();
    if (!prop_count)
      return prop_count.takeError();
    for (unsigned i = 0; i + 1 < *prop_count; i += 2) {
      auto tag = props.operand(i).asUint();
      if (!tag)
        return tag.takeError();
      auto property = parse_property(static_cast<EntryPropertyTag>(*tag), props.operand(i + 1));
      if (!property)
        return property.takeError();
      ep.properties.push_back(std::move(*property));
    }
  }
  return ep;
}

Expected<Info>
parse(const Module &mod) {
  Info info;

  auto model = MetadataReader::fromNamed(mod, "dx.shaderModel", 0).asTuple<std::string, uint32_t, uint32_t>();
  if (!model)
    return model.takeError();
  std::tie(info.shader_model.stage, info.shader_model.major, info.shader_model.minor) = *model;

  auto ep_count = MetadataReader::namedOperandCount(mod, "dx.entryPoints");
  if (!ep_count)
    return ep_count.takeError();
  for (unsigned i = 0; i < *ep_count; ++i) {
    auto entry = parse_entry_point(MetadataReader::fromNamed(mod, "dx.entryPoints", i));
    if (!entry)
      return entry.takeError();
    info.entry_points.push_back(std::move(*entry));
  }

  return info;
}

static Expected<ParsedModule>
parse_bitcode(MemoryBufferRef bitcode) {
  auto context = std::make_unique<LLVMContext>();
  SMDiagnostic err;
  std::unique_ptr<Module> mod = parseIR(bitcode, err, *context);
  if (!mod) {
    std::string msg;
    raw_string_ostream os(msg);
    err.print("dxilmeta", os);
    return metadata_error(os.str());
  }

  ParsedModule result;
  auto info = parse(*mod);
  if (!info)
    return info.takeError();
  result.info = std::move(*info);
  result.module = std::move(mod);
  result.context = std::move(context);
  return result;
}

Expected<ParsedModule>
parse_dxil_blob(StringRef blob) {
  auto bitcode = extract_dxil_bitcode(blob);
  if (!bitcode)
    return bitcode.takeError();
  return parse_bitcode(MemoryBufferRef(*bitcode, ""));
}

} // namespace dxmt::dxil

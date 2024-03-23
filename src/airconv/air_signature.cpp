#include "air_signature.hpp"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <variant>

#include "air_signature.hpp"
#include "ftl.hpp"

using namespace llvm;

namespace dxmt::air {

class StreamMDHelper {
public:
  auto string(std::string str) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return MDString::get(context, str);
    });
    return this;
  };
  auto string(const char *c_str) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return MDString::get(context, c_str);
    });
    return this;
  };
  auto integer(uint64_t value, bool is_signed = false) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return ConstantAsMetadata::get(
        ConstantInt::get(context, APInt{32, value, is_signed})
      );
    });
    return this;
  };
  template <typename enumeration>
  auto integer(enumeration value, bool is_signed = false) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return ConstantAsMetadata::get(
        ConstantInt::get(context, APInt{32, (uint32_t)value, is_signed})
      );
    });
    return this;
  };
  auto metadata(llvm::Metadata *metadata) {
    factories.push_back([=](llvm::LLVMContext &context) { return metadata; });
    return this;
  };

  auto BuildTuple(llvm::LLVMContext &context) {
    auto mds = factories | [&](auto factory) -> llvm::Metadata * {
      return factory(context);
    };
    return MDTuple::get(context, mds);
  };

private:
  std::vector<std::function<llvm::Metadata *(llvm::LLVMContext &)>> factories;
};

uint32_t ArgumentBufferBuilder::DefineBuffer(
  std::string name, AddressSpace addressp_space, MemoryAccess access,
  MSLRepresentableType type
) {
  auto element_index = fieldsType.size();
  assert(
    fieldsType.find(name) == fieldsType.end() &&
    "otherwise duplicated field name"
  );
  fieldsType[name] = ArgumentBindingBuffer{
    .location_index = (uint32_t)element_index,
    .array_size = 1,
    .memory_access = access,
    .address_space = addressp_space,
    .type = type,
    .arg_name = name,
  };
  return element_index;
};

// uint32_t ArgumentBufferBuilder::DefineIndirectBuffer(
//   std::string name, AddressSpace addressp_space, llvm::StructType
//   *struct_type, llvm::Metadata *struct_type_metadata
// ) {
//   auto element_index = fieldsType.size();
//   assert(
//     fieldsType.find(name) == fieldsType.end() &&
//     "otherwise duplicated field name"
//   );
//   fieldsType[name] = ArgumentBindingIndirectBuffer {};
//   return element_index;
// }

uint32_t ArgumentBufferBuilder::DefineSampler(std::string name) {
  auto element_index = fieldsType.size();
  assert(
    fieldsType.find(name) == fieldsType.end() &&
    "otherwise duplicated field name"
  );
  fieldsType[name] = ArgumentBindingSampler{
    .location_index = (uint32_t)element_index, .array_size = 1, .arg_name = name
  };
  return element_index;
};

uint32_t ArgumentBufferBuilder::DefineTexture(
  std::string name, TextureKind kind, MemoryAccess access,
  MSLScalerType scaler_type
) {
  auto element_index = fieldsType.size();
  assert(
    fieldsType.find(name) == fieldsType.end() &&
    "otherwise duplicated field name"
  );
  fieldsType[name] = ArgumentBindingTexture{
    .location_index = (uint32_t)element_index,
    .array_size = 1,
    .memory_access = access,
    .type =
      MSLTexture{
        .component_type = scaler_type,
        .memory_access = access,
        .resource_kind = kind
      },
    .arg_name = name
  };
  return element_index;
};

auto build_argument_binding_buffer(
  StreamMDHelper &md, const ArgumentBindingBuffer &buffer,
  llvm::LLVMContext &context, llvm::Module &module
) -> llvm::Type * {
  md.string("air.buffer");
  if (buffer.buffer_size.has_value()) {
    md.string("air.buffer_size");
    md.integer(buffer.buffer_size.value());
  }
  md.string("air.location_index");
  md.integer(buffer.location_index);
  md.integer(buffer.array_size);

  switch (buffer.memory_access) {
  case MemoryAccess::read:
    md.string("air.read");
    break;
  case MemoryAccess::write:
    md.string("air.write");
    break;
  case MemoryAccess::read_write:
    md.string("air.read_write");
    break;
  case MemoryAccess::sample:
    assert(0 && "buffer can't be sampled");
    break;
  }

  md.string("air.address_space");
  md.integer(buffer.address_space);

  auto type = get_llvm_type(buffer.type, context);

  md.string("air.arg_type_size");
  md.integer( // FIXME: bool (i1)
    module.getDataLayout().getTypeSizeInBits(type) / 8
  );

  md.string("air.arg_type_align_size");
  md.integer(module.getDataLayout().getABITypeAlign(type).value());

  md.string("air.arg_type_name");
  md.string(get_name(buffer.type));

  md.string("air.arg_name");
  md.string(buffer.arg_name);

  return type->getPointerTo((uint32_t)buffer.address_space);
};
auto build_argument_binding_indirect_buffer(
  StreamMDHelper &md, const ArgumentBindingIndirectBuffer &indirect_buffer,
  llvm::LLVMContext &context, llvm::Module &module
) -> llvm::Type * {
  md.string("air.indirect_buffer");
  auto struct_layout =
    module.getDataLayout().getStructLayout(indirect_buffer.struct_type);
  md.string("air.buffer_size");
  md.integer((struct_layout->getSizeInBytes() * indirect_buffer.array_size));
  md.string("air.location_index");
  md.integer(indirect_buffer.location_index);
  md.integer(indirect_buffer.array_size);

  switch (indirect_buffer.memory_access) {
  case MemoryAccess::read:
    md.string("air.read");
    break;
  case MemoryAccess::write:
    md.string("air.write");
    break;
  case MemoryAccess::read_write:
    md.string("air.read_write");
    break;
  case MemoryAccess::sample:
    assert(0 && "buffer can't be sampled");
    break;
  }

  md.string("air.address_space");
  md.integer(indirect_buffer.address_space);

  md.string("air.struct_type_info");
  md.metadata(indirect_buffer.struct_type_info);

  md.string("air.arg_type_size");
  md.integer(struct_layout->getSizeInBytes());

  md.string("air.arg_type_align_size");
  md.integer(struct_layout->getAlignment().value());

  md.string("air.arg_type_name");
  md.string(indirect_buffer.struct_type->getName().str());

  md.string("air.arg_name");
  md.string(indirect_buffer.arg_name);

  return indirect_buffer.struct_type->getPointerTo((uint32_t
  )indirect_buffer.address_space);
};
auto build_argument_binding_texture(
  StreamMDHelper &md, const ArgumentBindingTexture &texture,
  llvm::LLVMContext &context, llvm::Module &module
) -> llvm::Type * {
  auto texture_type_name = texture.type.get_name();
  md.string("air.texture")
    ->string("air.location_index")
    ->integer(texture.location_index)
    ->integer(texture.array_size);

  switch (texture.memory_access) {
  case dxmt::air::MemoryAccess::read:
    md.string("air.read");
    break;
  case dxmt::air::MemoryAccess::write:
    md.string("air.write");
    break;
  case dxmt::air::MemoryAccess::read_write:
    md.string("air.read_write");
    break;
  case dxmt::air::MemoryAccess::sample:
    md.string("air.sample");
    break;
  }

  md.string("air.arg_type_name")
    ->string(texture_type_name)
    ->string("air.arg_name")
    ->string(texture.arg_name);

  return texture.type.get_llvm_type(context);
};
auto build_argument_binding_sampler(
  StreamMDHelper &md, const ArgumentBindingSampler &sampler,
  llvm::LLVMContext &context, llvm::Module &module
) -> llvm::Type * {
  md.string("air.sampler")
    ->string("air.location_index")
    ->integer(sampler.location_index)
    ->integer(sampler.array_size)
    ->string("air.arg_type_name")
    ->string("sampler")
    ->string("air.arg_name")
    ->string(sampler.arg_name);
  return (MSLSampler{}).get_llvm_type(context);
};
auto insert_inteterpolation(StreamMDHelper &md, Interpolation interpolation) {
  switch (interpolation) {
  case Interpolation::center_perspective:
    md.string("air.center")->string("air.perspective");
    break;
  case Interpolation::center_no_perspective:
    md.string("air.center")->string("air.no_perspective");
    break;
  case Interpolation::centroid_perspective:
    md.string("air.centroid")->string("air.perspective");
    break;
  case Interpolation::centroid_no_perspective:
    md.string("air.centroid")->string("air.no_perspective");
    break;
  case Interpolation::sample_perspective:
    md.string("air.sample")->string("air.perspective");
    break;
  case Interpolation::sample_no_perspective:
    md.string("air.sample")->string("air.no_perspective");
    break;
  case Interpolation::flat:
    md.string("air.flat");
    break;
  }
}

auto ArgumentBufferBuilder::Build(
  llvm::LLVMContext &context, llvm::Module &module
) -> std::tuple<llvm::StructType *, llvm::MDNode *> {
  std::vector<llvm::Type *> fields;
  std::vector<llvm::Metadata *> indirect_argument;
  uint32_t offset = 0;
  for (auto [name, s] : fieldsType) {
    StreamMDHelper metadata_field;
    metadata_field.integer(offset);
    auto field_type = std::visit(
      patterns{
        [&](const ArgumentBindingBuffer &buffer) {
          return build_argument_binding_buffer(
            metadata_field, buffer, context, module
          );
        },
        [&](const ArgumentBindingIndirectBuffer &indirect_buffer) {
          return build_argument_binding_indirect_buffer(
            metadata_field, indirect_buffer, context, module
          );
        },
        [&](const ArgumentBindingTexture &texture) {
          return build_argument_binding_texture(
            metadata_field, texture, context, module
          );
        },
        [&](const ArgumentBindingSampler &sampler) {
          return build_argument_binding_sampler(
            metadata_field, sampler, context, module
          );
        },
        [&](const ArgumentBindingIndirectConstant &constant) {
          metadata_field.string("air.indirect_constant")
            ->string("air.location_index")
            ->integer(constant.location_index)
            ->integer(constant.array_size)
            ->string("air.arg_type_name")
            ->string(get_name(constant.type))
            ->string("air.arg_name")
            ->string(constant.arg_name);
          return get_llvm_type(constant.type, context);
        }
      },
      s
    );
    fields.push_back(field_type);
    indirect_argument.push_back(metadata_field.BuildTuple(context));
    offset++;
  };

  auto struct_type =
    StructType::create(context, fields, "argument_buffer_struct");

  // struct_type.
  auto struct_layout = module.getDataLayout().getStructLayout(struct_type);
  auto member_offsets = struct_layout->getMemberOffsets();
  StreamMDHelper struct_metadata;
  for (auto [field_type, member_offset, indirect_argument_metadata] :
       zip(fields, member_offsets, indirect_argument)) {
    struct_metadata.integer(member_offset)
      ->integer(
        module.getDataLayout().getTypeSizeInBits(field_type) / 8
      )            // CAVEATS: the bool is represented as i8 in arugment
                   // buffer (but i1 in function argument)
      ->integer(0) // TODO: array_size (0 for non-array, like
                   // MTLArguementDescriptor)
      ->string(cast<MDString>(
                 cast<MDTuple>(indirect_argument_metadata)->operands().end()[-3]
      )
                 ->getString()
                 .str()) // FIXME: is this guaranteed?
      ->string(cast<MDString>(
                 cast<MDTuple>(indirect_argument_metadata)->operands().end()[-1]
      )
                 ->getString()
                 .str()) // FIXME: is this guaranteed?
      ->string("air.indirect_argument")
      ->metadata(indirect_argument_metadata);
  }
  return std::make_pair(struct_type, struct_metadata.BuildTuple(context));
};

uint32_t FunctionSignatureBuilder::DefineInput(const FunctionInput &input) {
  // TODO: check duplication (in case it's already defined)
  uint32_t index = inputs.size();
  inputs.push_back(input);
  return index;
};

uint32_t FunctionSignatureBuilder::DefineOutput(const FunctionOutput &output) {
  // TODO: check duplication (in case it's already defined)
  uint32_t index = inputs.size();
  outputs.push_back(output);
  return index;
};

auto FunctionSignatureBuilder::CreateFunction(
  std::string name, llvm::LLVMContext &context, llvm::Module &module
) -> std::pair<llvm::Function *, llvm::MDNode *> {
  std::vector<Metadata *> metadata_input;
  std::vector<llvm::Type *> type_input;
  std::vector<Metadata *> metadata_output;
  std::vector<llvm::Type *> type_output;
  for (auto &item : enumerate(inputs)) {
    auto i = item.index();
    auto input = item.value();
    StreamMDHelper metadata_field;
    metadata_field.integer(i);
    llvm::Type *field_type = std::visit(
      patterns{
        [&](const InputVertexStageIn &vertex_in) {
          metadata_field.string("air.vertex_input")
            ->string("air.location_index")
            ->integer(vertex_in.attribute)
            ->integer(1)
            ->string("air.arg_type_name")
            ->string(get_name(vertex_in.type))
            ->string("air.arg_name")
            ->string(vertex_in.name);
          return get_llvm_type(vertex_in.type, context);
        },
        [&](const InputFragmentStageIn &frag_in) {
          metadata_field.string("air.fragment_input")
            ->string(std::string("user(") + frag_in.user + ")");
          insert_inteterpolation(metadata_field, frag_in.interpolation);
          metadata_field.string("air.arg_type_name")
            ->string(get_name(frag_in.type))
            ->string("air.arg_name")
            ->string(frag_in.user);
          return get_llvm_type(frag_in.type, context);
        },
        [&](const ArgumentBindingBuffer &buffer) {
          return build_argument_binding_buffer(
            metadata_field, buffer, context, module
          );
        },
        [&](const ArgumentBindingIndirectBuffer &indirect_buffer) {
          return build_argument_binding_indirect_buffer(
            metadata_field, indirect_buffer, context, module
          );
        },
        [&](const ArgumentBindingTexture &texture) {
          return build_argument_binding_texture(
            metadata_field, texture, context, module
          );
        },
        [&](const ArgumentBindingSampler &sampler) {
          return build_argument_binding_sampler(
            metadata_field, sampler, context, module
          );
        },
        [&](const InputPosition &position) {
          metadata_field.string("air.position");
          insert_inteterpolation(metadata_field, position.interpolation);
          metadata_field.string("air.arg_type_name")
            ->string("float4") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_position");

          return msl_float4.get_llvm_type(context);
        },
        [](auto _) {
          assert(0 && "Unhandled input");
          return (llvm::Type *)nullptr;
        }
      },
      input
    );

    metadata_input.push_back(metadata_field.BuildTuple(context));
    type_input.push_back(field_type);
  };
  for (auto &item : enumerate(outputs)) {
    auto output = item.value();
    StreamMDHelper md;
    llvm::Type *field_type = std::visit(
      patterns{
        [&](const OutputVertex &vertex_out) {
          md.string("air.vertex_output")
            ->string("user(" + vertex_out.user + ")")
            ->string("air.arg_type_name")
            ->string(get_name(vertex_out.type))
            ->string("air.arg_name")
            ->string(vertex_out.user);
          return get_llvm_type(vertex_out.type, context);
        },
        [&](const OutputRenderTarget &render_target) {
          md.string("air.render_target")
            ->integer(render_target.index)
            ->integer(0) // TODO: dual source blending
            ->string("air.arg_type_name")
            ->string(get_name(render_target.type))
            ->string("air.arg_name")
            ->string("render_target_" + std::to_string(render_target.index));
          return get_llvm_type(render_target.type, context);
        },
        [&](const OutputPosition &position) {
          md.string("air.position")
            ->string("air.arg_type_name")
            ->string(get_name(position.type))
            ->string("air.arg_name")
            ->string("mtl_position");
          return get_llvm_type(position.type, context);
        },
        [](auto _) {
          assert(0 && "Unhandled output");
          return (llvm::Type *)nullptr;
        }
      },
      output
    );
    metadata_output.push_back(md.BuildTuple(context));
    type_output.push_back(field_type);
  };
  auto output_struct_type = type_output.size() > 0
                              ? StructType::get(context, type_output, true)
                              : Type::getVoidTy(context);
  auto function = Function::Create(
    FunctionType::get(output_struct_type, type_input, false),
    llvm::Function::LinkageTypes::ExternalLinkage, name, module
  );
  auto metadata_output_tuple = MDTuple::get(context, metadata_output);
  auto metadata_input_tuple = MDTuple::get(context, metadata_input);
  return std::make_pair(
    function, MDTuple::get(
                context, {ConstantAsMetadata::get(function),
                          metadata_output_tuple, metadata_input_tuple}
              )
  );
};

} // namespace dxmt::air
#include "air_signature.hpp"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"

using namespace llvm;

namespace dxmt::air {

uint32_t ArgumentBufferBuilder::DefineBuffer(std::string name,
                                             AddressSpace addressp_space,
                                             MemoryAccess access,
                                             MSLRepresentableType type) {
  auto element_index = fieldsType.size();
  assert(fieldsType.find(name) == fieldsType.end() &&
         "otherwise duplicated field name");
  fieldsType[name] = ArgumentBindingBuffer{
      .location_index = (uint32_t)element_index,
      .array_size = 1,
      .memory_access = access, // wtf
      .address_space = addressp_space,
      .type = type,
      .arg_name = name,
  };
  return element_index;
};

uint32_t ArgumentBufferBuilder::DefineSampler(std::string name) {
  auto element_index = fieldsType.size();
  assert(fieldsType.find(name) == fieldsType.end() &&
         "otherwise duplicated field name");
  fieldsType[name] = ArgumentBindingSampler{

  };
  return element_index;
};

uint32_t ArgumentBufferBuilder::DefineTexture(std::string name,
                                              TextureKind kind,
                                              MemoryAccess access,
                                              MSLScalerType scaler_type) {
  auto element_index = fieldsType.size();
  assert(fieldsType.find(name) == fieldsType.end() &&
         "otherwise duplicated field name");
  fieldsType[name] = ArgumentBindingTexture{

  };
  return element_index;
};

auto ArgumentBufferBuilder::Build(llvm::LLVMContext &context,
                                  llvm::Module &module)
    -> std::tuple<llvm::StructType *, llvm::MDNode> {
  std::vector<llvm::Type *> fields;
  for (auto &[name, s] : fieldsType) {
  };
};

template <typename I, typename O>
uint32_t FunctionSignatureBuilder<I, O>::DefineInput(const I &input) {
  // TODO: check duplication (in case it's already defined)
  inputs.push_back(input);
};

template <typename I, typename O>
uint32_t FunctionSignatureBuilder<I, O>::DefineOutput(const O &output) {
  // TODO: check duplication (in case it's already defined)
  outputs.push_back(output);
};

template <typename I, typename O>
llvm::Function *
FunctionSignatureBuilder<I, O>::CreateFunction(llvm::LLVMContext &context,
                                               llvm::Module &module) {
  auto md_str = [&](const char *str) { return MDString::get(context, str); };
  auto md_int = [&](uint32_t n) {
    return ConstantAsMetadata::get(
        ConstantInt::get(context, APInt{32, n, false}));
  };
  std::vector<Metadata *> metadata_input;
  std::vector<llvm::Type *> type_input;
  std::vector<Metadata *> metadata_output;
  std::vector<llvm::Type *> type_output;
  for (auto &[i, input] : enumerate(inputs)) {
    std::vector<Metadata *> metadata_field;
    metadata_field.push_back(md_int(i));
    llvm::Type *field_type = std::visit( // disgusting
        patterns                         // indent
        {                                // fuck it
         [&](const InputVertexStageIn &vertex_in) {
           metadata_field.push_back(md_str("air.vertex_input"));
         },
         [&](const InputFragmentStageIn &vertex_in) {
           metadata_field.push_back(
               md_str("air.fragment_input")); // is this true?
         },
         [&](const ArgumentBindingBuffer &buffer) {
           metadata_field.push_back(md_str("air.buffer"));
           if (buffer.buffer_size.has_value()) {
             metadata_field.push_back(md_str("air.buffer_size"));
             metadata_field.push_back(md_int(buffer.buffer_size.value()));
           }
           metadata_field.push_back(md_str("air.location_index"));
           metadata_field.push_back(md_int(buffer.location_index));
           metadata_field.push_back(md_int(buffer.array_size));
           // access

           metadata_field.push_back(md_str("air.address_space"));
           metadata_field.push_back(md_int(buffer.address_space));

           auto type = get_llvm_type(buffer.type, context);

           metadata_field.push_back(md_str("air.arg_type_size"));
           metadata_field.push_back(md_int( // FIXME: bool (i1)
               module.getDataLayout().getTypeSizeInBits(type) / 8));

           metadata_field.push_back(md_str("air.arg_type_align_size"));
           metadata_field.push_back(
               md_int(module.getDataLayout().getABITypeAlign(type).value()));

           metadata_field.push_back(md_str("air.arg_type_name"));
           metadata_field.push_back(md_str(get_name(buffer.type).c_str()));

           metadata_field.push_back(md_str("air.arg_name"));
           metadata_field.push_back(md_str(buffer.arg_name.c_str()));

           return type->getPointerTo(buffer.address_space);
           ;
         },
         [&](const ArgumentBindingIndirectBuffer &indirect_buffer) {
           metadata_field.push_back(md_str("air.indirect_buffer"));
           auto struct_layout = module.getDataLayout().getStructLayout(
               indirect_buffer.struct_type);
           metadata_field.push_back(md_str("air.buffer_size"));
           metadata_field.push_back(md_int(struct_layout->getSizeInBytes() *
                                           indirect_buffer.array_size));
           metadata_field.push_back(md_str("air.location_index"));
           metadata_field.push_back(md_int(indirect_buffer.location_index));
           metadata_field.push_back(md_int(indirect_buffer.array_size));

           // access

           metadata_field.push_back(md_str("air.address_space"));
           metadata_field.push_back(md_int(indirect_buffer.address_space));

           metadata_field.push_back(md_str("air.struct_type_info"));
           metadata_field.push_back(indirect_buffer.struct_type_info);

           metadata_field.push_back(md_str("air.arg_type_size"));
           metadata_field.push_back(md_int(struct_layout->getSizeInBytes()));

           metadata_field.push_back(md_str("air.arg_type_align_size"));
           metadata_field.push_back(
               md_int(struct_layout->getAlignment().value()));

           metadata_field.push_back(md_str("air.arg_type_name"));
           metadata_field.push_back(md_str(
               (indirect_buffer.arg_name + std::string("_type")).c_str()));

           metadata_field.push_back(md_str("air.arg_name"));
           metadata_field.push_back(md_str(indirect_buffer.arg_name.c_str()));

           return indirect_buffer.struct_type->getPointerTo(
               indirect_buffer.address_space);
         },
         [&](const ArgumentBindingTexture &texture) {
           metadata_field.push_back(md_str("air.texture"));
           // TODO
         },
         [&](const ArgumentBindingSampler &sampler) {
           metadata_field.push_back(md_str("air.sampler"));
           // TODO
         },
         [&](const InputPosition &position) {
           metadata_field.push_back(
               md_str("air.position")); // is this true?
         },
         [](auto _) { assert(0 && "Unhandled input"); }},
        input);

    metadata_input.push_back(MDTuple::get(context, metadata_field));
    type_input.push_back(field_type);
  };
  for (auto &[i, output] : enumerate(outputs)) {
    std::vector<Metadata *> metadata_field;
    llvm::Type *field_type = std::visit( // disgusting
        patterns                         // indent
        {                                // fuck it
         [&](const OutputVertex &vertex_out) {
           // TODO
           return get_llvm_type(vertex_out.type, context);
         },
         [&](const OutputRenderTarget &render_target) {
           // TODO
           return get_llvm_type(render_target.type, context);
         },
         [](auto _) { assert(0 && "Unhandled output"); }},
        output);
    metadata_output.push_back(MDTuple::get(context, metadata_field));
    type_output.push_back(field_type);
  };
};

} // namespace dxmt::air

#include "air_metadata.hpp"
#include "air_constants.hpp"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include <sstream>

using namespace llvm;
using namespace dxmt::air;

namespace dxmt {

AirMetadata::AirMetadata(LLVMContext &context)
    : context(context), builder(context) {}

ConstantAsMetadata *AirMetadata::createUnsignedInteger(uint32_t value) {
  return builder.createConstant(
      ConstantInt::get(context, APInt(32, value, false)));
};

MDString *AirMetadata::createString(Twine string) {
  return builder.createString(string.str());
};

MDString *AirMetadata::createString(StringRef string, StringRef prefix) {
  auto str = prefix + string;
  return builder.createString(str.str());
};

MDNode *AirMetadata::createInput(uint32_t index, air::EInput input,
                                 StringRef argName) {

  switch (input) {
  // vertex
  case air::EInput::amplification_count:
  case air::EInput::amplification_id:
  case air::EInput::base_instance:
  case air::EInput::base_vertex:
  case air::EInput::instance_id:
  case air::EInput::vertex_id:
  // frag
  case air::EInput::primitive_id:
  case air::EInput::sample_id:
  case air::EInput::sample_mask:
  case air::EInput::thread_index_in_quadgroup:
  case air::EInput::thread_index_in_simdgroup:
  case air::EInput::threads_per_simdgroup:
  case air::EInput::render_target_array_index:
  case air::EInput::viewport_array_index:
  // kernel
  case air::EInput::dispatch_quadgroups_per_threadgroup:
  case air::EInput::dispatch_simdgroups_per_threadgroup:
  case air::EInput::quadgroup_index_in_threadgroup:
  case air::EInput::simdgroup_index_in_threadgroup:
  case air::EInput::simdgroups_per_threadgroup:
  // case air::Input::thread_execution_width: // same as threads_per_simdgroup
  case air::EInput::thread_index_in_threadgroup:
    // case air::Input::thread_index_in_quadgroup:
    // case air::Input::thread_index_in_simdgroup:
    return createInput(index, input, air::EType::uint, argName);
    // case air::Input::barycentric_coord: // this needs proper handling (as
    // well as render_target)
    //   return createInput(index, input, air::Type::float3, argName);

  case air::EInput::dispatch_threads_per_threadgroup:
  case air::EInput::stage_in_grid_origin:
  case air::EInput::stage_in_grid_size:
  case air::EInput::thread_position_in_grid:
  case air::EInput::threadgroup_position_in_grid:
  case air::EInput::thread_position_in_threadgroup:
  case air::EInput::threadgroups_per_grid:
  case air::EInput::threads_per_grid:
  case air::EInput::threads_per_threadgroup:
    return createInput(index, input, air::EType::uint3, argName);
  case air::EInput::front_facing:
    return createInput(index, input, air::EType::_bool, argName);
  case air::EInput::point_coord:
    return createInput(index, input, air::EType::float2, argName);
  // case air::Input::position: // proper handling: interpolation
  //   return createInput(index, input, air::Type::float4, argName);
  default:
    // should throw here
    break;
  }
  return nullptr;
}

llvm::MDNode *AirMetadata::createInput(uint32_t index, air::EInput input,
                                       air::EType type,
                                       llvm::StringRef argName) {
  return MDTuple::get(context,
                      {createUnsignedInteger(index),
                       createString(input._to_string(), "air."),
                       createString("air.arg_type_name"),
                       createString(air::getAirTypeName(type)),
                       createString("air.arg_name"), createString(argName)});
}

llvm::MDNode *AirMetadata::createUserVertexInput(uint32_t index,
                                                 uint32_t location,
                                                 air::EType type,
                                                 llvm::StringRef argName) {
  return MDTuple::get(
      context,
      {createUnsignedInteger(index), createString("air.vertex_input"),
       createString("air.location_index"), createUnsignedInteger(location),
       createUnsignedInteger(1), // TODO: figure out why it's 1
       createString("air.arg_type_name"),
       createString(air::getAirTypeName(type)), createString("air.arg_name"),
       createString(argName)});
}

llvm::MDNode *AirMetadata::createSamplerBinding(uint32_t index,
                                                uint32_t location,
                                                uint32_t arraySize,
                                                llvm::StringRef argName) {
  return MDTuple::get(
      context,
      {createUnsignedInteger(index), createString("air.sampler"),
       createString("air.location_index"), createUnsignedInteger(location),
       createUnsignedInteger(arraySize), createString("air.arg_type_name"),
       createString("sampler"), createString("air.arg_name"),
       createString(argName)});
}

llvm::MDNode *AirMetadata::createUserKernelInput(uint32_t index,
                                                 uint32_t location,
                                                 air::EType type,
                                                 llvm::StringRef argName) {
  return MDTuple::get(
      context,
      {createUnsignedInteger(index), createString("air.stage_in"),
       createString("air.location_index"), createUnsignedInteger(location),
       createUnsignedInteger(1), // TODO: figure out why it's 1
       createString("air.arg_type_name"),
       createString(air::getAirTypeName(type)), createString("air.arg_name"),
       createString(argName)});
}

llvm::MDNode *AirMetadata::createOutput(air::EOutput output,
                                        llvm::StringRef argName) {
  switch (output) {
  case air::EOutput::render_target_array_index:
  case air::EOutput::viewport_array_index:
  case air::EOutput::stencil:     // frag
  case air::EOutput::sample_mask: // frag
    return createOutput(output, air::EType::uint, argName);
  case air::EOutput::position:
    return createOutput(output, air::EType::float4, argName);
  case air::EOutput::point_size:
    return createOutput(output, air::EType::_float, argName);
  case air::EOutput::render_target:
    return createRenderTargetOutput(0, 0, air::EType::float4, argName);
  case air::EOutput::depth:
    return createDepthTargetOutput(air::EDepthArgument::any,
                                   argName); // TODO: is this correct?
  default:
    break;
  }
  assert(0 && "unhandled output");
  return nullptr; // not reachable
}

llvm::MDNode *AirMetadata::createOutput(air::EOutput output, air::EType type,
                                        llvm::StringRef argName) {
  return MDTuple::get(context,
                      {createString(output._to_string(), "air."),
                       createString("air.arg_type_name"),
                       createString(air::getAirTypeName(type)),
                       createString("air.arg_name"), createString(argName)});
}

llvm::MDNode *AirMetadata::createRenderTargetOutput(uint32_t attachmentIndex,
                                                    uint32_t idkIndex,
                                                    air::EType type,
                                                    llvm::StringRef argName) {
  return MDTuple::get(context,
                      {createString("air.render_target"),
                       createUnsignedInteger(attachmentIndex),
                       createUnsignedInteger(idkIndex),
                       createString("air.arg_type_name"),
                       createString(air::getAirTypeName(type)),
                       createString("air.arg_name"), createString(argName)});
}

llvm::MDNode *
AirMetadata::createDepthTargetOutput(air::EDepthArgument depthArgument,
                                     llvm::StringRef argName) {

  return MDTuple::get(
      context, {createString("air.depth"), createString("air.depth_qualifier"),
                createString(depthArgument._to_string(), "air."),
                createString("air.arg_type_name"), createString("float"),
                createString("air.arg_name"), createString(argName)});
}

llvm::MDNode *AirMetadata::createUserVertexOutput(llvm::StringRef key,
                                                  air::EType type,
                                                  llvm::StringRef argName) {
  std::stringstream userKey;
  userKey << "user(" << key.str() << ")";
  return MDTuple::get(
      context, {createString("air.vertex_output"), createString(userKey.str()),
                createString("air.arg_type_name"),
                createString(air::getAirTypeName(type)),
                createString("air.arg_name"), createString(argName)});
};

llvm::MDNode *
AirMetadata::createUserFragmentInput(uint32_t index, llvm::StringRef key,
                                     air::ESampleInterpolation interpolation,
                                     air::EType type, llvm::StringRef argName) {
  std::stringstream userKey;
  userKey << "user(" << key.str() << ")";
  std::vector<Metadata *> data = {createUnsignedInteger(index),
                                  createString("air.fragment_input"),
                                  createString(userKey.str())};

  switch (interpolation) {
  case air::ESampleInterpolation::flat:
    data.push_back(createString("air.flat"));
    break;
  case air::ESampleInterpolation::sample_no_perspective:
    data.push_back(createString("air.sample"));
    data.push_back(createString("air.no_perspective"));
    break;
  case air::ESampleInterpolation::sample_perspective:
    data.push_back(createString("air.sample"));
    data.push_back(createString("air.perspective"));
    break;
  case air::ESampleInterpolation::center_no_perspective:
    data.push_back(createString("air.center"));
    data.push_back(createString("air.no_perspective"));
    break;
  case air::ESampleInterpolation::center_perspective:
    data.push_back(createString("air.center"));
    data.push_back(createString("air.perspective"));
    break;
  case air::ESampleInterpolation::centroid_no_perspective:
    data.push_back(createString("air.centroid"));
    data.push_back(createString("air.no_perspective"));
    break;
  case air::ESampleInterpolation::centroid_perspective:
    data.push_back(createString("air.centroid"));
    data.push_back(createString("air.perspective"));
    break;
  }

  data.insert(data.end(),
              {createString("air.arg_type_name"),
               createString(air::getAirTypeName(type)),
               createString("air.arg_name"), createString(argName)});

  return MDTuple::get(context, data);
}

llvm::MDNode *AirMetadata::createBufferBinding(
    uint32_t index, uint32_t location, uint32_t arraySize,
    air::EBufferAccess access, air::EBufferAddrSpace addrSpace,
    air::EType elementType, llvm::StringRef argName) {
  switch (elementType) {
  case air::EType::float4:
  case air::EType::uint4:
    return createBufferBinding(index, location, arraySize, access, addrSpace,
                               16, 16, air::getAirTypeName(elementType),
                               argName);
  default:
    assert(0 && "Overload not implemented");
  }
}

llvm::MDNode *AirMetadata::createBufferBinding(
    uint32_t index, uint32_t location, uint32_t arraySize,
    air::EBufferAccess access, air::EBufferAddrSpace addrSpace,
    uint32_t argTypeSize, uint32_t argTypeAlignSize, llvm::StringRef typeName,
    llvm::StringRef argName) {

  return MDTuple::get(
      context,
      {

          createUnsignedInteger(index), createString("air.buffer"),
          createString("air.location_index"), createUnsignedInteger(location),
          createUnsignedInteger(arraySize),
          createString(access._to_string(), "air."),
          createString("air.address_space"), createUnsignedInteger(addrSpace),
          createString("air.arg_type_size"), createUnsignedInteger(argTypeSize),
          createString("air.arg_type_align_size"),
          createUnsignedInteger(argTypeAlignSize),
          createString("air.arg_type_name"), createString(typeName),
          createString("air.arg_name"), createString(argName)});
}

llvm::MDNode *AirMetadata::createIndirectBufferBinding(
    uint32_t index, uint32_t bufferSize, uint32_t location, uint32_t arraySize,
    air::EBufferAccess access, air::EBufferAddrSpace addrSpace,
    llvm::MDNode *structTypeInfo, uint32_t structAlignSize,
    llvm::StringRef structTypeName, llvm::StringRef argName) {
  return MDTuple::get(context, {createUnsignedInteger(index),
                                createString("air.indirect_buffer"),
                                createString("air.buffer_size"),
                                createUnsignedInteger(bufferSize),
                                createString("air.location_index"),
                                createUnsignedInteger(location),
                                createUnsignedInteger(arraySize),
                                createString(access._to_string(), "air."),
                                createString("air.address_space"),
                                createUnsignedInteger(addrSpace),
                                createString("air.struct_type_info"),
                                structTypeInfo,
                                createString("air.arg_type_size"),
                                createUnsignedInteger(bufferSize),
                                createString("air.arg_type_align_size"),
                                createUnsignedInteger(structAlignSize),
                                createString("air.arg_type_name"),
                                createString(structTypeName),
                                createString("air.arg_name"),
                                createString(argName)});
}

} // namespace dxmt
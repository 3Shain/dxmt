#include "air_signature_builder.hpp"

using namespace llvm;

namespace dxmt::air {

AirStructureSignatureBuilder::AirStructureSignatureBuilder(AirType &types, AirMetadata& metadata)
    : types(types), metadata(metadata) {}

uint32_t
AirStructureSignatureBuilder::DefineBuffer(EBufferAddrSpace addrSpace) {

  return elementCount_++;
};

auto AirStructureSignatureBuilder::Build()
    -> std::tuple<llvm::Type *, llvm::MDNode>{

    };

} // namespace dxmt::air
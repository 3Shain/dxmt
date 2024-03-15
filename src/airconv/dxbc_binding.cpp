#include "dxbc_binding.hpp"
#include "air_metadata.hpp"
#include "air_type.hpp"
#include "dxbc_binding_sm50.hpp"
#include <vector>

using namespace llvm;
using namespace dxmt::air;

namespace dxmt::dxbc {

class DxbcBindingSM50 : public IDxbcBindingDeclare {
public:
  DxbcBindingSM50() {}

  void DeclareConstantBuffer(uint32_t slot, uint32_t size) {}

  void Initialize() {
    // add new parameter
    // generate metadata for for that parameter (it would be a structure)

    // and there should be a callback that: get the value for parameter, which
    // is needed for access
  }

  AirType &types;
  AirMetadata &metadata;

  std::vector<uint32_t> constantBuffers;
};

class DxbcBindingResolveSM50 : public IDxbcBindingResolve {

  DxbcBindingResolveSM50(const DxbcBindingSM50 &declare) : declare(declare) {}

  Value *AccessConstantBuffer(uint32_t slot){

  };

  const DxbcBindingSM50 &declare;
};

} // namespace dxmt::dxbc
#pragma once
#include <cstdint>

namespace dxmt {

enum class SM50BindableResourceKind {
  ConstantBuffer,
  ShaderResourceView,
  UnorderedAccessView,
  Sampler,
  UnorderedAccessViewCounter,
};

inline uint32_t CalculateBindingArgumentIndex(SM50BindableResourceKind kind,
                                               uint32_t slot) {
  switch (kind) {
  case SM50BindableResourceKind::ConstantBuffer:
    return 0 + slot; // at most 16
  case SM50BindableResourceKind::Sampler:
    return 16 + slot; // at most 16
  case SM50BindableResourceKind::ShaderResourceView:
    return 32 + slot; // 128
  case SM50BindableResourceKind::UnorderedAccessView:
    return 192 + slot;
  case SM50BindableResourceKind::UnorderedAccessViewCounter:
    return 256 + slot;
  }
};
} // namespace dxmt
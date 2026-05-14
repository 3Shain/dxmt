#include "d3d12_root_signature.hpp"
#include "d3d12_device.hpp"
#include "dxmt_device.hpp"
#include "dxmt_sampler.hpp"
#include "log/log.hpp"
#include <cstring>

namespace dxmt::d3d12 {

// ── Filter mapping helpers ──

static WMTSamplerMinMagFilter MapFilter(D3D12_FILTER filter) {
    switch (filter) {
    case D3D12_FILTER_MIN_MAG_MIP_POINT:                return WMTSamplerMinMagFilterNearest;
    case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR:         return WMTSamplerMinMagFilterNearest;
    case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:   return WMTSamplerMinMagFilterLinear;
    case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR:         return WMTSamplerMinMagFilterLinear;
    case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT:         return WMTSamplerMinMagFilterLinear;
    case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:  return WMTSamplerMinMagFilterLinear;
    case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:         return WMTSamplerMinMagFilterLinear;
    case D3D12_FILTER_MIN_MAG_MIP_LINEAR:               return WMTSamplerMinMagFilterLinear;
    case D3D12_FILTER_ANISOTROPIC:                      return WMTSamplerMinMagFilterLinear;
    default:                                            return WMTSamplerMinMagFilterNearest;
    }
}

static WMTSamplerMipFilter MapMipFilter(D3D12_FILTER filter) {
    switch (filter) {
    case D3D12_FILTER_MIN_MAG_MIP_POINT:                return WMTSamplerMipFilterNearest;
    case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR:         return WMTSamplerMipFilterLinear;
    case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:   return WMTSamplerMipFilterNearest;
    case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR:         return WMTSamplerMipFilterLinear;
    case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT:         return WMTSamplerMipFilterNearest;
    case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:  return WMTSamplerMipFilterLinear;
    case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:         return WMTSamplerMipFilterNearest;
    case D3D12_FILTER_MIN_MAG_MIP_LINEAR:               return WMTSamplerMipFilterLinear;
    case D3D12_FILTER_ANISOTROPIC:                      return WMTSamplerMipFilterLinear;
    default:                                            return WMTSamplerMipFilterNearest;
    }
}

static WMTSamplerAddressMode MapAddressMode(D3D12_TEXTURE_ADDRESS_MODE mode) {
    switch (mode) {
    case D3D12_TEXTURE_ADDRESS_MODE_WRAP:        return WMTSamplerAddressModeRepeat;
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR:      return WMTSamplerAddressModeMirrorRepeat;
    case D3D12_TEXTURE_ADDRESS_MODE_CLAMP:       return WMTSamplerAddressModeClampToEdge;
    case D3D12_TEXTURE_ADDRESS_MODE_BORDER:      return WMTSamplerAddressModeClampToBorderColor;
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE: return WMTSamplerAddressModeMirrorClampToEdge;
    default:                                     return WMTSamplerAddressModeClampToEdge;
    }
}

D3D12RootSignature::D3D12RootSignature(
    D3D12Device *device,
    const D3D12_ROOT_SIGNATURE_DESC *pDesc)
    : device_(device)
    , refcount_(1) {

    if (pDesc) {
        ParseParameters(pDesc->pParameters, pDesc->NumParameters);
        ParseStaticSamplers(pDesc->pStaticSamplers, pDesc->NumStaticSamplers);
        SerializeBlob(pDesc);
    }

    TRACE("D3D12RootSignature created: ", params_.size(), " params, ",
          static_samplers_.size(), " static samplers, ",
          arg_buffer_size_, " bytes arg buffer");
}

D3D12RootSignature::~D3D12RootSignature() {
    TRACE("D3D12RootSignature destroyed");
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12RootSignature::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12RootSignature) {
        *ppvObject = static_cast<ID3D12RootSignature *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12RootSignature::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12RootSignature::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) delete this;
    return c;
}

// ── Accessors ──

const RootParameterBinding &D3D12RootSignature::GetParamBinding(UINT index) const {
    static RootParameterBinding empty = {};
    return index < params_.size() ? params_[index] : empty;
}

const std::vector<D3D12RootSignature::DescriptorTableRange> &
D3D12RootSignature::GetDescriptorRanges(UINT param_idx) const {
    static std::vector<DescriptorTableRange> empty;
    return param_idx < descriptor_ranges_.size() ? descriptor_ranges_[param_idx] : empty;
}

// ── Root parameter parsing ──

void D3D12RootSignature::ParseParameters(
    const D3D12_ROOT_PARAMETER *pParams, UINT count) {

    if (!pParams || count == 0) return;

    for (UINT i = 0; i < count; i++) {
        const auto &param = pParams[i];
        RootParameterBinding binding = {};
        binding.type = param.ParameterType;
        binding.shader_visibility = param.ShaderVisibility;
        binding.arg_buffer_index = kArgBufferSlot;
        binding.arg_buffer_offset = arg_buffer_size_;

        switch (param.ParameterType) {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
            // Parse descriptor ranges into flat list
            std::vector<DescriptorTableRange> ranges;
            for (UINT r = 0; r < param.DescriptorTable.NumDescriptorRanges; r++) {
                const auto &range = param.DescriptorTable.pDescriptorRanges[r];
                DescriptorTableRange dt_range = {};
                dt_range.type = range.RangeType;
                dt_range.base_shader_register = range.BaseShaderRegister;
                dt_range.num_descriptors = range.NumDescriptors;
                dt_range.offset_in_handles = range.OffsetInDescriptorsFromTableStart;
                ranges.push_back(dt_range);
            }
            descriptor_ranges_.push_back(std::move(ranges));
            // Each descriptor is 8 bytes in the argument buffer
            UINT total_descriptors = 0;
            for (auto &r : descriptor_ranges_.back())
                total_descriptors += r.num_descriptors;
            arg_buffer_size_ += total_descriptors * 8;
            break;
        }
        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            arg_buffer_size_ += param.Constants.Num32BitValues * 4;
            descriptor_ranges_.push_back({});
            break;
        case D3D12_ROOT_PARAMETER_TYPE_CBV:
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
            arg_buffer_size_ += 16; // GPU VA + size
            descriptor_ranges_.push_back({});
            break;
        }

        params_.push_back(binding);
    }
}

// ── Static sampler parsing ──

void D3D12RootSignature::ParseStaticSamplers(
    const D3D12_STATIC_SAMPLER_DESC *pSamplers, UINT count) {

    if (!pSamplers || count == 0) return;

    WMT::Device mtl_device = device_->GetMTLDevice();

    for (UINT i = 0; i < count; i++) {
        const auto &s = pSamplers[i];

        // Build Metal sampler descriptor from D3D12 static sampler
        WMTSamplerInfo info = {};
        info.min_filter = MapFilter(s.Filter);
        info.mag_filter = MapFilter(s.Filter);
        info.mip_filter = MapMipFilter(s.Filter);
        info.r_address_mode = MapAddressMode(s.AddressU);
        info.s_address_mode = MapAddressMode(s.AddressV);
        info.t_address_mode = MapAddressMode(s.AddressW);
        info.border_color = WMTSamplerBorderColorTransparentBlack;
        info.compare_function = s.ComparisonFunc != D3D12_COMPARISON_FUNC_NONE
            ? WMTCompareFunctionAlways : WMTCompareFunctionNever;
        info.lod_min_clamp = s.MinLOD;
        info.lod_max_clamp = FLT_MAX;
        info.max_anisotroy = s.Filter == D3D12_FILTER_ANISOTROPIC
            ? std::max(s.MaxAnisotropy, 1u) : 1;
        info.normalized_coords = true;
        info.lod_average = false;
        info.support_argument_buffers = true;

        auto sampler = Sampler::createSampler(mtl_device, info, s.MipLODBias);
        if (sampler) {
            static_samplers_.push_back(std::move(sampler));
        }
    }

    TRACE("ParseStaticSamplers: created ", static_samplers_.size(), " samplers");
}

// ── Blob serialization ──

void D3D12RootSignature::SerializeBlob(const D3D12_ROOT_SIGNATURE_DESC *pDesc) {

    if (!pDesc) return;

    // Simple binary copy of the descriptor for GetCachedBlob
    size_t param_size = pDesc->NumParameters * sizeof(D3D12_ROOT_PARAMETER);
    size_t sampler_size = pDesc->NumStaticSamplers * sizeof(D3D12_STATIC_SAMPLER_DESC);
    size_t total = sizeof(D3D12_ROOT_SIGNATURE_FLAGS) +
                   sizeof(UINT) + param_size +
                   sizeof(UINT) + sampler_size;

    blob_.resize(total);
    uint8_t *dst = blob_.data();

    // Flags
    memcpy(dst, &pDesc->Flags, sizeof(D3D12_ROOT_SIGNATURE_FLAGS));
    dst += sizeof(D3D12_ROOT_SIGNATURE_FLAGS);

    // NumParameters + parameters
    memcpy(dst, &pDesc->NumParameters, sizeof(UINT));
    dst += sizeof(UINT);
    if (pDesc->NumParameters > 0) {
        memcpy(dst, pDesc->pParameters, param_size);
        dst += param_size;
    }

    // NumStaticSamplers + samplers
    memcpy(dst, &pDesc->NumStaticSamplers, sizeof(UINT));
    dst += sizeof(UINT);
    if (pDesc->NumStaticSamplers > 0) {
        memcpy(dst, pDesc->pStaticSamplers, sampler_size);
    }
}

} // namespace dxmt::d3d12

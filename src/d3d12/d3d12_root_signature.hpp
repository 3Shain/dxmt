#pragma once

#include "d3d12_private.h"
#include "dxmt_sampler.hpp"
#include "rc/util_rc_ptr.hpp"
#include <atomic>
#include <vector>

namespace dxmt::d3d12 {

class D3D12Device;

struct RootParameterBinding {
    D3D12_ROOT_PARAMETER_TYPE type;
    UINT shader_visibility;
    UINT arg_buffer_offset;    // byte offset in argument buffer
    UINT arg_buffer_index;     // Metal [[buffer(N)]] slot
};

class D3D12RootSignature final : public ID3D12RootSignature {
public:
    D3D12RootSignature(D3D12Device *device,
                        const D3D12_ROOT_SIGNATURE_DESC *pDesc);
    ~D3D12RootSignature();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final { return S_OK; }

    // ── Accessors ──
    UINT GetNumParameters() const { return (UINT)params_.size(); }
    const RootParameterBinding &GetParamBinding(UINT index) const;
    UINT GetArgBufferSize() const { return arg_buffer_size_; }
    UINT GetArgBufferIndex() const { return kArgBufferSlot; }
    bool HasStaticSamplers() const { return !static_samplers_.empty(); }

    // ── Descriptor table ranges ──
    struct DescriptorTableRange {
        D3D12_DESCRIPTOR_RANGE_TYPE type;
        UINT base_shader_register;
        UINT num_descriptors;
        UINT offset_in_handles;
    };
    const std::vector<DescriptorTableRange> &GetDescriptorRanges(UINT param_idx) const;

    // ── Serialized blob ──
    const void *GetBlob() const { return blob_.data(); }
    size_t GetBlobSize() const { return blob_.size(); }

private:
    void ParseParameters(const D3D12_ROOT_PARAMETER *pParams, UINT count);
    void ParseStaticSamplers(const D3D12_STATIC_SAMPLER_DESC *pSamplers, UINT count);
    void SerializeBlob(const D3D12_ROOT_SIGNATURE_DESC *pDesc);

    static constexpr UINT kArgBufferSlot = 29; // Fixed Metal buffer slot

    D3D12Device *device_;
    std::vector<RootParameterBinding> params_;
    std::vector<std::vector<DescriptorTableRange>> descriptor_ranges_;
    std::vector<Rc<Sampler>> static_samplers_;
    UINT arg_buffer_size_ = 0;
    std::vector<uint8_t> blob_;
    std::atomic<ULONG> refcount_;
};

} // namespace dxmt::d3d12

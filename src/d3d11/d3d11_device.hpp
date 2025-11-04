#pragma once

#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_3.h"
#include "d3d11_multithread.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_device.hpp"
#include "dxmt_format.hpp"

struct MTL_GRAPHICS_PIPELINE_DESC;
struct MTL_COMPUTE_PIPELINE_DESC;

DEFINE_COM_INTERFACE("3a3f085a-d0fe-4324-b0ae-fe04de18571c",
                     IMTLD3D11DeviceContext)
    : public ID3D11DeviceContext4 {
  virtual void WaitUntilGPUIdle() = 0;
  virtual void PrepareFlush() = 0;
  virtual void Commit() = 0;
  virtual void SOGetTargetsWithOffsets(UINT NumBuffers, REFIID riid, void ** ppSOTargets, UINT *pOffsets) = 0;
};

namespace dxmt {

class MTLCompiledGraphicsPipeline;
class MTLCompiledComputePipeline;
class MTLCompiledGeometryPipeline;
class MTLCompiledTessellationMeshPipeline;

class MTLD3D11Device : public ID3D11Device5 {
public:

  virtual WMT::Device STDMETHODCALLTYPE GetMTLDevice() = 0;

  virtual HRESULT CreateGraphicsPipeline(MTL_GRAPHICS_PIPELINE_DESC * pDesc,
                                         MTLCompiledGraphicsPipeline *
                                             *ppPipeline) = 0;

  virtual HRESULT CreateComputePipeline(MTL_COMPUTE_PIPELINE_DESC * pDesc,
                                        MTLCompiledComputePipeline *
                                            *ppPipeline) = 0;

  virtual HRESULT CreateGeometryPipeline(MTL_GRAPHICS_PIPELINE_DESC * pDesc,
                                             MTLCompiledGeometryPipeline *
                                                 *ppPipeline) = 0;

  virtual HRESULT CreateTessellationMeshPipeline(MTL_GRAPHICS_PIPELINE_DESC * pDesc,
                                             MTLCompiledTessellationMeshPipeline *
                                                 *ppPipeline) = 0;

  virtual bool IsTraced() = 0;

  virtual Device& GetDXMTDevice() = 0;

  virtual void CreateCommandList(ID3D11CommandList** pCommandList) = 0;

  virtual FormatCapability GetMTLPixelFormatCapability(WMTPixelFormat Format) = 0;

  virtual IMTLD3D11DeviceContext *GetImmediateContextPrivate() = 0;

  virtual unsigned int GetDirectXVersion() = 0;

  d3d11_device_mutex mutex;
};

Com<IMTLDXGIDevice> CreateD3D11Device(std::unique_ptr<Device> &&device,
                                      IMTLDXGIAdapter *pAdapter,
                                      D3D_FEATURE_LEVEL FeatureLevel,
                                      UINT Flags);
} // namespace dxmt
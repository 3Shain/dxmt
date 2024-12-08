#pragma once

#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_3.h"
#include "Metal/MTLDevice.hpp"
#include "d3d11_private.h"
#include "dxgi_interfaces.h"
#include "dxmt_buffer_pool.hpp"
#include "dxmt_device.hpp"

DEFINE_COM_INTERFACE("14e1e5e4-3f08-4741-a8e3-597d79373266", IMTLThreadpoolWork)
    : public IUnknown {
  virtual IMTLThreadpoolWork* RunThreadpoolWork() = 0;
  virtual bool GetIsDone() = 0;
  virtual void SetIsDone(bool state) = 0;
};

struct IMTLCompiledGraphicsPipeline;
struct IMTLCompiledComputePipeline;
struct IMTLCompiledTessellationPipeline;

struct MTL_GRAPHICS_PIPELINE_DESC;
struct MTL_COMPUTE_PIPELINE_DESC;


namespace dxmt {

class MTLD3D11Device : public ID3D11Device3 {
public:

  virtual void AddRefPrivate() = 0;
  virtual void ReleasePrivate() = 0;

  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
  /**
  TODO: should ensure pWork is not released before executed
  or support cancellation.
  In theory a work can be submitted multiple time,
  if that makes sense (usually not)
  */
  virtual void SubmitThreadgroupWork(IMTLThreadpoolWork * pWork) = 0;

  virtual HRESULT CreateGraphicsPipeline(MTL_GRAPHICS_PIPELINE_DESC * pDesc,
                                         IMTLCompiledGraphicsPipeline *
                                             *ppPipeline) = 0;

  virtual HRESULT CreateComputePipeline(MTL_COMPUTE_PIPELINE_DESC * pDesc,
                                        IMTLCompiledComputePipeline *
                                            *ppPipeline) = 0;

  virtual HRESULT CreateTessellationPipeline(MTL_GRAPHICS_PIPELINE_DESC * pDesc,
                                             IMTLCompiledTessellationPipeline *
                                                 *ppPipeline) = 0;

  virtual bool IsTraced() = 0;

  virtual Device& GetDXMTDevice() = 0;

  virtual void CreateCommandList(ID3D11CommandList** pCommandList) = 0;
};

Com<IMTLDXGIDevice> CreateD3D11Device(std::unique_ptr<Device> &&device,
                                      IMTLDXGIAdapter *pAdapter,
                                      D3D_FEATURE_LEVEL FeatureLevel,
                                      UINT Flags);
} // namespace dxmt
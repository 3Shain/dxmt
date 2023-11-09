
#pragma once
#include "../util/objc_pointer.h"
#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"
#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "dxmt_binding.hpp"
#include <unordered_map>
namespace dxmt {

struct RenderPipelineCacheEntry {
  void *VertexShaderRef;
  void *PixelShaderRef;

  bool operator==(const RenderPipelineCacheEntry &other) const {
    return VertexShaderRef == other.VertexShaderRef &&
           PixelShaderRef == other.PixelShaderRef;
  }
};

class RenderPipelineCache : public RcObject {
public:
  Obj<MTL::RenderPipelineState> PSO;
  Obj<MTL::ArgumentEncoder> vertexFunctionArgumentEncoder;
  Obj<MTL::ArgumentEncoder> fragmentFunctionArgumentEncoder;

  RenderPipelineCache(MTL::RenderPipelineState *pso, MTL::ArgumentEncoder *ve,
                      MTL::ArgumentEncoder *fe)
      : PSO(pso), vertexFunctionArgumentEncoder(ve),
        fragmentFunctionArgumentEncoder(fe) {}
};

RenderPipelineCache *findCache(RenderPipelineCacheEntry &key);
void setCache(RenderPipelineCacheEntry &key, RenderPipelineCache *value);

} // namespace dxmt

template <> struct std::hash<dxmt::RenderPipelineCacheEntry> {
  std::size_t operator()(const dxmt::RenderPipelineCacheEntry &p) const {
    return std::hash<void *>()(p.VertexShaderRef) ^
           std::hash<void *>()(p.PixelShaderRef);
  }
};
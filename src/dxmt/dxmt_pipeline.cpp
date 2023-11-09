#include "./dxmt_pipeline.hpp"
#include <utility>

namespace dxmt {

std::unordered_map<RenderPipelineCacheEntry, Rc<RenderPipelineCache>> cache;

RenderPipelineCache *findCache(RenderPipelineCacheEntry &key) {
  if (auto w = cache.find(key); w != cache.end()) {
    return w->second.ptr();
  }
  return nullptr;
};

void setCache(RenderPipelineCacheEntry &key, RenderPipelineCache *value) {
  cache.insert(std::make_pair(key, Rc(value)));
};
} // namespace dxmt
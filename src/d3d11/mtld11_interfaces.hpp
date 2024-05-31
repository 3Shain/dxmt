#pragma once

#include "Metal/MTLBuffer.hpp"
#include "com/com_guid.hpp"

DEFINE_COM_INTERFACE("3b1c3251-2053-4d41-99b8-c6c246d0b219",
                     IMTLDynamicBufferPool)
    : public IUnknown {
  /**
  Provide a buffer (include its ownership) and return a buffer of
  the same size (also include its ownership).
  "buffer" is specifically a dynamic buffer: cpu write-only, gpu
  read-only.

  */
  virtual void ExchangeFromPool(MTL::Buffer * *ppBuffer) = 0;
};

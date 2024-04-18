#pragma once

#include "Metal/MTLBuffer.hpp"
#include "com/com_guid.hpp"

DEFINE_COM_INTERFACE("3b1c3251-2053-4d41-99b8-c6c246d0b219",
                     IMTLDynamicBufferPool)
    : public IUnknown {

  /*
   * Recycle the old buffer and return a fresh buffer.
   * The content of new buffer is undefined.
   * Note the old buffer might be still (or will be) used by GPU
   */
  virtual void SwapDynamicBuffer(MTL::Buffer * pCurrent) = 0;
};

DEFINE_COM_INTERFACE("3b1c3251-2053-4d41-99b8-c6c246d0b251",
                     IMTLRenamingObserver)
    : public IUnknown {

  /*
   * Notify the observer that a renaming happen
   */
  virtual void OnRenamed(IUnknown * pState) = 0;
};

DEFINE_COM_INTERFACE("db4b9deb-d72e-468a-bb22-0624c81cb970",
                     IMTLDynamicResource)
    : public IUnknown {

  /**
   * Observe any renaming happened and notify the observer
   * The state is passed to observer. It's also the identifier
   * of this observation, which can be cancelled later
   */
  virtual HRESULT ObserveRenaming(IMTLRenamingObserver * pObserver,
                                  IUnknown * pState) = 0;
  /**
   * Unobserve
   * \see ObserveRenaming
   */
  virtual HRESULT UnobserveRenaming(IUnknown * pState) = 0;
};

DEFINE_COM_INTERFACE("65feb8c5-01de-49df-bf58-d115007a117d", IMTLDynamicBuffer)
    : public IMTLDynamicResource {
  virtual MTL::Buffer* GetCurrent() = 0;
};
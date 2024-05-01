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

// DEFINE_COM_INTERFACE("3b1c3251-2053-4d41-99b8-c6c246d0b251",
//                      IMTLRenamingObserver)
//     : public IUnknown {

//   /*
//    * Notify the observer that a renaming happen
//    */
//   virtual void OnRenamed(IUnknown * pState) = 0;
// };

// DEFINE_COM_INTERFACE("db4b9deb-d72e-468a-bb22-0624c81cb970",
//                      IMTLDynamicResource)
//     : public IUnknown {

//   /**
//    * Observe any renaming happened and notify the observer
//    * The state is passed to observer. It's also the identifier
//    * of this observation, which can be cancelled later
//    */
//   virtual HRESULT ObserveRenaming(IMTLRenamingObserver * pObserver,
//                                   IUnknown * pState) = 0;
//   /**
//    * Unobserve
//    * \see ObserveRenaming
//    */
//   virtual HRESULT UnobserveRenaming(IUnknown * pState) = 0;
// };
#pragma once

#include <windows.h>

typedef LONG NTSTATUS;
typedef UINT D3DKMT_HANDLE;

typedef struct _D3DKMT_CREATEDEVICEFLAGS
{
  UINT LegacyMode : 1;
  UINT RequestVSync : 1;
  UINT DisableGpuTimeout : 1;
  UINT Reserved : 29;
} D3DKMT_CREATEDEVICEFLAGS;

typedef struct _D3DDDI_ALLOCATIONLIST
{
  D3DKMT_HANDLE hAllocation;
  union
  {
    struct
    {
      UINT WriteOperation : 1;
      UINT DoNotRetireInstance : 1;
      UINT OfferPriority : 3;
      UINT Reserved : 27;
    } DUMMYSTRUCTNAME;
    UINT Value;
  } DUMMYUNIONNAME;
} D3DDDI_ALLOCATIONLIST;

typedef struct _D3DDDI_PATCHLOCATIONLIST
{
  UINT AllocationIndex;
  union
  {
    struct
    {
      UINT SlotId : 24;
      UINT Reserved : 8;
    } DUMMYSTRUCTNAME;
    UINT Value;
  } DUMMYUNIONNAME;
  UINT DriverId;
  UINT AllocationOffset;
  UINT PatchOffset;
  UINT SplitOffset;
} D3DDDI_PATCHLOCATIONLIST;

typedef struct _D3DKMT_CREATEDEVICE
{
  union
  {
    D3DKMT_HANDLE hAdapter;
    VOID *pAdapter;
  } DUMMYUNIONNAME;
  D3DKMT_CREATEDEVICEFLAGS Flags;
  D3DKMT_HANDLE hDevice;
  VOID *pCommandBuffer;
  UINT CommandBufferSize;
  D3DDDI_ALLOCATIONLIST *pAllocationList;
  UINT AllocationListSize;
  D3DDDI_PATCHLOCATIONLIST *pPatchLocationList;
  UINT PatchLocationListSize;
} D3DKMT_CREATEDEVICE;

typedef struct _D3DKMT_DESTROYDEVICE
{
  D3DKMT_HANDLE hDevice;
} D3DKMT_DESTROYDEVICE;

typedef struct _D3DKMT_OPENADAPTERFROMLUID
{
  LUID AdapterLuid;
  D3DKMT_HANDLE hAdapter;
} D3DKMT_OPENADAPTERFROMLUID;

typedef struct _D3DKMT_CLOSEADAPTER
{
  D3DKMT_HANDLE hAdapter;
} D3DKMT_CLOSEADAPTER;

#if !defined(DXMT_NATIVE)

extern "C"
{
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTCreateDevice(D3DKMT_CREATEDEVICE *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER *desc);
}

#else

#include "log/log.hpp"

extern "C"
{
  inline NTSTATUS D3DKMTCreateDevice(D3DKMT_CREATEDEVICE *desc) {
    dxmt::Logger::warn("D3DKMTCreateDevice not implemented");
    return -1;
  }

  inline NTSTATUS D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *desc) {
    dxmt::Logger::warn("D3DKMTDestroyDevice not implemented.");
    return -1;
  }

  inline NTSTATUS D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER *desc) {
    dxmt::Logger::warn("D3DKMTCloseAdapter not implemented.");
    return -1;
  }

  inline NTSTATUS D3DKMTOpenAdapterFromLuid(const D3DKMT_OPENADAPTERFROMLUID *desc) {
    dxmt::Logger::warn("D3DKMTOpenAdapterFromLuid not implemented.");
    return -1;
  }
}

#endif
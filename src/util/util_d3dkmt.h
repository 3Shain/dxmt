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

typedef struct _D3DKMT_DESTROYALLOCATION
{
  D3DKMT_HANDLE hDevice;
  D3DKMT_HANDLE hResource;
  const D3DKMT_HANDLE *phAllocationList;
  UINT AllocationCount;
} D3DKMT_DESTROYALLOCATION;

typedef struct _D3DKMT_DESTROYDEVICE
{
  D3DKMT_HANDLE hDevice;
} D3DKMT_DESTROYDEVICE;

typedef struct _D3DKMT_DESTROYSYNCHRONIZATIONOBJECT
{
  D3DKMT_HANDLE hSyncObject;
} D3DKMT_DESTROYSYNCHRONIZATIONOBJECT;

typedef struct _D3DKMT_OPENADAPTERFROMLUID
{
  LUID AdapterLuid;
  D3DKMT_HANDLE hAdapter;
} D3DKMT_OPENADAPTERFROMLUID;

typedef enum _D3DDDI_SYNCHRONIZATIONOBJECT_TYPE
{
    D3DDDI_SYNCHRONIZATION_MUTEX = 1,
    D3DDDI_SEMAPHORE = 2,
    D3DDDI_FENCE = 3,
    D3DDDI_CPU_NOTIFICATION = 4,
    D3DDDI_MONITORED_FENCE = 5,
    D3DDDI_PERIODIC_MONITORED_FENCE = 6,
    D3DDDI_SYNCHRONIZATION_TYPE_LIMIT
} D3DDDI_SYNCHRONIZATIONOBJECT_TYPE;

typedef ULONGLONG D3DGPU_VIRTUAL_ADDRESS;

typedef UINT D3DDDI_VIDEO_PRESENT_TARGET_ID;

#ifndef D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS_EXT
#define D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS_EXT
#define D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS_RESERVED0 Reserved0
#endif

typedef struct _D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS
{
  union
  {
    struct
    {
      UINT Shared : 1;
      UINT NtSecuritySharing : 1;
      UINT CrossAdapter : 1;
      UINT TopOfPipeline : 1;
      UINT NoSignal : 1;
      UINT NoWait : 1;
      UINT NoSignalMaxValueOnTdr : 1;
      UINT NoGPUAccess : 1;
      UINT Reserved : 23;
      UINT D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS_RESERVED0 : 1;
    };
    UINT Value;
  };
} D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS;

typedef struct _D3DDDI_SYNCHRONIZATIONOBJECTINFO2
{
  D3DDDI_SYNCHRONIZATIONOBJECT_TYPE Type;
  D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS Flags;
  union
  {
    struct
    {
      BOOL InitialState;
    } SynchronizationMutex;
    struct
    {
      UINT MaxCount;
      UINT InitialCount;
    } Semaphore;
    struct
    {
      UINT64 FenceValue;
    } Fence;
    struct
    {
      HANDLE Event;
    } CPUNotification;
    struct
    {
      UINT64 InitialFenceValue;
      void *FenceValueCPUVirtualAddress;
      D3DGPU_VIRTUAL_ADDRESS FenceValueGPUVirtualAddress;
      UINT EngineAffinity;
    } MonitoredFence;
    struct
    {
      D3DKMT_HANDLE hAdapter;
      D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId;
      UINT64 Time;
      void *FenceValueCPUVirtualAddress;
      D3DGPU_VIRTUAL_ADDRESS FenceValueGPUVirtualAddress;
      UINT EngineAffinity;
    } PeriodicMonitoredFence;
    struct
    {
      UINT64 Reserved[8];
    } Reserved;
  };
  D3DKMT_HANDLE SharedHandle;
} D3DDDI_SYNCHRONIZATIONOBJECTINFO2;

typedef struct _D3DKMT_CREATESYNCHRONIZATIONOBJECT2
{
  D3DKMT_HANDLE hDevice;
  D3DDDI_SYNCHRONIZATIONOBJECTINFO2 Info;
  D3DKMT_HANDLE hSyncObject;
} D3DKMT_CREATESYNCHRONIZATIONOBJECT2;

typedef struct _D3DKMT_CREATESTANDARDALLOCATIONFLAGS
{
  union
  {
    struct
    {
      UINT Reserved : 32;
    };
    UINT Value;
  };
} D3DKMT_CREATESTANDARDALLOCATIONFLAGS;

typedef enum _D3DKMT_STANDARDALLOCATIONTYPE
{
  D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP = 1,
} D3DKMT_STANDARDALLOCATIONTYPE;

typedef struct _D3DKMT_STANDARDALLOCATION_EXISTINGHEAP
{
  SIZE_T Size;
} D3DKMT_STANDARDALLOCATION_EXISTINGHEAP;

typedef struct _D3DKMT_CREATESTANDARDALLOCATION
{
  D3DKMT_STANDARDALLOCATIONTYPE Type;
  union
  {
    D3DKMT_STANDARDALLOCATION_EXISTINGHEAP ExistingHeapData;
  };
  D3DKMT_CREATESTANDARDALLOCATIONFLAGS Flags;
} D3DKMT_CREATESTANDARDALLOCATION;

typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;

typedef struct _D3DDDI_ALLOCATIONINFO
{
  D3DKMT_HANDLE hAllocation;
  const void *pSystemMem;
  void *pPrivateDriverData;
  UINT PrivateDriverDataSize;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
  union
  {
    struct
    {
      UINT Primary : 1;
      UINT Stereo : 1;
      UINT Reserved : 30;
    };
    UINT Value;
  } Flags;
} D3DDDI_ALLOCATIONINFO;

typedef struct _D3DDDI_ALLOCATIONINFO2
{
  D3DKMT_HANDLE hAllocation;
  union
  {
    HANDLE hSection;
    const void *pSystemMem;
  };
  VOID *pPrivateDriverData;
  UINT PrivateDriverDataSize;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
  union
  {
    struct
    {
      UINT Primary : 1;
      UINT Stereo : 1;
      UINT OverridePriority : 1;
      UINT Reserved : 29;
    };
    UINT Value;
  } Flags;
  D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress;
  union
  {
    UINT Priority;
    ULONG_PTR Unused;
  };
  ULONG_PTR Reserved[5];
} D3DDDI_ALLOCATIONINFO2;

typedef struct _D3DKMT_CREATEALLOCATIONFLAGS
{
    UINT CreateResource : 1;
    UINT CreateShared : 1;
    UINT NonSecure : 1;
    UINT CreateProtected : 1;
    UINT RestrictSharedAccess : 1;
    UINT ExistingSysMem : 1;
    UINT NtSecuritySharing : 1;
    UINT ReadOnly : 1;
    UINT CreateWriteCombined : 1;
    UINT CreateCached : 1;
    UINT SwapChainBackBuffer : 1;
    UINT CrossAdapter : 1;
    UINT OpenCrossAdapter : 1;
    UINT PartialSharedCreation : 1;
    UINT Zeroed : 1;
    UINT WriteWatch : 1;
    UINT StandardAllocation : 1;
    UINT ExistingSection : 1;
    UINT AllowNotZeroed : 1;
    UINT PhysicallyContiguous : 1;
    UINT Reserved : 12;
} D3DKMT_CREATEALLOCATIONFLAGS;

typedef struct _D3DKMT_CREATEALLOCATION
{
  D3DKMT_HANDLE hDevice;
  D3DKMT_HANDLE hResource;
  D3DKMT_HANDLE hGlobalShare;
  const void *pPrivateRuntimeData;
  UINT PrivateRuntimeDataSize;
  union
  {
    D3DKMT_CREATESTANDARDALLOCATION *pStandardAllocation;
    const void *pPrivateDriverData;
  };
  UINT PrivateDriverDataSize;
  UINT NumAllocations;
  union
  {
    D3DDDI_ALLOCATIONINFO *pAllocationInfo;
    D3DDDI_ALLOCATIONINFO2 *pAllocationInfo2;
  };
  D3DKMT_CREATEALLOCATIONFLAGS Flags;
  HANDLE hPrivateRuntimeResourceHandle;
} D3DKMT_CREATEALLOCATION;

typedef struct _D3DKMT_CLOSEADAPTER
{
  D3DKMT_HANDLE hAdapter;
} D3DKMT_CLOSEADAPTER;

#if !defined(DXMT_NATIVE)

extern "C"
{
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTCreateDevice(D3DKMT_CREATEDEVICE *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTDestroyAllocation(const D3DKMT_DESTROYALLOCATION *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTDestroySynchronizationObject(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTCreateSynchronizationObject2(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTCreateAllocation2(D3DKMT_CREATEALLOCATION *desc);
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

  inline NTSTATUS D3DKMTDestroyAllocation(const D3DKMT_DESTROYALLOCATION *desc) {
    dxmt::Logger::warn("D3DKMTDestroyAllocation not implemented.");
    return -1;
  }

  inline NTSTATUS D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *desc) {
    dxmt::Logger::warn("D3DKMTDestroyDevice not implemented.");
    return -1;
  }

  inline NTSTATUS D3DKMTDestroySynchronizationObject(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *desc) {
    dxmt::Logger::warn("D3DKMTDestroySynchronizationObject not implemented.");
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

  inline NTSTATUS D3DKMTCreateSynchronizationObject2(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *desc) {
    dxmt::Logger::warn("D3DKMTCreateSynchronizationObject2 not implemented.");
    return -1;
  }

  inline NTSTATUS D3DKMTCreateAllocation2(D3DKMT_CREATEALLOCATION *desc) {
    dxmt::Logger::warn("D3DKMTCreateAllocation2 not implemented.");
    return -1;
  }
}

#endif
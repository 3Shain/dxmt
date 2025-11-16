#pragma once

#include <windows.h>

typedef LONG NTSTATUS;
typedef UINT D3DKMT_HANDLE;

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
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID *desc);
  DECLSPEC_IMPORT NTSTATUS WINAPI D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER *desc);
}

#else

#include "log/log.hpp"

extern "C"
{
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
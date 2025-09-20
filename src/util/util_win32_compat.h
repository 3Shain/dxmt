#pragma once

#if defined(DXMT_NATIVE)

#include <windows.h>

#include "log/log.hpp"

#define THREAD_PRIORITY_TIME_CRITICAL 15
#define FILE_MAP_ALL_ACCESS 0xF001F
#define PAGE_READWRITE 0x04

inline HANDLE GetCurrentProcess() {
  dxmt::Logger::warn("GetCurrentProcess not implemented.");
  return nullptr;
}

inline HANDLE GetCurrentThread() {
  dxmt::Logger::warn("GetCurrentThread not implemented.");
  return nullptr;
}

inline BOOL SetThreadPriority(HANDLE hThread, int nPriority) {
  dxmt::Logger::warn("SetThreadPriority not implemented.");
  return FALSE;
}

inline HANDLE GetModuleHandle(LPCSTR lpModuleName) {
  dxmt::Logger::warn("GetModuleHandle not implemented.");
  return nullptr;
}

inline void ExitThread(DWORD dwExitCode) {
  dxmt::Logger::warn("ExitThread not implemented.");
}

#define WM_SIZE 0

inline LONG SendMessage(HWND hWnd, UINT Msg, UINT_PTR wParam, LONG_PTR lParam) {
  dxmt::Logger::warn("SendMessage not implemented.");
  return 0;
}

inline DWORD GetProcessId(HANDLE Process) {
  dxmt::Logger::warn("GetProcessId not implemented.");
  return 0;
}

inline DWORD GetWindowThreadProcessId(HWND hWnd, LPDWORD lpdwProcessId) {
  dxmt::Logger::warn("GetWindowThreadProcessId not implemented.");
  *lpdwProcessId = 0;
  return 0;
}

inline DWORD MAKELONG(WORD a, WORD b) {
  return (((DWORD)a) << 16) | ((DWORD)b);
}

typedef SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;

inline HANDLE CreateSemaphore(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
                              LONG                  lInitialCount,
                              LONG                  lMaximumCount,
                              LPCSTR                lpName) {
  dxmt::Logger::warn("CreateSemaphore not implemented.");
  return nullptr;
}

inline BOOL ReleaseSemaphore(HANDLE hSemaphore,
                             LONG   lReleaseCount,
                             LPLONG lpPreviousCount) {
  dxmt::Logger::warn("ReleaseSemaphore not implemented.");
  return FALSE;
}

inline BOOL CloseHandle(HANDLE hObject) {
  dxmt::Logger::warn("CloseHandle not implemented.");
  return FALSE;
}

inline BOOL DuplicateHandle(HANDLE   hSourceProcessHandle,
                            HANDLE   hSourceHandle,
                            HANDLE   hTargetProcessHandle,
                            LPHANDLE lpTargetHandle,
                            DWORD    dwDesiredAccess,
                            BOOL     bInheritHandle,
                            DWORD    dwOptions)
{
  dxmt::Logger::warn("DuplicateHandle not implemented.");
  return FALSE;
}

inline HANDLE MapViewOfFile(HANDLE  hFileMappingObject,
                            DWORD   dwDesiredAccess,
                            DWORD   dwFileOffsetHigh,
                            DWORD   dwFileOffsetLow,
                            SIZE_T dwNumberOfBytesToMap)
{
  dxmt::Logger::warn("MapViewOfFile not implemented.");
  return nullptr;
}

inline BOOL UnmapViewOfFile(LPCVOID lpBaseAddress) {
  dxmt::Logger::warn("UnmapViewOfFile not implemented.");
  return false;
}

inline HANDLE CreateFileMapping(HANDLE                hFile,
                                LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                                DWORD                 flProtect,
                                DWORD                 dwMaximumSizeHigh,
                                DWORD                 dwMaximumSizeLow,
                                LPCWSTR               lpName)
{
  dxmt::Logger::warn("CreateFileMapping not implemented.");
  return nullptr;
}

inline DWORD GetLastError(void) {
  dxmt::Logger::warn("GetLastError not implemented.");
  return 0;
}


#define ARRAYSIZE(a) (sizeof(a)/sizeof(*(a)))

#endif
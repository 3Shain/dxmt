#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <windows.h>

static inline void DXMTD3D12Trace(const char *component, const char *fmt, ...) {
  FILE *f = fopen("Z:\\tmp\\dxmt_d3d12_trace.log", "a");
  if (!f)
    return;

  fprintf(f, "[pid=%lu tid=%lu] %s: ", GetCurrentProcessId(),
          GetCurrentThreadId(), component ? component : "d3d12");

  va_list args;
  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);

  fputc('\n', f);
  fclose(f);
}

static inline uint64_t DXMTD3D12Hash64(const void *data, size_t size) {
  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  uint64_t hash = 1469598103934665603ull;
  for (size_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

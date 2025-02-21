#pragma once

struct ID3D11Resource;
struct ID3D12Resource;

#define NVNGX_API extern "C"

enum NVNGX_RESULT : unsigned int {
  NVNGX_RESULT_OK = 1,
  NVNGX_RESULT_FAIL = 0xbad0000,
  NVNGX_RESULT_INVALID_PARAMETER = 0xbad0005,
};

class NVNGXParameter { // vtable seems to be inconsistent?
public:
  virtual void Set(const char *name, void *value) = 0;
  virtual void Set(const char *name, ID3D12Resource *value) = 0;
  virtual void Set(const char *name, ID3D11Resource *value) = 0;
  virtual void Set(const char *name, int value) = 0;
  virtual void Set(const char *name, unsigned int value) = 0;
  virtual void Set(const char *name, double value) = 0;
  virtual void Set(const char *name, float value) = 0;
  virtual void Set(const char *name, unsigned long long value) = 0;

  virtual NVNGX_RESULT Get(const char *name, void **out) const = 0;
  virtual NVNGX_RESULT Get(const char *name, ID3D12Resource **out) const = 0;
  virtual NVNGX_RESULT Get(const char *name, double *out) const = 0;
  virtual NVNGX_RESULT Get(const char *name, unsigned int *out) const = 0;
  virtual NVNGX_RESULT Get(const char *name, int *out) const = 0;
  virtual NVNGX_RESULT Get(const char *name, ID3D11Resource **out) const = 0;
  virtual NVNGX_RESULT Get(const char *name, float *out) const = 0;
  virtual NVNGX_RESULT Get(const char *name, unsigned long long *out) const = 0;

  virtual void Reset() = 0;
};

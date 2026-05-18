#define INITGUID
#include "d3d12_dxgi_device.hpp"
#include "d3d12_device.hpp"
#include "d3d12_trace.hpp"
#include "com/com_pointer.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <d3d12.h>
#include <atomic>
#include <cstdarg>
#include <exception>
#include <vector>
#include <cstring>
#include <map>

namespace {

constexpr UINT kD3D12AgilitySDKVersion = 620;

constexpr GUID kCLSID_D3D12SDKConfiguration = {
    0x7cda6aca,
    0xa03e,
    0x49c8,
    {0x94, 0x58, 0x03, 0x34, 0xd2, 0x0e, 0x07, 0xce}};
constexpr GUID kIID_ID3D12SDKConfiguration = {
    0xe9eb5314,
    0x33aa,
    0x42b2,
    {0xa7, 0x18, 0xd7, 0x7f, 0x58, 0xb1, 0xf1, 0xc7}};
constexpr GUID kIID_ID3D12SDKConfiguration1 = {
    0x8aaf9303,
    0xad25,
    0x48b9,
    {0x9a, 0x57, 0xd9, 0xc3, 0x7e, 0x00, 0x9d, 0x9f}};
constexpr GUID kCLSID_D3D12DeviceFactory = {
    0x114863bf,
    0xc386,
    0x4aee,
    {0xb3, 0x9d, 0x8f, 0x0b, 0xbb, 0x06, 0x29, 0x55}};
constexpr GUID kIID_ID3D12DeviceFactory = {
    0x61f307d3,
    0xd34e,
    0x4e7c,
    {0x83, 0x74, 0x3b, 0xa4, 0xde, 0x23, 0xcc, 0xcb}};
constexpr GUID kIID_ID3D12DeviceConfiguration = {
    0x78dbf87b,
    0xf766,
    0x422b,
    {0xa6, 0x1c, 0xc8, 0xc4, 0x46, 0xbd, 0xb9, 0xad}};
constexpr GUID kIID_ID3D12DeviceConfiguration1 = {
    0xed342442,
    0x6343,
    0x4e16,
    {0xbb, 0x82, 0xa3, 0xa5, 0x77, 0x87, 0x4e, 0x56}};
constexpr GUID kCLSID_D3D12StateObjectFactory = {
    0x54e1c9f3,
    0x1303,
    0x4112,
    {0xbf, 0x8e, 0x7b, 0xf2, 0xbb, 0x60, 0x6a, 0x73}};
constexpr GUID kCLSID_D3D12RuntimeValidationControl = {
    0xe5b53e74,
    0x3fca,
    0x47b4,
    {0x88, 0xb9, 0xa8, 0xb4, 0x1e, 0xf8, 0xfb, 0x73}};
constexpr GUID kCLSID_D3D12ApplicationIdentity = {
    0x08d8e1e8,
    0x75a6,
    0x42a7,
    {0xbf, 0x3a, 0xd0, 0x5f, 0xe5, 0x29, 0xc4, 0x7c}};
constexpr GUID kIID_ID3D12StateObjectDatabase = {
    0xc56060b7,
    0xb5fc,
    0x4135,
    {0x98, 0xe0, 0xa1, 0xe9, 0x99, 0x7e, 0xac, 0xe0}};
constexpr GUID kIID_ID3D12StateObjectDatabaseFactory = {
    0xf5b066f0,
    0x648a,
    0x4611,
    {0xbd, 0x41, 0x27, 0xfd, 0x09, 0x48, 0xb9, 0xeb}};
constexpr GUID kIID_ID3D12RuntimeValidationControl = {
    0xc706c811,
    0x3663,
    0x4bf1,
    {0x91, 0xb9, 0x1e, 0x8a, 0x7c, 0x11, 0x4a, 0xb9}};
constexpr GUID kIID_ID3D12ApplicationIdentity = {
    0x82dc6c85,
    0x727b,
    0x4a8d,
    {0x91, 0x69, 0xdb, 0x6c, 0xe3, 0xe9, 0x75, 0xa0}};

struct ID3D12SDKConfiguration : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SetSDKVersion(UINT SDKVersion,
                                                  LPCSTR SDKPath) = 0;
};

struct ID3D12SDKConfiguration1 : public ID3D12SDKConfiguration {
  virtual HRESULT STDMETHODCALLTYPE CreateDeviceFactory(UINT SDKVersion,
                                                        LPCSTR SDKPath,
                                                        REFIID riid,
                                                        void **ppvFactory) = 0;
  virtual void STDMETHODCALLTYPE FreeUnusedSDKs() = 0;
};

enum D3D12DeviceFactoryFlagsCompat : UINT {
  D3D12DeviceFactoryFlagNone = 0,
  D3D12DeviceFactoryFlagAllowReturningExistingDevice = 1,
  D3D12DeviceFactoryFlagAllowReturningIncompatibleExistingDevice = 2,
  D3D12DeviceFactoryFlagDisallowStoringNewDeviceAsSingleton = 4,
};

struct ID3D12DeviceFactoryCompat : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE InitializeFromGlobalState() = 0;
  virtual HRESULT STDMETHODCALLTYPE ApplyToGlobalState() = 0;
  virtual HRESULT STDMETHODCALLTYPE SetFlags(D3D12DeviceFactoryFlagsCompat flags) = 0;
  virtual D3D12DeviceFactoryFlagsCompat STDMETHODCALLTYPE GetFlags() = 0;
  virtual HRESULT STDMETHODCALLTYPE GetConfigurationInterface(
      REFCLSID clsid, REFIID iid, void **ppv) = 0;
  virtual HRESULT STDMETHODCALLTYPE EnableExperimentalFeatures(
      UINT num_features, const IID *iids, void *configuration_structs,
      UINT *configuration_struct_sizes) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateDevice(IUnknown *adapter,
                                                D3D_FEATURE_LEVEL feature_level,
                                                REFIID riid,
                                                void **device) = 0;
};

enum D3D12DeviceFlagsCompat : UINT {
  D3D12DeviceFlagNone = 0,
  D3D12DeviceFlagDebugLayerEnabled = 1,
  D3D12DeviceFlagGPUBasedValidationEnabled = 2,
  D3D12DeviceFlagSynchronizedCommandQueueValidationDisabled = 4,
  D3D12DeviceFlagDREDAutoBreadcrumbsEnabled = 8,
  D3D12DeviceFlagDREDPageFaultReportingEnabled = 16,
  D3D12DeviceFlagDREDWatsonReportingEnabled = 32,
  D3D12DeviceFlagDREDBreadcrumbContextEnabled = 64,
  D3D12DeviceFlagDREDUseMarkersOnlyBreadcrumbs = 128,
  D3D12DeviceFlagShaderInstrumentationEnabled = 256,
  D3D12DeviceFlagAutoDebugNameEnabled = 512,
  D3D12DeviceFlagForceLegacyStateValidation = 1024,
};

struct D3D12DeviceConfigurationDescCompat {
  D3D12DeviceFlagsCompat Flags;
  UINT GpuBasedValidationFlags;
  UINT SDKVersion;
  UINT NumEnabledExperimentalFeatures;
};

struct ID3D12DeviceConfigurationCompat : public IUnknown {
  virtual D3D12DeviceConfigurationDescCompat *STDMETHODCALLTYPE GetDesc(
      D3D12DeviceConfigurationDescCompat *ret) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetEnabledExperimentalFeatures(
      GUID *guids, UINT num_guids) = 0;
  virtual HRESULT STDMETHODCALLTYPE SerializeVersionedRootSignature(
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *desc, ID3DBlob **result,
      ID3DBlob **error) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateVersionedRootSignatureDeserializer(
      const void *blob, SIZE_T size, REFIID riid, void **deserializer) = 0;
};

struct ID3D12DeviceConfiguration1Compat
    : public ID3D12DeviceConfigurationCompat {
  virtual HRESULT STDMETHODCALLTYPE CreateVersionedRootSignatureDeserializerFromSubobjectInLibrary(
      const void *library_blob, SIZE_T size, LPCWSTR root_signature_subobject_name,
      REFIID riid, void **deserializer) = 0;
};

struct ID3D12RuntimeValidationControlCompat : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE DisableFailuresFromStricterValidationInAppLocalRuntime(
      BOOL disable) = 0;
  virtual BOOL STDMETHODCALLTYPE FailuresFromStricterValidationInAppLocalRuntimeDisabled() = 0;
};

union D3D12VersionNumberCompat {
  UINT64 Version;
  UINT16 VersionParts[4];
};

struct D3D12ApplicationDescCompat {
  LPCWSTR pExeFilename;
  LPCWSTR pName;
  D3D12VersionNumberCompat Version;
  LPCWSTR pEngineName;
  D3D12VersionNumberCompat EngineVersion;
};

struct ID3D12ApplicationIdentityCompat : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SetApplicationIdentity(
      const D3D12ApplicationDescCompat *desc, REFGUID app_id) = 0;
};

typedef void(STDMETHODCALLTYPE *D3D12ApplicationDescFuncCompat)(
    const D3D12ApplicationDescCompat *application_desc, void *context);
typedef void(STDMETHODCALLTYPE *D3D12PipelineStateFuncCompat)(
    const void *key, UINT key_size, UINT version,
    const D3D12_PIPELINE_STATE_STREAM_DESC *desc, void *context);
typedef void(STDMETHODCALLTYPE *D3D12StateObjectFuncCompat)(
    const void *key, UINT key_size, UINT version,
    const D3D12_STATE_OBJECT_DESC *desc, const void *parent_key,
    UINT parent_key_size, void *context);

enum D3D12StateObjectDatabaseFlagsCompat : UINT {
  D3D12StateObjectDatabaseFlagNone = 0,
  D3D12StateObjectDatabaseFlagReadOnly = 1,
};

struct ID3D12StateObjectDatabaseCompat : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SetApplicationDesc(
      const D3D12ApplicationDescCompat *application_desc) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetApplicationDesc(
      D3D12ApplicationDescFuncCompat callback, void *context) = 0;
  virtual HRESULT STDMETHODCALLTYPE StorePipelineStateDesc(
      const void *key, UINT key_size, UINT version,
      const D3D12_PIPELINE_STATE_STREAM_DESC *desc) = 0;
  virtual HRESULT STDMETHODCALLTYPE FindPipelineStateDesc(
      const void *key, UINT key_size, D3D12PipelineStateFuncCompat callback,
      void *context) = 0;
  virtual HRESULT STDMETHODCALLTYPE StoreStateObjectDesc(
      const void *key, UINT key_size, UINT version,
      const D3D12_STATE_OBJECT_DESC *desc, const void *parent_key,
      UINT parent_key_size) = 0;
  virtual HRESULT STDMETHODCALLTYPE FindStateObjectDesc(
      const void *key, UINT key_size, D3D12StateObjectFuncCompat callback,
      void *context) = 0;
  virtual HRESULT STDMETHODCALLTYPE FindObjectVersion(const void *key,
                                                      UINT key_size,
                                                      UINT *version) = 0;
};

struct ID3D12StateObjectDatabaseFactoryCompat : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE CreateStateObjectDatabaseFromFile(
      LPCWSTR database_file, D3D12StateObjectDatabaseFlagsCompat flags,
      REFIID riid, void **state_object_database) = 0;
};

static void TraceAgility(const char *fmt, ...) {
  FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
  if (!f)
    return;

  va_list args;
  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);
  fprintf(f, "\n");
  fclose(f);
}

class MTLD3D12SDKConfiguration final : public ID3D12SDKConfiguration1 {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;

    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12SDKConfiguration ||
        riid == kIID_ID3D12SDKConfiguration1) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  HRESULT STDMETHODCALLTYPE SetSDKVersion(UINT SDKVersion,
                                          LPCSTR SDKPath) override {
    TraceAgility("ID3D12SDKConfiguration::SetSDKVersion version=%u path=%s "
                 "accepted_runtime=%u",
                 SDKVersion, SDKPath ? SDKPath : "(null)",
                 kD3D12AgilitySDKVersion);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE CreateDeviceFactory(UINT SDKVersion, LPCSTR SDKPath,
                                                REFIID riid,
                                                void **ppvFactory) override;

  void STDMETHODCALLTYPE FreeUnusedSDKs() override {
    TraceAgility("ID3D12SDKConfiguration1::FreeUnusedSDKs");
  }

private:
  std::atomic<ULONG> m_ref = {1};
};

class MTLD3D12DeviceFactory final : public ID3D12DeviceFactoryCompat {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12DeviceFactory) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  HRESULT STDMETHODCALLTYPE InitializeFromGlobalState() override {
    TraceAgility("DeviceFactory::InitializeFromGlobalState");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ApplyToGlobalState() override {
    TraceAgility("DeviceFactory::ApplyToGlobalState");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetFlags(D3D12DeviceFactoryFlagsCompat flags) override {
    m_flags = flags;
    TraceAgility("DeviceFactory::SetFlags flags=0x%x", flags);
    return S_OK;
  }

  D3D12DeviceFactoryFlagsCompat STDMETHODCALLTYPE GetFlags() override {
    TraceAgility("DeviceFactory::GetFlags -> 0x%x", m_flags);
    return m_flags;
  }

  HRESULT STDMETHODCALLTYPE GetConfigurationInterface(
      REFCLSID clsid, REFIID iid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    TraceAgility("DeviceFactory::GetConfigurationInterface clsid=%s iid=%s",
                 dxmt::str::format(clsid).c_str(),
                 dxmt::str::format(iid).c_str());
    if (iid == kIID_ID3D12DeviceConfiguration ||
        iid == kIID_ID3D12DeviceConfiguration1) {
      extern HRESULT CreateD3D12DeviceConfiguration(REFIID riid, void **ppv);
      return CreateD3D12DeviceConfiguration(iid, ppv);
    }
    return D3D12GetInterface(clsid, iid, ppv);
  }

  HRESULT STDMETHODCALLTYPE EnableExperimentalFeatures(
      UINT num_features, const IID *iids, void *, UINT *) override {
    if (num_features && !iids)
      return E_INVALIDARG;
    TraceAgility("DeviceFactory::EnableExperimentalFeatures count=%u",
                 num_features);
    for (UINT i = 0; i < num_features; i++) {
      TraceAgility("  feature[%u]=%s", i,
                   dxmt::str::format(iids[i]).c_str());
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE CreateDevice(IUnknown *adapter,
                                         D3D_FEATURE_LEVEL feature_level,
                                         REFIID riid, void **device) override {
    TraceAgility("DeviceFactory::CreateDevice adapter=%p FL=%d riid=%s",
                 adapter, feature_level, dxmt::str::format(riid).c_str());
    return D3D12CreateDevice(adapter, feature_level, riid, device);
  }

private:
  std::atomic<ULONG> m_ref = {1};
  D3D12DeviceFactoryFlagsCompat m_flags = D3D12DeviceFactoryFlagNone;
};

class MTLD3D12DeviceConfiguration final
    : public ID3D12DeviceConfiguration1Compat {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12DeviceConfiguration ||
        riid == kIID_ID3D12DeviceConfiguration1) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  D3D12DeviceConfigurationDescCompat *STDMETHODCALLTYPE GetDesc(
      D3D12DeviceConfigurationDescCompat *ret) override {
    if (!ret)
      return nullptr;
    ret->Flags = D3D12DeviceFlagNone;
    ret->GpuBasedValidationFlags = 0;
    ret->SDKVersion = kD3D12AgilitySDKVersion;
    ret->NumEnabledExperimentalFeatures = 0;
    TraceAgility("DeviceConfiguration::GetDesc sdk=%u",
                 kD3D12AgilitySDKVersion);
    return ret;
  }

  HRESULT STDMETHODCALLTYPE GetEnabledExperimentalFeatures(
      GUID *guids, UINT num_guids) override {
    if (num_guids && !guids)
      return E_POINTER;
    TraceAgility("DeviceConfiguration::GetEnabledExperimentalFeatures count=%u -> S_OK",
                 num_guids);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SerializeVersionedRootSignature(
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *desc, ID3DBlob **result,
      ID3DBlob **error) override {
    TraceAgility("DeviceConfiguration::SerializeVersionedRootSignature version=%u",
                 desc ? desc->Version : 0);
    return D3D12SerializeVersionedRootSignature(desc, result, error);
  }

  HRESULT STDMETHODCALLTYPE CreateVersionedRootSignatureDeserializer(
      const void *blob, SIZE_T size, REFIID riid,
      void **deserializer) override {
    TraceAgility("DeviceConfiguration::CreateVersionedRootSignatureDeserializer size=%zu riid=%s",
                 size, dxmt::str::format(riid).c_str());
    return D3D12CreateVersionedRootSignatureDeserializer(blob, size, riid,
                                                         deserializer);
  }

  HRESULT STDMETHODCALLTYPE CreateVersionedRootSignatureDeserializerFromSubobjectInLibrary(
      const void *library_blob, SIZE_T size,
      LPCWSTR root_signature_subobject_name, REFIID riid,
      void **deserializer) override {
    TraceAgility("DeviceConfiguration::CreateVersionedRootSignatureDeserializerFromSubobjectInLibrary size=%zu subobject=%ls riid=%s",
                 size,
                 root_signature_subobject_name ? root_signature_subobject_name
                                               : L"(null)",
                 dxmt::str::format(riid).c_str());
    return D3D12CreateVersionedRootSignatureDeserializer(
        library_blob, size, riid, deserializer);
  }

private:
  std::atomic<ULONG> m_ref = {1};
};

HRESULT CreateD3D12DeviceConfiguration(REFIID riid, void **ppv) {
  if (!ppv)
    return E_POINTER;
  *ppv = nullptr;
  auto *configuration = new MTLD3D12DeviceConfiguration();
  HRESULT hr = configuration->QueryInterface(riid, ppv);
  configuration->Release();
  TraceAgility("CreateD3D12DeviceConfiguration riid=%s -> 0x%lx out=%p",
               dxmt::str::format(riid).c_str(), hr, ppv ? *ppv : nullptr);
  return hr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12SDKConfiguration::CreateDeviceFactory(
    UINT SDKVersion, LPCSTR SDKPath, REFIID riid, void **ppvFactory) {
  if (!ppvFactory)
    return E_POINTER;

  *ppvFactory = nullptr;
  auto *factory = new MTLD3D12DeviceFactory();
  HRESULT hr = factory->QueryInterface(riid, ppvFactory);
  factory->Release();
  TraceAgility("ID3D12SDKConfiguration1::CreateDeviceFactory version=%u "
               "path=%s riid=%s -> 0x%lx out=%p",
               SDKVersion, SDKPath ? SDKPath : "(null)",
               dxmt::str::format(riid).c_str(), hr,
               ppvFactory ? *ppvFactory : nullptr);
  return hr;
}

class MTLD3D12StateObjectDatabase final
    : public ID3D12StateObjectDatabaseCompat {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12StateObjectDatabase) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  HRESULT STDMETHODCALLTYPE SetApplicationDesc(
      const D3D12ApplicationDescCompat *application_desc) override {
    if (!application_desc)
      return E_INVALIDARG;
    m_application_desc = *application_desc;
    m_has_application_desc = true;
    TraceAgility("StateObjectDatabase::SetApplicationDesc name=%ls engine=%ls",
                 m_application_desc.pName ? m_application_desc.pName : L"(null)",
                 m_application_desc.pEngineName
                     ? m_application_desc.pEngineName
                     : L"(null)");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetApplicationDesc(
      D3D12ApplicationDescFuncCompat callback, void *context) override {
    if (!callback)
      return E_POINTER;
    if (!m_has_application_desc)
      return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    callback(&m_application_desc, context);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE StorePipelineStateDesc(
      const void *key, UINT key_size, UINT version,
      const D3D12_PIPELINE_STATE_STREAM_DESC *desc) override {
    if (!key || !key_size || !desc)
      return E_INVALIDARG;
    m_pipeline_versions[MakeKey(key, key_size)] = version;
    TraceAgility("StateObjectDatabase::StorePipelineStateDesc key_size=%u version=%u bytes=%zu",
                 key_size, version, desc->SizeInBytes);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE FindPipelineStateDesc(
      const void *key, UINT key_size, D3D12PipelineStateFuncCompat, void *) override {
    TraceAgility("StateObjectDatabase::FindPipelineStateDesc key_size=%u -> not materialized",
                 key_size);
    if (!key || !key_size)
      return E_INVALIDARG;
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  HRESULT STDMETHODCALLTYPE StoreStateObjectDesc(
      const void *key, UINT key_size, UINT version,
      const D3D12_STATE_OBJECT_DESC *desc, const void *, UINT) override {
    if (!key || !key_size || !desc)
      return E_INVALIDARG;
    m_state_object_versions[MakeKey(key, key_size)] = version;
    TraceAgility("StateObjectDatabase::StoreStateObjectDesc key_size=%u version=%u subobjects=%u",
                 key_size, version, desc->NumSubobjects);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE FindStateObjectDesc(
      const void *key, UINT key_size, D3D12StateObjectFuncCompat, void *) override {
    TraceAgility("StateObjectDatabase::FindStateObjectDesc key_size=%u -> not materialized",
                 key_size);
    if (!key || !key_size)
      return E_INVALIDARG;
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  HRESULT STDMETHODCALLTYPE FindObjectVersion(const void *key, UINT key_size,
                                              UINT *version) override {
    if (!key || !key_size || !version)
      return E_INVALIDARG;
    auto key_bytes = MakeKey(key, key_size);
    auto pipeline = m_pipeline_versions.find(key_bytes);
    if (pipeline != m_pipeline_versions.end()) {
      *version = pipeline->second;
      return S_OK;
    }
    auto state_object = m_state_object_versions.find(key_bytes);
    if (state_object != m_state_object_versions.end()) {
      *version = state_object->second;
      return S_OK;
    }
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

private:
  static std::vector<uint8_t> MakeKey(const void *key, UINT key_size) {
    auto *bytes = static_cast<const uint8_t *>(key);
    return std::vector<uint8_t>(bytes, bytes + key_size);
  }

  std::atomic<ULONG> m_ref = {1};
  bool m_has_application_desc = false;
  D3D12ApplicationDescCompat m_application_desc = {};
  std::map<std::vector<uint8_t>, UINT> m_pipeline_versions;
  std::map<std::vector<uint8_t>, UINT> m_state_object_versions;
};

class MTLD3D12StateObjectDatabaseFactory final
    : public ID3D12StateObjectDatabaseFactoryCompat {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12StateObjectDatabaseFactory) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  HRESULT STDMETHODCALLTYPE CreateStateObjectDatabaseFromFile(
      LPCWSTR database_file, D3D12StateObjectDatabaseFlagsCompat flags,
      REFIID riid, void **state_object_database) override {
    if (!state_object_database)
      return E_POINTER;
    *state_object_database = nullptr;
    TraceAgility("StateObjectDatabaseFactory::CreateStateObjectDatabaseFromFile file=%ls flags=0x%x riid=%s",
                 database_file ? database_file : L"(null)", flags,
                 dxmt::str::format(riid).c_str());
    auto *database = new MTLD3D12StateObjectDatabase();
    HRESULT hr = database->QueryInterface(riid, state_object_database);
    database->Release();
    return hr;
  }

private:
  std::atomic<ULONG> m_ref = {1};
};

class MTLD3D12RuntimeValidationControl final
    : public ID3D12RuntimeValidationControlCompat {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12RuntimeValidationControl) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  HRESULT STDMETHODCALLTYPE DisableFailuresFromStricterValidationInAppLocalRuntime(
      BOOL disable) override {
    m_disabled = disable;
    TraceAgility("RuntimeValidationControl::DisableFailuresFromStricterValidationInAppLocalRuntime disabled=%d",
                 disable);
    return S_OK;
  }

  BOOL STDMETHODCALLTYPE FailuresFromStricterValidationInAppLocalRuntimeDisabled() override {
    TraceAgility("RuntimeValidationControl::FailuresFromStricterValidationInAppLocalRuntimeDisabled -> %d",
                 m_disabled);
    return m_disabled;
  }

private:
  std::atomic<ULONG> m_ref = {1};
  BOOL m_disabled = FALSE;
};

class MTLD3D12ApplicationIdentity final
    : public ID3D12ApplicationIdentityCompat {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12ApplicationIdentity) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  HRESULT STDMETHODCALLTYPE SetApplicationIdentity(
      const D3D12ApplicationDescCompat *desc, REFGUID app_id) override {
    if (!desc)
      return E_INVALIDARG;
    TraceAgility("ApplicationIdentity::SetApplicationIdentity name=%ls engine=%ls app_id=%s",
                 desc->pName ? desc->pName : L"(null)",
                 desc->pEngineName ? desc->pEngineName : L"(null)",
                 dxmt::str::format(app_id).c_str());
    return S_OK;
  }

private:
  std::atomic<ULONG> m_ref = {1};
};

} // namespace

#pragma pack(push, 1)
struct _RSHeader {
  uint32_t num_parameters;
  uint32_t num_static_samplers;
  uint32_t flags;
};
struct _RSParameter {
  uint8_t type;
  uint8_t visibility;
  union {
    struct {
      uint32_t register_space;
      uint32_t register_index;
      uint32_t num_32bit_values;
    } constants;
    struct {
      uint32_t register_space;
      uint32_t register_index;
    } descriptor;
    struct {
      uint32_t num_ranges;
    } table;
  };
};
struct _RSDescriptorRange {
  uint8_t range_type;
  uint32_t num_descriptors;
  uint32_t base_register;
  uint32_t register_space;
  uint32_t offset_in_table;
};
struct _RSStaticSampler {
  uint32_t filter;
  uint32_t address_u;
  uint32_t address_v;
  uint32_t address_w;
  float mip_lod_bias;
  uint32_t max_anisotropy;
  uint32_t comparison_func;
  uint32_t border_color;
  float min_lod;
  float max_lod;
  uint32_t register_space;
  uint32_t register_index;
  uint32_t shader_register_space;
  uint8_t shader_visibility;
};

struct _DXContainerHeader {
  uint8_t magic[4];
  uint8_t digest[16];
  uint16_t major;
  uint16_t minor;
  uint32_t file_size;
  uint32_t part_count;
};

struct _DXContainerPartHeader {
  uint8_t name[4];
  uint32_t size;
};

struct _DXRootSignatureHeader {
  uint32_t version;
  uint32_t num_parameters;
  uint32_t parameters_offset;
  uint32_t num_static_samplers;
  uint32_t static_sampler_offset;
  uint32_t flags;
};

struct _DXRootParameterHeader {
  uint32_t parameter_type;
  uint32_t shader_visibility;
  uint32_t parameter_offset;
};

struct _DXRootConstants {
  uint32_t shader_register;
  uint32_t register_space;
  uint32_t num_32bit_values;
};

struct _DXRootDescriptor10 {
  uint32_t shader_register;
  uint32_t register_space;
};

struct _DXRootDescriptor11 {
  uint32_t shader_register;
  uint32_t register_space;
  uint32_t flags;
};

struct _DXDescriptorTable {
  uint32_t num_ranges;
  uint32_t ranges_offset;
};

struct _DXDescriptorRange10 {
  uint32_t range_type;
  uint32_t num_descriptors;
  uint32_t base_shader_register;
  uint32_t register_space;
  uint32_t offset_in_table;
};

struct _DXDescriptorRange11 {
  uint32_t range_type;
  uint32_t num_descriptors;
  uint32_t base_shader_register;
  uint32_t register_space;
  uint32_t flags;
  uint32_t offset_in_table;
};

struct _DXStaticSampler {
  uint32_t filter;
  uint32_t address_u;
  uint32_t address_v;
  uint32_t address_w;
  float mip_lod_bias;
  uint32_t max_anisotropy;
  uint32_t comparison_func;
  uint32_t border_color;
  float min_lod;
  float max_lod;
  uint32_t shader_register;
  uint32_t register_space;
  uint32_t shader_visibility;
};
#pragma pack(pop)

static bool _RSRangeContains(size_t size, uint32_t offset, size_t bytes) {
  return offset <= size && bytes <= size - offset;
}

class _RSBlob : public ID3DBlob {
  ULONG m_ref = 1;
  std::vector<uint8_t> m_data;

public:
  _RSBlob(std::vector<uint8_t> &&data) : m_data(std::move(data)) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) {
    if (riid == IID_IUnknown || riid == IID_ID3D10Blob ||
        riid == __uuidof(ID3DBlob)) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() { return ++m_ref; }
  ULONG STDMETHODCALLTYPE Release() {
    ULONG r = --m_ref;
    if (!r)
      delete this;
    return r;
  }
  LPVOID STDMETHODCALLTYPE GetBufferPointer() { return m_data.data(); }
  SIZE_T STDMETHODCALLTYPE GetBufferSize() { return m_data.size(); }
};

class _RSDeserializer final : public ID3D12RootSignatureDeserializer,
                              public ID3D12VersionedRootSignatureDeserializer {
public:
  _RSDeserializer(const void *data, SIZE_T size) {
    m_valid = Parse(data, size);
  }

  bool Valid() const { return m_valid; }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == IID_ID3D12RootSignatureDeserializer) {
      *ppv = static_cast<ID3D12RootSignatureDeserializer *>(this);
      AddRef();
      return S_OK;
    }
    if (riid == IID_ID3D12VersionedRootSignatureDeserializer) {
      *ppv = static_cast<ID3D12VersionedRootSignatureDeserializer *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG r = --m_ref;
    if (!r)
      delete this;
    return r;
  }

  const D3D12_ROOT_SIGNATURE_DESC *STDMETHODCALLTYPE
  GetRootSignatureDesc() override {
    return &m_desc;
  }

  HRESULT STDMETHODCALLTYPE GetRootSignatureDescAtVersion(
      D3D_ROOT_SIGNATURE_VERSION version,
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC **desc) override {
    if (!desc)
      return E_POINTER;
    *desc = nullptr;

    if (version == D3D_ROOT_SIGNATURE_VERSION_1_0) {
      *desc = &m_versioned_desc0;
      return S_OK;
    }

    if (version == D3D_ROOT_SIGNATURE_VERSION_1_1) {
      *desc = &m_versioned_desc1;
      return S_OK;
    }

    return E_INVALIDARG;
  }

  const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *STDMETHODCALLTYPE
  GetUnconvertedRootSignatureDesc() override {
    return m_unconverted_version == D3D_ROOT_SIGNATURE_VERSION_1_1
               ? &m_versioned_desc1
               : &m_versioned_desc0;
  }

private:
  void FinalizeDescs(UINT flags, D3D_ROOT_SIGNATURE_VERSION original_version) {
    m_desc.NumParameters = static_cast<UINT>(m_params0.size());
    m_desc.pParameters = m_params0.empty() ? nullptr : m_params0.data();
    m_desc.NumStaticSamplers = static_cast<UINT>(m_static_samplers.size());
    m_desc.pStaticSamplers =
        m_static_samplers.empty() ? nullptr : m_static_samplers.data();
    m_desc.Flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(flags);

    m_desc1.NumParameters = static_cast<UINT>(m_params1.size());
    m_desc1.pParameters = m_params1.empty() ? nullptr : m_params1.data();
    m_desc1.NumStaticSamplers = static_cast<UINT>(m_static_samplers.size());
    m_desc1.pStaticSamplers =
        m_static_samplers.empty() ? nullptr : m_static_samplers.data();
    m_desc1.Flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(flags);

    m_versioned_desc0.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
    m_versioned_desc0.Desc_1_0 = m_desc;
    m_versioned_desc1.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    m_versioned_desc1.Desc_1_1 = m_desc1;
    m_unconverted_version = original_version;
  }

  bool ParseRTS0(const uint8_t *data, SIZE_T size) {
    if (!data || size < sizeof(_DXRootSignatureHeader))
      return false;

    auto *hdr = reinterpret_cast<const _DXRootSignatureHeader *>(data);
    if ((hdr->version != D3D_ROOT_SIGNATURE_VERSION_1_0 &&
         hdr->version != D3D_ROOT_SIGNATURE_VERSION_1_1) ||
        hdr->num_parameters > 64 || hdr->num_static_samplers > 64 ||
        !_RSRangeContains(size, hdr->parameters_offset,
                          hdr->num_parameters * sizeof(_DXRootParameterHeader)))
      return false;

    if (hdr->num_static_samplers > 0 &&
        !_RSRangeContains(size, hdr->static_sampler_offset,
                          hdr->num_static_samplers * sizeof(_DXStaticSampler)))
      return false;

    m_params0.clear();
    m_ranges0.clear();
    m_params1.clear();
    m_ranges1.clear();
    m_static_samplers.clear();
    m_params0.resize(hdr->num_parameters);
    m_ranges0.resize(hdr->num_parameters);
    m_params1.resize(hdr->num_parameters);
    m_ranges1.resize(hdr->num_parameters);
    m_static_samplers.resize(hdr->num_static_samplers);

    auto *param_headers = reinterpret_cast<const _DXRootParameterHeader *>(
        data + hdr->parameters_offset);

    for (UINT i = 0; i < hdr->num_parameters; i++) {
      const auto &src = param_headers[i];
      if (!_RSRangeContains(size, src.parameter_offset, sizeof(uint32_t)))
        return false;

      auto &dst0 = m_params0[i];
      auto &dst1 = m_params1[i];
      dst0.ParameterType =
          static_cast<D3D12_ROOT_PARAMETER_TYPE>(src.parameter_type);
      dst0.ShaderVisibility =
          static_cast<D3D12_SHADER_VISIBILITY>(src.shader_visibility);
      dst1.ParameterType = dst0.ParameterType;
      dst1.ShaderVisibility = dst0.ShaderVisibility;

      switch (dst0.ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
        if (!_RSRangeContains(size, src.parameter_offset,
                              sizeof(_DXRootConstants)))
          return false;

        auto *constants = reinterpret_cast<const _DXRootConstants *>(
            data + src.parameter_offset);
        dst0.Constants.RegisterSpace = constants->register_space;
        dst0.Constants.ShaderRegister = constants->shader_register;
        dst0.Constants.Num32BitValues = constants->num_32bit_values;
        dst1.Constants = dst0.Constants;
        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV: {
        size_t descriptor_size =
            hdr->version == D3D_ROOT_SIGNATURE_VERSION_1_0
                ? sizeof(_DXRootDescriptor10)
                : sizeof(_DXRootDescriptor10) + sizeof(uint32_t);
        if (!_RSRangeContains(size, src.parameter_offset, descriptor_size))
          return false;

        auto *descriptor = reinterpret_cast<const _DXRootDescriptor10 *>(
            data + src.parameter_offset);
        dst0.Descriptor.RegisterSpace = descriptor->register_space;
        dst0.Descriptor.ShaderRegister = descriptor->shader_register;
        dst1.Descriptor.RegisterSpace = descriptor->register_space;
        dst1.Descriptor.ShaderRegister = descriptor->shader_register;
        dst1.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
        if (hdr->version == D3D_ROOT_SIGNATURE_VERSION_1_1) {
          auto *descriptor1 = reinterpret_cast<const _DXRootDescriptor11 *>(
              data + src.parameter_offset);
          dst1.Descriptor.Flags =
              static_cast<D3D12_ROOT_DESCRIPTOR_FLAGS>(descriptor1->flags);
        }
        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
        if (!_RSRangeContains(size, src.parameter_offset,
                              sizeof(_DXDescriptorTable)))
          return false;

        auto *table = reinterpret_cast<const _DXDescriptorTable *>(
            data + src.parameter_offset);
        if (table->num_ranges > 256)
          return false;

        size_t range_size = hdr->version == D3D_ROOT_SIGNATURE_VERSION_1_0
                                ? sizeof(_DXDescriptorRange10)
                                : sizeof(_DXDescriptorRange11);
        const uint8_t *ranges_base =
            data + src.parameter_offset + sizeof(*table);
        uint32_t inline_ranges_offset = src.parameter_offset + sizeof(*table);
        if (_RSRangeContains(size, table->ranges_offset,
                             table->num_ranges * range_size)) {
          ranges_base = data + table->ranges_offset;
        } else if (!_RSRangeContains(size, inline_ranges_offset,
                                     table->num_ranges * range_size)) {
          return false;
        }

        m_ranges0[i].resize(table->num_ranges);
        m_ranges1[i].resize(table->num_ranges);
        for (UINT r = 0; r < table->num_ranges; r++) {
          auto &out_range0 = m_ranges0[i][r];
          auto &out_range1 = m_ranges1[i][r];
          if (hdr->version == D3D_ROOT_SIGNATURE_VERSION_1_0) {
            auto *src_range = reinterpret_cast<const _DXDescriptorRange10 *>(
                ranges_base + r * range_size);
            out_range0.RangeType =
                static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(src_range->range_type);
            out_range0.NumDescriptors = src_range->num_descriptors;
            out_range0.BaseShaderRegister = src_range->base_shader_register;
            out_range0.RegisterSpace = src_range->register_space;
            out_range0.OffsetInDescriptorsFromTableStart =
                src_range->offset_in_table;
            out_range1.RangeType = out_range0.RangeType;
            out_range1.NumDescriptors = out_range0.NumDescriptors;
            out_range1.BaseShaderRegister = out_range0.BaseShaderRegister;
            out_range1.RegisterSpace = out_range0.RegisterSpace;
            out_range1.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
            out_range1.OffsetInDescriptorsFromTableStart =
                out_range0.OffsetInDescriptorsFromTableStart;
          } else {
            auto *src_range = reinterpret_cast<const _DXDescriptorRange11 *>(
                ranges_base + r * range_size);
            out_range0.RangeType =
                static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(src_range->range_type);
            out_range0.NumDescriptors = src_range->num_descriptors;
            out_range0.BaseShaderRegister = src_range->base_shader_register;
            out_range0.RegisterSpace = src_range->register_space;
            out_range0.OffsetInDescriptorsFromTableStart =
                src_range->offset_in_table;
            out_range1.RangeType = out_range0.RangeType;
            out_range1.NumDescriptors = out_range0.NumDescriptors;
            out_range1.BaseShaderRegister = out_range0.BaseShaderRegister;
            out_range1.RegisterSpace = out_range0.RegisterSpace;
            out_range1.Flags =
                static_cast<D3D12_DESCRIPTOR_RANGE_FLAGS>(src_range->flags);
            out_range1.OffsetInDescriptorsFromTableStart =
                out_range0.OffsetInDescriptorsFromTableStart;
          }
        }
        dst0.DescriptorTable.NumDescriptorRanges = table->num_ranges;
        dst0.DescriptorTable.pDescriptorRanges =
            m_ranges0[i].empty() ? nullptr : m_ranges0[i].data();
        dst1.DescriptorTable.NumDescriptorRanges = table->num_ranges;
        dst1.DescriptorTable.pDescriptorRanges =
            m_ranges1[i].empty() ? nullptr : m_ranges1[i].data();
        break;
      }
      default:
        return false;
      }
    }

    auto *samplers = reinterpret_cast<const _DXStaticSampler *>(
        data + hdr->static_sampler_offset);
    for (UINT i = 0; i < hdr->num_static_samplers; i++) {
      const auto &src = samplers[i];
      auto &dst = m_static_samplers[i];
      dst.Filter = static_cast<D3D12_FILTER>(src.filter);
      dst.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src.address_u);
      dst.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src.address_v);
      dst.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src.address_w);
      dst.MipLODBias = src.mip_lod_bias;
      dst.MaxAnisotropy = src.max_anisotropy;
      dst.ComparisonFunc =
          static_cast<D3D12_COMPARISON_FUNC>(src.comparison_func);
      dst.BorderColor =
          static_cast<D3D12_STATIC_BORDER_COLOR>(src.border_color);
      dst.MinLOD = src.min_lod;
      dst.MaxLOD = src.max_lod;
      dst.ShaderRegister = src.shader_register;
      dst.RegisterSpace = src.register_space;
      dst.ShaderVisibility =
          static_cast<D3D12_SHADER_VISIBILITY>(src.shader_visibility);
    }

    FinalizeDescs(hdr->flags,
                  static_cast<D3D_ROOT_SIGNATURE_VERSION>(hdr->version));
    TraceAgility("D3D12RootSignatureDeserializer parsed RTS0 version=%u "
                 "params=%u samplers=%u flags=0x%x",
                 hdr->version, hdr->num_parameters, hdr->num_static_samplers,
                 hdr->flags);
    return true;
  }

  bool ParsePrivate(const void *data, SIZE_T size) {
    if (!data || size < sizeof(_RSHeader))
      return false;

    const uint8_t *ptr = static_cast<const uint8_t *>(data);
    const uint8_t *end = ptr + size;
    auto canRead = [&](SIZE_T bytes) -> bool {
      return bytes <= static_cast<SIZE_T>(end - ptr);
    };

    auto *hdr = reinterpret_cast<const _RSHeader *>(ptr);
    if (hdr->num_parameters > size / sizeof(_RSParameter) ||
        hdr->num_static_samplers > size / sizeof(_RSStaticSampler))
      return false;

    ptr += sizeof(_RSHeader);
    m_params0.clear();
    m_ranges0.clear();
    m_params1.clear();
    m_ranges1.clear();
    m_static_samplers.clear();
    m_params0.resize(hdr->num_parameters);
    m_ranges0.resize(hdr->num_parameters);
    m_params1.resize(hdr->num_parameters);
    m_ranges1.resize(hdr->num_parameters);
    m_static_samplers.resize(hdr->num_static_samplers);

    for (UINT i = 0; i < hdr->num_parameters; i++) {
      if (!canRead(sizeof(_RSParameter)))
        return false;

      auto *src = reinterpret_cast<const _RSParameter *>(ptr);
      auto &dst0 = m_params0[i];
      auto &dst1 = m_params1[i];
      dst0.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(src->type);
      dst0.ShaderVisibility =
          static_cast<D3D12_SHADER_VISIBILITY>(src->visibility);
      dst1.ParameterType = dst0.ParameterType;
      dst1.ShaderVisibility = dst0.ShaderVisibility;
      ptr += sizeof(_RSParameter);

      switch (dst0.ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        dst0.Constants.RegisterSpace = src->constants.register_space;
        dst0.Constants.ShaderRegister = src->constants.register_index;
        dst0.Constants.Num32BitValues = src->constants.num_32bit_values;
        dst1.Constants = dst0.Constants;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV:
        dst0.Descriptor.RegisterSpace = src->descriptor.register_space;
        dst0.Descriptor.ShaderRegister = src->descriptor.register_index;
        dst1.Descriptor.RegisterSpace = dst0.Descriptor.RegisterSpace;
        dst1.Descriptor.ShaderRegister = dst0.Descriptor.ShaderRegister;
        dst1.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        if (src->table.num_ranges >
            static_cast<UINT>((end - ptr) / sizeof(_RSDescriptorRange)))
          return false;
        m_ranges0[i].resize(src->table.num_ranges);
        m_ranges1[i].resize(src->table.num_ranges);
        for (UINT r = 0; r < src->table.num_ranges; r++) {
          auto *range = reinterpret_cast<const _RSDescriptorRange *>(ptr);
          auto &out_range0 = m_ranges0[i][r];
          auto &out_range1 = m_ranges1[i][r];
          out_range0.RangeType =
              static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(range->range_type);
          out_range0.NumDescriptors = range->num_descriptors;
          out_range0.BaseShaderRegister = range->base_register;
          out_range0.RegisterSpace = range->register_space;
          out_range0.OffsetInDescriptorsFromTableStart = range->offset_in_table;
          out_range1.RangeType = out_range0.RangeType;
          out_range1.NumDescriptors = out_range0.NumDescriptors;
          out_range1.BaseShaderRegister = out_range0.BaseShaderRegister;
          out_range1.RegisterSpace = out_range0.RegisterSpace;
          out_range1.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
          out_range1.OffsetInDescriptorsFromTableStart =
              out_range0.OffsetInDescriptorsFromTableStart;
          ptr += sizeof(_RSDescriptorRange);
        }
        dst0.DescriptorTable.NumDescriptorRanges = src->table.num_ranges;
        dst0.DescriptorTable.pDescriptorRanges = m_ranges0[i].data();
        dst1.DescriptorTable.NumDescriptorRanges = src->table.num_ranges;
        dst1.DescriptorTable.pDescriptorRanges = m_ranges1[i].data();
        break;
      default:
        return false;
      }
    }

    for (UINT i = 0; i < hdr->num_static_samplers; i++) {
      if (!canRead(sizeof(_RSStaticSampler)))
        return false;

      auto *src = reinterpret_cast<const _RSStaticSampler *>(ptr);
      auto &dst = m_static_samplers[i];
      dst.Filter = static_cast<D3D12_FILTER>(src->filter);
      dst.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src->address_u);
      dst.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src->address_v);
      dst.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src->address_w);
      dst.MipLODBias = src->mip_lod_bias;
      dst.MaxAnisotropy = src->max_anisotropy;
      dst.ComparisonFunc =
          static_cast<D3D12_COMPARISON_FUNC>(src->comparison_func);
      dst.BorderColor =
          static_cast<D3D12_STATIC_BORDER_COLOR>(src->border_color);
      dst.MinLOD = src->min_lod;
      dst.MaxLOD = src->max_lod;
      dst.ShaderRegister = src->register_index;
      dst.RegisterSpace = src->register_space;
      dst.ShaderVisibility =
          static_cast<D3D12_SHADER_VISIBILITY>(src->shader_visibility);
      ptr += sizeof(_RSStaticSampler);
    }

    FinalizeDescs(hdr->flags, D3D_ROOT_SIGNATURE_VERSION_1_0);
    return ptr <= end;
  }

  bool Parse(const void *data, SIZE_T size) {
    if (!data || size < sizeof(_RSHeader))
      return false;

    auto *bytes = static_cast<const uint8_t *>(data);
    if (size >= sizeof(_DXContainerHeader) && memcmp(bytes, "DXBC", 4) == 0) {
      auto *container = reinterpret_cast<const _DXContainerHeader *>(bytes);
      if (container->part_count <= 256 && container->file_size <= size &&
          _RSRangeContains(size, sizeof(_DXContainerHeader),
                           container->part_count * sizeof(uint32_t))) {
        auto *part_offsets = reinterpret_cast<const uint32_t *>(
            bytes + sizeof(_DXContainerHeader));
        for (UINT i = 0; i < container->part_count; i++) {
          uint32_t offset = part_offsets[i];
          if (!_RSRangeContains(size, offset, sizeof(_DXContainerPartHeader)))
            continue;

          auto *part =
              reinterpret_cast<const _DXContainerPartHeader *>(bytes + offset);
          if (memcmp(part->name, "RTS0", 4) != 0)
            continue;
          if (!_RSRangeContains(size, offset + sizeof(*part), part->size))
            continue;
          if (ParseRTS0(bytes + offset + sizeof(*part), part->size))
            return true;
        }
      }
    }

    if (ParseRTS0(bytes, size))
      return true;

    return ParsePrivate(data, size);
  }

  ULONG m_ref = 1;
  bool m_valid = false;
  D3D_ROOT_SIGNATURE_VERSION m_unconverted_version =
      D3D_ROOT_SIGNATURE_VERSION_1_0;
  D3D12_ROOT_SIGNATURE_DESC m_desc = {};
  D3D12_ROOT_SIGNATURE_DESC1 m_desc1 = {};
  D3D12_VERSIONED_ROOT_SIGNATURE_DESC m_versioned_desc0 = {};
  D3D12_VERSIONED_ROOT_SIGNATURE_DESC m_versioned_desc1 = {};
  std::vector<D3D12_ROOT_PARAMETER> m_params0;
  std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> m_ranges0;
  std::vector<D3D12_ROOT_PARAMETER1> m_params1;
  std::vector<std::vector<D3D12_DESCRIPTOR_RANGE1>> m_ranges1;
  std::vector<D3D12_STATIC_SAMPLER_DESC> m_static_samplers;
};

static HRESULT _SerializeRootSig(const D3D12_ROOT_SIGNATURE_DESC *desc,
                                 ID3DBlob **ppBlob) {
  if (!desc || !ppBlob)
    return E_INVALIDARG;
  *ppBlob = nullptr;

  std::vector<uint8_t> buf;
  size_t total = sizeof(_RSHeader) + desc->NumParameters * sizeof(_RSParameter);
  for (UINT i = 0; i < desc->NumParameters; i++) {
    if (desc->pParameters[i].ParameterType ==
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      total += desc->pParameters[i].DescriptorTable.NumDescriptorRanges *
               sizeof(_RSDescriptorRange);
  }
  total += desc->NumStaticSamplers * sizeof(_RSStaticSampler);
  buf.resize(total);

  auto *hdr = reinterpret_cast<_RSHeader *>(buf.data());
  hdr->num_parameters = desc->NumParameters;
  hdr->num_static_samplers = desc->NumStaticSamplers;
  hdr->flags = desc->Flags;

  uint8_t *ptr = buf.data() + sizeof(_RSHeader);
  for (UINT i = 0; i < desc->NumParameters; i++) {
    auto &p = desc->pParameters[i];
    auto *out = reinterpret_cast<_RSParameter *>(ptr);
    out->type = (uint8_t)p.ParameterType;
    out->visibility = (uint8_t)p.ShaderVisibility;
    switch (p.ParameterType) {
    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
      out->constants.register_space = p.Constants.RegisterSpace;
      out->constants.register_index = p.Constants.ShaderRegister;
      out->constants.num_32bit_values = p.Constants.Num32BitValues;
      break;
    case D3D12_ROOT_PARAMETER_TYPE_CBV:
    case D3D12_ROOT_PARAMETER_TYPE_SRV:
    case D3D12_ROOT_PARAMETER_TYPE_UAV:
      out->descriptor.register_space = p.Descriptor.RegisterSpace;
      out->descriptor.register_index = p.Descriptor.ShaderRegister;
      break;
    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
      out->table.num_ranges = p.DescriptorTable.NumDescriptorRanges;
      ptr += sizeof(_RSParameter);
      for (UINT r = 0; r < p.DescriptorTable.NumDescriptorRanges; r++) {
        auto &rng = p.DescriptorTable.pDescriptorRanges[r];
        auto *orng = reinterpret_cast<_RSDescriptorRange *>(ptr);
        orng->range_type = (uint8_t)rng.RangeType;
        orng->num_descriptors = rng.NumDescriptors;
        orng->base_register = rng.BaseShaderRegister;
        orng->register_space = rng.RegisterSpace;
        orng->offset_in_table = rng.OffsetInDescriptorsFromTableStart;
        ptr += sizeof(_RSDescriptorRange);
      }
      continue;
    }
    ptr += sizeof(_RSParameter);
  }

  for (UINT i = 0; i < desc->NumStaticSamplers; i++) {
    auto &s = desc->pStaticSamplers[i];
    auto *out = reinterpret_cast<_RSStaticSampler *>(ptr);
    out->filter = s.Filter;
    out->address_u = s.AddressU;
    out->address_v = s.AddressV;
    out->address_w = s.AddressW;
    out->mip_lod_bias = s.MipLODBias;
    out->max_anisotropy = s.MaxAnisotropy;
    out->comparison_func = s.ComparisonFunc;
    out->border_color = s.BorderColor;
    out->min_lod = s.MinLOD;
    out->max_lod = s.MaxLOD;
    out->register_space = s.RegisterSpace;
    out->register_index = s.ShaderRegister;
    out->shader_register_space = s.RegisterSpace;
    out->shader_visibility = (uint8_t)s.ShaderVisibility;
    ptr += sizeof(_RSStaticSampler);
  }

  *ppBlob = new _RSBlob(std::move(buf));
  return S_OK;
}

using namespace dxmt;

extern "C" HRESULT WINAPI
D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel,
                  REFIID riid, void **ppDevice) {
  {
    DXMTD3D12Trace("Entry",
                   "D3D12CreateDevice CALLED FL=%d adapter=%p riid=%s",
                   MinimumFeatureLevel, pAdapter, str::format(riid).c_str());
  }
  const bool support_probe = ppDevice == nullptr;
  if (ppDevice)
    *ppDevice = nullptr;

  Com<IMTLDXGIAdapter> dxgi_adapter;

  if (pAdapter) {
    HRESULT adapter_hr = pAdapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter));
    DXMTD3D12Trace("Entry",
                   "D3D12CreateDevice adapter QI IMTLDXGIAdapter hr=0x%lx",
                   adapter_hr);
    if (FAILED(adapter_hr)) {
      ERR("D3D12CreateDevice: adapter is not a DXMT adapter");
      return E_INVALIDARG;
    }
  } else {
    Com<IDXGIFactory1> factory;
    HRESULT factory_hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    DXMTD3D12Trace("Entry", "D3D12CreateDevice CreateDXGIFactory1 hr=0x%lx",
                   factory_hr);
    if (FAILED(factory_hr)) {
      ERR("D3D12CreateDevice: failed to create DXGI factory");
      return E_FAIL;
    }
    Com<IDXGIAdapter> adapter;
    HRESULT enum_hr = factory->EnumAdapters(0, &adapter);
    DXMTD3D12Trace("Entry",
                   "D3D12CreateDevice default EnumAdapters hr=0x%lx adapter=%p",
                   enum_hr, adapter.ptr());
    if (FAILED(enum_hr)) {
      ERR("D3D12CreateDevice: no adapters available");
      return E_FAIL;
    }
    HRESULT adapter_hr = adapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter));
    DXMTD3D12Trace("Entry",
                   "D3D12CreateDevice default adapter QI IMTLDXGIAdapter hr=0x%lx",
                   adapter_hr);
    if (FAILED(adapter_hr)) {
      ERR("D3D12CreateDevice: default adapter is not DXMT");
      return E_FAIL;
    }
  }

  if (support_probe) {
    DXMTD3D12Trace("Entry",
                   "D3D12CreateDevice SUPPORT PROBE SUCCESS FL=%d",
                   MinimumFeatureLevel);
    return S_FALSE;
  }

  try {
    void *device_mem =
        VirtualAlloc((void *)0x500000000ULL, sizeof(MTLD3D12DXGIDevice),
                     MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!device_mem) {
      device_mem =
          VirtualAlloc((void *)0x200000000ULL, sizeof(MTLD3D12DXGIDevice),
                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    if (!device_mem) {
      device_mem = VirtualAlloc(nullptr, sizeof(MTLD3D12DXGIDevice),
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    {
      DXMTD3D12Trace("Entry", "Device allocated at %p size=%zu", device_mem,
                     sizeof(MTLD3D12DXGIDevice));
    }
    auto dxgi_device = new (device_mem) MTLD3D12DXGIDevice(
        CreateDXMTDevice({.device = dxgi_adapter->GetMTLDevice()}),
        dxgi_adapter.ptr());

    HRESULT hr = dxgi_device->QueryInterface(riid, ppDevice);
    if (FAILED(hr)) {
      {
        DXMTD3D12Trace("Entry", "D3D12CreateDevice QI FAILED hr=0x%lx FL=%d",
                       hr, MinimumFeatureLevel);
      }
      dxgi_device->Release();
      return hr;
    }

    Logger::info(str::format("D3D12CreateDevice: created device with FL ",
                             MinimumFeatureLevel));
    {
      DXMTD3D12Trace("Entry", "D3D12CreateDevice SUCCESS FL=%d",
                     MinimumFeatureLevel);
    }
    return S_OK;
  } catch (const std::exception &e) {
    Logger::err(str::format("D3D12CreateDevice: exception: ", e.what()));
    {
      DXMTD3D12Trace("Entry", "D3D12CreateDevice EXCEPTION: %s FL=%d",
                     e.what(), MinimumFeatureLevel);
    }
    return E_FAIL;
  }
}

extern "C" HRESULT WINAPI
D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature,
                            D3D_ROOT_SIGNATURE_VERSION Version,
                            ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob) {
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      fprintf(f, "D3D12SerializeRootSignature version=%u params=%u\n", Version,
              pRootSignature ? pRootSignature->NumParameters : 0);
      fclose(f);
    }
  }
  return _SerializeRootSig(pRootSignature, ppBlob);
}

extern "C" HRESULT WINAPI D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
    ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob) {
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      fprintf(f, "D3D12SerializeVersionedRootSignature version=%u\n",
              pRootSignature ? pRootSignature->Version : 0);
      fclose(f);
    }
  }
  if (!pRootSignature)
    return E_INVALIDARG;
  if (pRootSignature->Version == D3D_ROOT_SIGNATURE_VERSION_1_0)
    return _SerializeRootSig(&pRootSignature->Desc_1_0, ppBlob);
  if (pRootSignature->Version == D3D_ROOT_SIGNATURE_VERSION_1_1) {
    const auto &d1 = pRootSignature->Desc_1_1;
    D3D12_ROOT_SIGNATURE_DESC desc0 = {};
    desc0.NumParameters = d1.NumParameters;
    desc0.NumStaticSamplers = d1.NumStaticSamplers;
    desc0.pStaticSamplers = d1.pStaticSamplers;
    desc0.Flags = d1.Flags;
    std::vector<D3D12_ROOT_PARAMETER> params(d1.NumParameters);
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
    for (UINT i = 0; i < d1.NumParameters; i++) {
      auto &src = d1.pParameters[i];
      auto &dst = params[i];
      dst.ParameterType = src.ParameterType;
      dst.ShaderVisibility = src.ShaderVisibility;
      switch (src.ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        dst.Constants = src.Constants;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV:
        dst.Descriptor.ShaderRegister = src.Descriptor.ShaderRegister;
        dst.Descriptor.RegisterSpace = src.Descriptor.RegisterSpace;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
        dst.DescriptorTable.NumDescriptorRanges =
            src.DescriptorTable.NumDescriptorRanges;
        size_t base = ranges.size();
        for (UINT r = 0; r < src.DescriptorTable.NumDescriptorRanges; r++) {
          auto &rs = src.DescriptorTable.pDescriptorRanges[r];
          D3D12_DESCRIPTOR_RANGE dr = {};
          dr.RangeType = rs.RangeType;
          dr.NumDescriptors = rs.NumDescriptors;
          dr.BaseShaderRegister = rs.BaseShaderRegister;
          dr.RegisterSpace = rs.RegisterSpace;
          dr.OffsetInDescriptorsFromTableStart =
              rs.OffsetInDescriptorsFromTableStart;
          ranges.push_back(dr);
        }
        dst.DescriptorTable.pDescriptorRanges = ranges.data() + base;
        break;
      }
      }
    }
    desc0.pParameters = params.data();
    return _SerializeRootSig(&desc0, ppBlob);
  }
  return E_INVALIDARG;
}

extern "C" HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
    const void *pData, SIZE_T NumBytes, REFIID riid, void **ppDeserializer) {
  if (!ppDeserializer)
    return E_POINTER;
  *ppDeserializer = nullptr;

  auto *deserializer = new _RSDeserializer(pData, NumBytes);
  if (!deserializer->Valid()) {
    deserializer->Release();
    TraceAgility(
        "D3D12CreateRootSignatureDeserializer bytes=%zu -> E_INVALIDARG",
        NumBytes);
    return E_INVALIDARG;
  }

  HRESULT hr = deserializer->QueryInterface(riid, ppDeserializer);
  deserializer->Release();
  TraceAgility(
      "D3D12CreateRootSignatureDeserializer bytes=%zu riid=%s -> 0x%lx",
      NumBytes, str::format(riid).c_str(), hr);
  return hr;
}

extern "C" HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
    const void *pData, SIZE_T NumBytes, REFIID riid, void **ppDeserializer) {
  return D3D12CreateRootSignatureDeserializer(pData, NumBytes, riid,
                                              ppDeserializer);
}

extern "C" HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void **ppDebug) {
  TraceAgility("D3D12GetDebugInterface riid=%s out=%p -> E_NOINTERFACE",
               str::format(riid).c_str(), ppDebug);
  if (ppDebug)
    *ppDebug = nullptr;
  return E_NOINTERFACE;
}

extern "C" UINT D3D12SDKVersion = kD3D12AgilitySDKVersion;

extern "C" HRESULT WINAPI D3D12EnableExperimentalFeatures(
    UINT feature_count, const IID *iids, void *configurations,
    UINT *configuration_sizes) {
  if (feature_count && !iids)
    return E_INVALIDARG;

  TraceAgility("D3D12EnableExperimentalFeatures count=%u configs=%p sizes=%p",
               feature_count, configurations, configuration_sizes);
  for (UINT i = 0; i < feature_count; i++) {
    TraceAgility("  feature[%u]=%s", i, str::format(iids[i]).c_str());
  }
  return S_OK;
}

extern "C" HRESULT WINAPI D3D12GetInterface(REFCLSID clsid, REFIID riid,
                                            void **ppv) {
  TraceAgility("D3D12GetInterface ENTER clsid=%s riid=%s out=%p",
               str::format(clsid).c_str(), str::format(riid).c_str(), ppv);
  if (!ppv)
    return E_POINTER;
  *ppv = nullptr;
  if (clsid == kCLSID_D3D12SDKConfiguration) {
    auto *configuration = new MTLD3D12SDKConfiguration();
    HRESULT hr = configuration->QueryInterface(riid, ppv);
    configuration->Release();
    TraceAgility("D3D12GetInterface SDKConfiguration riid=%s -> 0x%lx out=%p",
                 str::format(riid).c_str(), hr, ppv ? *ppv : nullptr);
    return hr;
  }
  if (clsid == kCLSID_D3D12DeviceFactory ||
      clsid == kIID_ID3D12DeviceFactory) {
    auto *factory = new MTLD3D12DeviceFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    TraceAgility("D3D12GetInterface DeviceFactory riid=%s -> 0x%lx out=%p",
                 str::format(riid).c_str(), hr, ppv ? *ppv : nullptr);
    return hr;
  }
  if (riid == kIID_ID3D12DeviceConfiguration ||
      riid == kIID_ID3D12DeviceConfiguration1) {
    return CreateD3D12DeviceConfiguration(riid, ppv);
  }
  if (clsid == kCLSID_D3D12StateObjectFactory ||
      clsid == kIID_ID3D12StateObjectDatabaseFactory) {
    auto *factory = new MTLD3D12StateObjectDatabaseFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    TraceAgility("D3D12GetInterface StateObjectFactory riid=%s -> 0x%lx out=%p",
                 str::format(riid).c_str(), hr, ppv ? *ppv : nullptr);
    return hr;
  }
  if (clsid == kCLSID_D3D12RuntimeValidationControl) {
    auto *control = new MTLD3D12RuntimeValidationControl();
    HRESULT hr = control->QueryInterface(riid, ppv);
    control->Release();
    TraceAgility("D3D12GetInterface RuntimeValidationControl riid=%s -> 0x%lx out=%p",
                 str::format(riid).c_str(), hr, ppv ? *ppv : nullptr);
    return hr;
  }
  if (clsid == kCLSID_D3D12ApplicationIdentity) {
    auto *identity = new MTLD3D12ApplicationIdentity();
    HRESULT hr = identity->QueryInterface(riid, ppv);
    identity->Release();
    TraceAgility("D3D12GetInterface ApplicationIdentity riid=%s -> 0x%lx out=%p",
                 str::format(riid).c_str(), hr, ppv ? *ppv : nullptr);
    return hr;
  }

  Logger::warn(str::format("D3D12GetInterface: clsid=", clsid, " riid=", riid,
                           " -> E_NOINTERFACE"));
  TraceAgility("D3D12GetInterface clsid=%s riid=%s -> E_NOINTERFACE",
               str::format(clsid).c_str(), str::format(riid).c_str());
  return E_NOINTERFACE;
}

#ifdef _WIN32
extern void install_crash_handler();
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
    install_crash_handler();
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      char exe[MAX_PATH];
      GetModuleFileNameA(NULL, exe, MAX_PATH);
      fprintf(f, "=== d3d12.dll DllMain PROCESS_ATTACH pid=%lu exe=[%s] ===\n",
              GetCurrentProcessId(), exe);
      fclose(f);
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      char exe[MAX_PATH];
      GetModuleFileNameA(NULL, exe, MAX_PATH);
      fprintf(f, "=== d3d12.dll DllMain PROCESS_DETACH pid=%lu exe=[%s] ===\n",
              GetCurrentProcessId(), exe);
      fclose(f);
    }
  }
  return TRUE;
}
#endif

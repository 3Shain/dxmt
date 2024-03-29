#include "d3d11_inspection.hpp"

#include "../dxgi/dxgi_format.hpp"

namespace dxmt {

MTLD3D11Inspection::MTLD3D11Inspection(MTL::Device *pDevice)
    : m_device(pDevice), cap_inspector_() {

  // FIXME: Apple Silicon definitely TBDR
  m_architectureInfo.TileBasedDeferredRenderer = FALSE;

  m_threading.DriverConcurrentCreates = TRUE; // I guess
  m_threading.DriverCommandLists = TRUE;      //  should be?

  /*
  MSL Specification 2.1: Metal does _not_ support double
  */
  m_doubles.DoublePrecisionFloatShaderOps = FALSE;

  m_d3d9Options.FullNonPow2TextureSupport = TRUE; // no doubt
  m_d3d10Options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x =
      TRUE; // no doubt

  // dx11 features
  m_d3d11Options.ClearView = TRUE; // ..
  m_d3d11Options.ConstantBufferOffsetting = TRUE;
  m_d3d11Options.ConstantBufferPartialUpdate = TRUE;
  m_d3d11Options.MapNoOverwriteOnDynamicConstantBuffer = TRUE;
  /*
  https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400754-copyfromtexture?language=objc
  Copying data to overlapping regions within the same texture may result in
  unexpected behavior.

  https://learn.microsoft.com/en-us/windows/win32/direct3d11/using-direct3d-optional-features-to-supplement-direct3d-feature-levels
  TODO: ClearView, CopyWithOverlap, ConstantBufferPartialUpdate,
  ConstantBufferOffsetting, and MapNoOverwriteOnDynamicConstantBuffer must be
  all TRUE/FALSE. Maybe we should support it and when there is overlap we use a
  temp buffer
  */
  m_d3d11Options.CopyWithOverlap = TRUE;
  /*
  FXIME: I think it is supported
   */
  m_d3d11Options.MultisampleRTVWithForcedSampleCountOne = TRUE;
  m_d3d11Options.MapNoOverwriteOnDynamicBufferSRV = TRUE;
  /*
  https://github.com/gpuweb/gpuweb/issues/503#issuecomment-908973358
  */
  m_d3d11Options.UAVOnlyRenderingForcedSampleCount = TRUE;
  m_d3d11Options.DiscardAPIsSeenByDriver = TRUE; // wtf
  // In metal we don't support double
  m_d3d11Options.ExtendedDoublesShaderInstructions = FALSE;
  m_d3d11Options.ExtendedResourceSharing = TRUE; // wtf
  // FIXME: check it
  m_d3d11Options.SAD4ShaderInstructions = FALSE;
  m_d3d11Options.FlagsForUpdateAndCopySeenByDriver = TRUE; // wtf
  m_d3d11Options.OutputMergerLogicOp =
      FALSE; // FIXME: This must be true for 11.1 .programmable blend might
             // solve this

  m_d3d11Options5.SharedResourceTier = D3D11_SHARED_RESOURCE_TIER_0; // TODO

  // use dxvk values
  m_gpuVirtualAddress.MaxGPUVirtualAddressBitsPerProcess = 40;
  m_gpuVirtualAddress.MaxGPUVirtualAddressBitsPerResource = 32;

  m_shaderCache.SupportFlags =
      D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_DISK_CACHE |
      D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_INPROC_CACHE; // why application
                                                         // would ask for
                                                         // this...

  /*
  Metal: half data type
  */
  m_shaderMinPrecision.PixelShaderMinPrecision =
      D3D11_SHADER_MIN_PRECISION_16_BIT;
  m_shaderMinPrecision.AllOtherShaderStagesMinPrecision =
      D3D11_SHADER_MIN_PRECISION_16_BIT;

  m_d3d9Shadow.SupportsDepthAsTextureWithLessEqualComparisonFilter = TRUE;

  cap_inspector_.Inspect(this->m_device.ptr());
}

HRESULT MTLD3D11Inspection::GetFeatureData(D3D11_FEATURE Feature,
                                           UINT FeatureDataSize,
                                           void *pFeatureData) const {
  // MSDN order
  // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_feature
  // so far support dx11.1
  switch (Feature) {
  case D3D11_FEATURE_THREADING:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_threading);
  case D3D11_FEATURE_DOUBLES:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_doubles);
  case D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d10Options);
  case D3D11_FEATURE_D3D11_OPTIONS:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options);
  case D3D11_FEATURE_ARCHITECTURE_INFO:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData,
                               &m_architectureInfo);
  case D3D11_FEATURE_D3D9_OPTIONS:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9Options);
  case D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData,
                               &m_shaderMinPrecision);
  case D3D11_FEATURE_D3D9_SHADOW_SUPPORT:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9Shadow);
    //   case D3D11_FEATURE_D3D11_OPTIONS1:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //     &m_d3d11Options1);
    //   case D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //                                &m_d3d9SimpleInstancing);
    //   case D3D11_FEATURE_MARKER_SUPPORT:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_marker);
    //   case D3D11_FEATURE_D3D9_OPTIONS1:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //     &m_d3d9Options1);
    //   case D3D11_FEATURE_D3D11_OPTIONS2:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //     &m_d3d11Options2);
    //   case D3D11_FEATURE_D3D11_OPTIONS3:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //     &m_d3d11Options3);
    //   case D3D11_FEATURE_D3D11_OPTIONS4:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //     &m_d3d11Options4);
  case D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData,
                               &m_gpuVirtualAddress);
  case D3D11_FEATURE_D3D11_OPTIONS5:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options5);
  case D3D11_FEATURE_SHADER_CACHE:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_shaderCache);
  default:
    ERR("Not supported feature: ", Feature);
    return E_INVALIDARG;
  }
}

HRESULT MTLD3D11Inspection::CheckSupportedFormat(DXGI_FORMAT Format,
                                                 UINT *pFlags) {

  const auto pfmt = g_metal_format_map[Format];
  if (pfmt.pixel_format == MTL::PixelFormatInvalid) {
    return E_FAIL;
  }

  // TODO:

  *pFlags =
      D3D11_FORMAT_SUPPORT_BUFFER | D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER |
      D3D11_FORMAT_SUPPORT_TEXTURE1D | D3D11_FORMAT_SUPPORT_TEXTURE2D |
      D3D11_FORMAT_SUPPORT_TEXTURE3D | D3D11_FORMAT_SUPPORT_TEXTURECUBE |
      D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE |
      D3D11_FORMAT_SUPPORT_MIP | D3D11_FORMAT_SUPPORT_MIP_AUTOGEN |
      D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_BLENDABLE |
      D3D11_FORMAT_SUPPORT_CPU_LOCKABLE |
      D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE | D3D11_FORMAT_SUPPORT_DISPLAY |
      D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT |
      D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET |
      D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD |
      D3D11_FORMAT_SUPPORT_SHADER_GATHER |
      D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT |
      D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT;

  return S_OK;
}

HRESULT MTLD3D11Inspection::CheckSupportedFormat2(DXGI_FORMAT Format,
                                                  UINT *pFlags) {
  // D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD

  return S_OK;
}

} // namespace dxmt
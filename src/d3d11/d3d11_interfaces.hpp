#pragma once
#include "com/com_guid.hpp"
#include "d3d11.h"

struct MTL_TEMPORAL_UPSCALE_D3D11_DESC {
  UINT InputContentWidth; // can be 0, which means full width
  UINT InputContentHeight; // can be 0, which means full height
  BOOL AutoExposure;
  BOOL InReset;
  BOOL DepthReversed;
  ID3D11Resource *Color;
  ID3D11Resource *Depth;
  ID3D11Resource *MotionVector;
  ID3D11Resource *Output;
  FLOAT MotionVectorScaleX;
  FLOAT MotionVectorScaleY;
  FLOAT PreExposure;
  ID3D11Resource *ExposureTexture;
  FLOAT JitterOffsetX;
  FLOAT JitterOffsetY;
};

DEFINE_COM_INTERFACE("43ace3ce-1956-448b-a4eb-aee68bdeb283",
                     IMTLD3D11ContextExt)
    : public IUnknown{
   virtual void STDMETHODCALLTYPE TemporalUpscale(const MTL_TEMPORAL_UPSCALE_D3D11_DESC *pDesc) = 0;
   virtual void STDMETHODCALLTYPE BeginUAVOverlap() = 0;
   virtual void STDMETHODCALLTYPE EndUAVOverlap() = 0;
};

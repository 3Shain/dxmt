/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "Metal.hpp"
#include "d3d12_device.hpp"

namespace dxmt {

constexpr WMTCompareFunction kCompareFunctionMap[] = {
    WMTCompareFunctionNever, // padding 0
    WMTCompareFunctionNever, // 1 - 1
    WMTCompareFunctionLess,    WMTCompareFunctionEqual,    WMTCompareFunctionLessEqual,
    WMTCompareFunctionGreater, WMTCompareFunctionNotEqual, WMTCompareFunctionGreaterEqual,
    WMTCompareFunctionAlways // 8 - 1
};

constexpr WMTSamplerAddressMode kAddressModeMap[] = {
    // MTL: Texture coordinates wrap to the other side of the texture,
    // effectively keeping only the fractional part of the texture coordinate.

    // MSDN: Tile the texture at every (u,v) integer junction. For example, for
    // u values between 0 and 3, the texture is repeated three times.
    WMTSamplerAddressModeRepeat, // 1 - 1

    // MTL:Between -1.0 and 1.0, the texture coordinates are mirrored across the
    // axis; outside -1.0 and 1.0, the image is repeated.

    // MSDN: Flip the texture at every (u,v) integer junction. For u values
    // between 0 and 1, for example, the texture is addressed normally; between
    // 1 and 2, the texture is flipped (mirrored); between 2 and 3, the texture
    // is normal again; and so on.
    WMTSamplerAddressModeMirrorRepeat,
    // MTL: Texture coordinates are clamped between 0.0 and 1.0, inclusive.
    // MSDN: Texture coordinates outside the range [0.0, 1.0] are set to the
    // texture color at 0.0 or 1.0, respectively.
    WMTSamplerAddressModeClampToEdge,

    // MTL: Out-of-range texture coordinates return the value specified by the
    // borderColor property.

    // MSDN: Texture coordinates outside the range [0.0, 1.0] are set to the
    // border color specified in D3D11_SAMPLER_DESC or HLSL code.
    WMTSamplerAddressModeClampToBorderColor,
    // MTL: Between -1.0 and 1.0, the texture coordinates are mirrored across
    // the axis; outside -1.0 and 1.0, texture coordinates are clamped.

    // MSDN:
    //  Similar to D3D11_TEXTURE_ADDRESS_MIRROR and D3D11_TEXTURE_ADDRESS_CLAMP.
    //  Takes the absolute value of the texture coordinate (thus, mirroring
    //  around 0), and then clamps to the maximum value.
    WMTSamplerAddressModeMirrorClampToEdge // 5 - 1
};

void
PopulateWMTSamplerInfo(WMT::Device Device, WMTSamplerInfo &InfoOut, D3D12_STATIC_SAMPLER_DESC const &Desc) {

  InfoOut.lod_average = false;
  InfoOut.mip_filter = WMTSamplerMipFilterNotMipmapped;
  // filter
  if (D3D12_DECODE_MIN_FILTER(Desc.Filter)) { // LINEAR = 1
    InfoOut.min_filter = WMTSamplerMinMagFilterLinear;
  } else {
    InfoOut.min_filter = WMTSamplerMinMagFilterNearest;
  }
  if (D3D12_DECODE_MAG_FILTER(Desc.Filter)) { // LINEAR = 1
    InfoOut.mag_filter = WMTSamplerMinMagFilterLinear;
  } else {
    InfoOut.mag_filter = WMTSamplerMinMagFilterNearest;
  }
  if (D3D12_DECODE_MIP_FILTER(Desc.Filter)) { // LINEAR = 1
    InfoOut.mip_filter = WMTSamplerMipFilterLinear;
  } else {
    InfoOut.mip_filter = WMTSamplerMipFilterNearest;
  }

  InfoOut.lod_min_clamp = Desc.MinLOD;
  InfoOut.lod_max_clamp = Desc.MaxLOD;

  // Anisotropy
  if (D3D12_DECODE_IS_ANISOTROPIC_FILTER(Desc.Filter)) {
    InfoOut.max_anisotroy = std::clamp(Desc.MaxAnisotropy, 1u, 16u);
  } else {
    InfoOut.max_anisotroy = 1;
  }

  // address modes
  // U-S  V-T W-R
  if (Desc.AddressU > 0 && Desc.AddressU < 6) {
    InfoOut.s_address_mode = kAddressModeMap[Desc.AddressU - 1];
  }
  if (Desc.AddressV > 0 && Desc.AddressV < 6) {
    InfoOut.t_address_mode = kAddressModeMap[Desc.AddressV - 1];
  }
  if (Desc.AddressW > 0 && Desc.AddressW < 6) {
    InfoOut.r_address_mode = kAddressModeMap[Desc.AddressW - 1];
  }

  InfoOut.compare_function = WMTCompareFunctionNever;
  if (D3D12_DECODE_IS_COMPARISON_FILTER(Desc.Filter)) {
    if (Desc.ComparisonFunc < 1 || Desc.ComparisonFunc > 8) {
      WARN("CreateSamplerState: invalid ComparisonFunc");
    } else {
      InfoOut.compare_function = kCompareFunctionMap[Desc.ComparisonFunc];
    }
  }
  switch (Desc.BorderColor) {
  case D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK:
    InfoOut.border_color = WMTSamplerBorderColorTransparentBlack;
    break;
  case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK:
    InfoOut.border_color = WMTSamplerBorderColorOpaqueBlack;
    break;
  default:
    InfoOut.border_color = WMTSamplerBorderColorOpaqueWhite;
    break;
  }
  InfoOut.support_argument_buffers = true;
  InfoOut.normalized_coords = true;
}

void
PopulateWMTSamplerInfo(WMT::Device Device, WMTSamplerInfo &InfoOut, D3D12_SAMPLER_DESC const &Desc) {

  InfoOut.lod_average = false;
  InfoOut.mip_filter = WMTSamplerMipFilterNotMipmapped;
  // filter
  if (D3D12_DECODE_MIN_FILTER(Desc.Filter)) { // LINEAR = 1
    InfoOut.min_filter = WMTSamplerMinMagFilterLinear;
  } else {
    InfoOut.min_filter = WMTSamplerMinMagFilterNearest;
  }
  if (D3D12_DECODE_MAG_FILTER(Desc.Filter)) { // LINEAR = 1
    InfoOut.mag_filter = WMTSamplerMinMagFilterLinear;
  } else {
    InfoOut.mag_filter = WMTSamplerMinMagFilterNearest;
  }
  if (D3D12_DECODE_MIP_FILTER(Desc.Filter)) { // LINEAR = 1
    InfoOut.mip_filter = WMTSamplerMipFilterLinear;
  } else {
    InfoOut.mip_filter = WMTSamplerMipFilterNearest;
  }

  InfoOut.lod_min_clamp = Desc.MinLOD;
  InfoOut.lod_max_clamp = Desc.MaxLOD;

  // Anisotropy
  if (D3D12_DECODE_IS_ANISOTROPIC_FILTER(Desc.Filter)) {
    InfoOut.max_anisotroy = std::clamp(Desc.MaxAnisotropy, 1u, 16u);
  } else {
    InfoOut.max_anisotroy = 1;
  }

  // address modes
  // U-S  V-T W-R
  if (Desc.AddressU > 0 && Desc.AddressU < 6) {
    InfoOut.s_address_mode = kAddressModeMap[Desc.AddressU - 1];
  }
  if (Desc.AddressV > 0 && Desc.AddressV < 6) {
    InfoOut.t_address_mode = kAddressModeMap[Desc.AddressV - 1];
  }
  if (Desc.AddressW > 0 && Desc.AddressW < 6) {
    InfoOut.r_address_mode = kAddressModeMap[Desc.AddressW - 1];
  }

  InfoOut.compare_function = WMTCompareFunctionNever;
  if (D3D12_DECODE_IS_COMPARISON_FILTER(Desc.Filter)) {
    if (Desc.ComparisonFunc < 1 || Desc.ComparisonFunc > 8) {
      WARN("CreateSamplerState: invalid ComparisonFunc");
    } else {
      InfoOut.compare_function = kCompareFunctionMap[Desc.ComparisonFunc];
    }
  }

  // border color
  InfoOut.border_color = WMTSamplerBorderColorOpaqueWhite;
  if ((Desc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER || Desc.AddressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
       Desc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER)) {
    if (Desc.BorderColor[0] == 0.0f && Desc.BorderColor[1] == 0.0f && Desc.BorderColor[2] == 0.0f &&
        Desc.BorderColor[3] == 0.0f) {
      InfoOut.border_color = WMTSamplerBorderColorTransparentBlack;
    } else if (Desc.BorderColor[0] == 0.0f && Desc.BorderColor[1] == 0.0f && Desc.BorderColor[2] == 0.0f &&
               Desc.BorderColor[3] == 1.0f) {
      InfoOut.border_color = WMTSamplerBorderColorOpaqueBlack;
    } else if (Desc.BorderColor[0] == 1.0f && Desc.BorderColor[1] == 1.0f && Desc.BorderColor[2] == 1.0f &&
               Desc.BorderColor[3] == 1.0f) {
      InfoOut.border_color = WMTSamplerBorderColorOpaqueWhite;
    } else {
      WARN(
          "CreateSamplerState: Unsupported border color (", Desc.BorderColor[0], ", ", Desc.BorderColor[1], ", ",
          Desc.BorderColor[2], ", ", Desc.BorderColor[3], ")"
      );
    }
  }

  InfoOut.support_argument_buffers = true;
  InfoOut.normalized_coords = true;
}

} // namespace dxmt
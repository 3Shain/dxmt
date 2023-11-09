//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// QuartzCore/CAPrivate.hpp
//
// Copyright 2020-2023 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "CADefines.hpp"

#include "objc-wrapper/runtime.h"

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#define _CA_PRIVATE_CLS(symbol) (Private::Class::s_k##symbol)
#define _CA_PRIVATE_SEL(accessor) (Private::Selector::s_k##accessor)

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef _EXTERN
#define _EXTERN extern
#endif

#define _CA_PRIVATE_DEF_CLS(symbol) _EXTERN void *s_k##symbol
#define _CA_PRIVATE_DEF_PRO(symbol) _EXTERN void *s_k##symbol
#define _CA_PRIVATE_DEF_SEL(accessor, symbol) _EXTERN SEL s_k##accessor
#define _CA_PRIVATE_DEF_STR(type, symbol) _EXTERN type const CA::symbol

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace CA {
namespace Private {
namespace Class {
_CA_PRIVATE_DEF_CLS(CAMetalLayer);
} // namespace Class
} // namespace Private
} // namespace CA

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace CA {
namespace Private {
namespace Protocol {

_CA_PRIVATE_DEF_PRO(CAMetalDrawable);

} // namespace Protocol
} // namespace Private
} // namespace CA

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace CA {
namespace Private {
namespace Selector {
_CA_PRIVATE_DEF_SEL(device, "device");
_CA_PRIVATE_DEF_SEL(drawableSize, "drawableSize");
_CA_PRIVATE_DEF_SEL(framebufferOnly, "framebufferOnly");
_CA_PRIVATE_DEF_SEL(allowsNextDrawableTimeout, "allowsNextDrawableTimeout");
_CA_PRIVATE_DEF_SEL(layer, "layer");
_CA_PRIVATE_DEF_SEL(nextDrawable, "nextDrawable");
_CA_PRIVATE_DEF_SEL(pixelFormat, "pixelFormat");
_CA_PRIVATE_DEF_SEL(setDevice_, "setDevice:");
_CA_PRIVATE_DEF_SEL(setDrawableSize_, "setDrawableSize:");
_CA_PRIVATE_DEF_SEL(setFramebufferOnly_, "setFramebufferOnly:");
_CA_PRIVATE_DEF_SEL(setPixelFormat_, "setPixelFormat:");
_CA_PRIVATE_DEF_SEL(texture, "texture");
} // namespace Selector
} // namespace Private
} // namespace CA

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

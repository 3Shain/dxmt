//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLPrivate.hpp
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

#include "MTLDefines.hpp"

#include "objc-wrapper/runtime.h"

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#define _MTL_PRIVATE_CLS(symbol) (Private::Class::s_k##symbol)
#define _MTL_PRIVATE_SEL(accessor) (Private::Selector::s_k##accessor)
// #define _MTL_PRIVATE_SEL(accessor) ((unix_printf("called to "#accessor"\n"),Private::Selector::s_k##accessor))

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef _EXTERN
#define _EXTERN extern
#endif

#define _MTL_PRIVATE_DEF_CLS(symbol) _EXTERN void *s_k##symbol
#define _MTL_PRIVATE_DEF_PRO(symbol) _EXTERN void *s_k##symbol
#define _MTL_PRIVATE_DEF_SEL(accessor, symbol) _EXTERN SEL s_k##accessor
#define _MTL_PRIVATE_DEF_STR(type, symbol) _EXTERN type symbol
#define _MTL_PRIVATE_DEF_CONST(type, symbol) _EXTERN type symbol
#define _MTL_PRIVATE_DEF_WEAK_CONST(type, symbol) _EXTERN type symbol

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace MTL {
namespace Private {
namespace Class {} // namespace Class
} // namespace Private
} // namespace MTL

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace MTL {
namespace Private {
namespace Protocol {} // namespace Protocol
} // namespace Private
} // namespace MTL

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace MTL {
namespace Private {
namespace Selector {

_MTL_PRIVATE_DEF_SEL(beginScope, "beginScope");
_MTL_PRIVATE_DEF_SEL(endScope, "endScope");
} // namespace Selector
} // namespace Private
} // namespace MTL

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLIOCompressor.hpp
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

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"
#include "MTLDevice.hpp"

#include <Foundation/Foundation.hpp>

namespace MTL {
_MTL_ENUM(NS::Integer, IOCompressionStatus){
    IOCompressionStatusComplete = 0,
    IOCompressionStatusError = 1,
};

size_t IOCompressionContextDefaultChunkSize();

void *IOCreateCompressionContext(const char *path, IOCompressionMethod type,
                                 size_t chunkSize);

void IOCompressionContextAppendData(void *context, const void *data,
                                    size_t size);

IOCompressionStatus IOFlushAndDestroyCompressionContext(void *context);

} // namespace MTL
//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// QuartzCore/CADeveloperHUDProperties.hpp
//
// Copyright 2024 Feifan He.
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
#include "CADeveloperHUDProperties.hpp"
#include "CAPrivate.hpp"

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace CA
{

class DeveloperHUDProperties : public NS::Referencing<DeveloperHUDProperties>
{
public:
    static class DeveloperHUDProperties* instance();

    bool                     addLabel(NS::String* label, NS::String* after);

    void                     updateLabel(NS::String* label, NS::String* value);
};
} // namespace CA

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
_CA_INLINE CA::DeveloperHUDProperties* CA::DeveloperHUDProperties::instance()
{
    // FIXME: the class is not ready when program is initialized
    return Object::sendMessage<CA::DeveloperHUDProperties*>(objc_lookUpClass("_CADeveloperHUDProperties"), _CA_PRIVATE_SEL(instance));
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_CA_INLINE bool CA::DeveloperHUDProperties::addLabel(NS::String* label, NS::String* after)
{
    return Object::sendMessage<bool>(this, _CA_PRIVATE_SEL(addLabel_),
        label, after);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_CA_INLINE void CA::DeveloperHUDProperties::updateLabel(NS::String* label, NS::String* value)
{
    return Object::sendMessage<void>(this, _CA_PRIVATE_SEL(updateLabel_),
        label, value);
}


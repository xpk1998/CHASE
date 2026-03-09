/**
 *  Copyright (C) 2021 FISCO BCOS.
 *  SPDX-License-Identifier: Apache-2.0
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.

 */
#pragma once

#include "../utilities/Ranges.h"
#include <span>
#include <stdexcept>

namespace bcos::crypto::trivial
{
    // Simple implementation for demonstration purposes
    template<typename T>
    std::span<std::byte> toView(T& object)
    {
        return std::span<std::byte>(reinterpret_cast<std::byte*>(&object), sizeof(T));
    }
    
    template<typename T>
    void resizeTo(T& out, size_t size)
    {
        // Simple implementation - in a real scenario, this would depend on the container type
        // For now, we'll assume it's a container with resize method
        out.resize(size);
    }
}


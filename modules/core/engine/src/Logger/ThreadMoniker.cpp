/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <atomic>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "AACE/Engine/Logger/ThreadMoniker.h"

namespace aace {
namespace engine {
namespace logger {

thread_local ThreadMoniker ThreadMoniker::m_threadMoniker;

/// Counter to generate (small) unique thread monikers.
static std::atomic<int> g_nextThreadMoniker(1);

ThreadMoniker::ThreadMoniker() : m_moniker{} {
    std::ostringstream stream;
    stream << std::setw(3) << std::hex << std::right << g_nextThreadMoniker++;
    auto moniker = stream.str();
    std::strncpy(m_moniker, moniker.c_str(), sizeof(m_moniker) - 1);
}

const char* ThreadMoniker::getThisThreadMoniker() {
    return m_threadMoniker.m_moniker;
}

}  // namespace logger
}  // namespace engine
}  // namespace aace

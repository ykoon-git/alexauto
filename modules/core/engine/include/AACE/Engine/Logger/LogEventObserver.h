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

#ifndef AACE_ENGINE_LOGGER_LOG_EVENT_OBSERVER_H
#define AACE_ENGINE_LOGGER_LOG_EVENT_OBSERVER_H

#include <chrono>

#include "AACE/Logger/LoggerEngineInterfaces.h"

namespace aace {
namespace engine {
namespace logger {

class LogEventObserver {
public:
    // LogEventObserver::Level alias
    using Level = aace::logger::LoggerEngineInterface::Level;

    virtual bool onLogEvent(
        Level level,
        std::chrono::system_clock::time_point time,
        const char* source,
        const char* text) = 0;
};

}  // namespace logger
}  // namespace engine
}  // namespace aace

#endif  // AACE_ENGINE_LOGGER_LOG_EVENT_OBSERVER_H

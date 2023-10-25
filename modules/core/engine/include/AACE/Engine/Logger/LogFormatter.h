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

#ifndef AACE_ENGINE_LOGGER_LOG_FORMATTER_H
#define AACE_ENGINE_LOGGER_LOG_FORMATTER_H

#include <string>
#include <chrono>
#include <memory>

#include "AACE/Logger/LoggerEngineInterfaces.h"

namespace aace {
namespace engine {
namespace logger {

class LogFormatter {
public:
    virtual ~LogFormatter() = default;

    static std::unique_ptr<LogFormatter> createPlainText();
    static std::unique_ptr<LogFormatter> createColor();

    // EngineLogger::Level alias
    using Level = aace::logger::LoggerEngineInterface::Level;

    std::string format(
        Level level,
        std::chrono::system_clock::time_point time,
        const char* source,
        const char* threadMoniker,
        const char* text);

    virtual void formatWithTime(
        std::stringstream& stringToEmit,
        bool dateTimeFailure,
        const char* dateTimeString,
        bool millisecondFailure,
        const char* millisString,
        const char* source,
        const char* threadMoniker,
        LogFormatter::Level level,
        const char* text) = 0;

    virtual void formatWithoutTime(
        std::stringstream& stringToEmit,
        const char* source,
        const char* threadMoniker,
        LogFormatter::Level level,
        const char* text) = 0;

protected:
    char getLevelCh(const Level& level) const;
};

}  // namespace logger
}  // namespace engine
}  // namespace aace

#endif  // AACE_ENGINE_LOGGER_LOG_FORMATTER_H

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

#include "AACE/Engine/Logger/LogEntry.h"

#include <cstring>
#include <iomanip>

namespace aace {
namespace engine {
namespace logger {

/// List of characters we need to escape.
static const char* RESERVED_METADATA_CHARS = R"(\,=:)";

/// Escape sequence for '\'.
static const char* ESCAPED_METADATA_ESCAPE = R"(\\)";

/// Escape sequence for ','.
static const char* ESCAPED_PAIR_SEPARATOR = R"(\,)";

/// Escape sequence for ':'.
static const char* ESCAPED_SECTION_SEPARATOR = R"(\:)";

/// Escape sequence for '='.
static const char* ESCAPED_KEY_VALUE_SEPARATOR = R"(\=)";

/// Reserved in metadata sequences for escaping other reserved values.
static const char METADATA_ESCAPE = '\\';

/// Reserved in metadata sequences to separate key/value pairs.
static const char PAIR_SEPARATOR = ',';

/// Reserved in metadata sequences to separate them from a preceding event and an optional terminal message.
static const char SECTION_SEPARATOR = ':';

/// String for boolean TRUE
static const char* BOOL_TRUE = "true";

/// String for boolean FALSE
static const char* BOOL_FALSE = "false";

LogEntry::LogEntry(const std::string& tag, const char* event) :
        m_tag(tag), m_hasMetadata(false), m_abortAfterEmission(false) {
    m_stream << tag << SECTION_SEPARATOR << event;
}

LogEntry::LogEntry(const std::string& tag, const std::string& event) :
        m_tag(tag), m_hasMetadata(false), m_abortAfterEmission(false) {
    m_stream << tag << SECTION_SEPARATOR << event;
}

LogEntry& LogEntry::d(const char* key, const char* value) {
    prefixKeyValuePair();
    m_stream << key << KEY_VALUE_SEPARATOR;
    appendEscapedString(value);
    return *this;
}

LogEntry& LogEntry::d(const char* key, const std::string& value) {
    return d(key, value.c_str());
}

LogEntry& LogEntry::d(const char* key, bool value) {
    return d(key, value ? BOOL_TRUE : BOOL_FALSE);
}

LogEntry& LogEntry::m(const char* message) {
    prefixMessage();
    m_stream << message;
    return *this;
}

LogEntry& LogEntry::m(const std::string& message) {
    prefixMessage();
    m_stream << message;
    return *this;
}

LogEntry& LogEntry::e(const char* key, int errnum) {
    prefixKeyValuePair();
    m_stream << key << KEY_VALUE_SEPARATOR << errnum;
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600 || __DARWIN_C_LEVEL >= 200112L) && !_GNU_SOURCE
    char errmsg[256];
    int ret = strerror_r(errnum, errmsg, sizeof(errmsg));
    if (ret == 0) {
        m_stream << SECTION_SEPARATOR << errmsg;
    }
#elif _GNU_SOURCE
    char buf[256];
    char* errmsg = strerror_r(errnum, buf, sizeof(buf));
    m_stream << SECTION_SEPARATOR << errmsg;
#endif
    return *this;
}

LogEntry& LogEntry::e(int errnum) {
    return e("errno", errnum);
}

const std::string& LogEntry::tag() const {
    return m_tag;
}

const char* LogEntry::c_str() const {
    return m_stream.c_str();
}

void LogEntry::prefixKeyValuePair() {
    if (m_hasMetadata) {
        m_stream << PAIR_SEPARATOR;
    } else {
        m_stream << SECTION_SEPARATOR;
        m_hasMetadata = true;
    }
}

void LogEntry::prefixMessage() {
    if (!m_hasMetadata) {
        m_stream << SECTION_SEPARATOR;
    }
    m_stream << SECTION_SEPARATOR;
}

void LogEntry::appendEscapedString(const char* in) {
    auto pos = in;
    // A little insurance against an infinite loop.
    auto maxCount = strlen(in);
    while (maxCount-- > 0 && *pos != 0) {
        auto next = strpbrk(pos, RESERVED_METADATA_CHARS);
        if (next) {
            m_stream.write(pos, next - pos);
            switch (*next) {
                case METADATA_ESCAPE:
                    m_stream << ESCAPED_METADATA_ESCAPE;
                    break;
                case PAIR_SEPARATOR:
                    m_stream << ESCAPED_PAIR_SEPARATOR;
                    break;
                case SECTION_SEPARATOR:
                    m_stream << ESCAPED_SECTION_SEPARATOR;
                    break;
                case KEY_VALUE_SEPARATOR:
                    m_stream << ESCAPED_KEY_VALUE_SEPARATOR;
                    break;
            }
            pos = next + 1;
        } else {
            m_stream << pos;
            return;
        }
    }
}

LogEntry& LogEntry::abortAfterEmission() {
    m_abortAfterEmission = true;
    return *this;
}

bool LogEntry::shouldAbortAfterEmission() const {
    return m_abortAfterEmission;
}

}  // namespace logger
}  // namespace engine
}  // namespace aace

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

#ifndef AACE_TEST_UNIT_CORE_MOCK_ENGINE_CONFIGURATION_H
#define AACE_TEST_UNIT_CORE_MOCK_ENGINE_CONFIGURATION_H

#include "AACE/Core/EngineConfiguration.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace aace {
namespace test {
namespace unit {
namespace core {

class MockEngineConfiguration : public aace::core::config::EngineConfiguration {
public:
    MockEngineConfiguration() {
    }

    MOCK_METHOD0(getStream, std::shared_ptr<std::istream>());
};

}  // namespace core
}  // namespace unit
}  // namespace test
}  // namespace aace

#endif  // AACE_TEST_UNIT_CORE_MOCK_ENGINE_CONFIGURATION_H

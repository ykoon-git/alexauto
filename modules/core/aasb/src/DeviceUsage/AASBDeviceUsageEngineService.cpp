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

#include <AASB/Engine/DeviceUsage/AASBDeviceUsage.h>
#include <AASB/Engine/DeviceUsage/AASBDeviceUsageEngineService.h>
#include <AACE/Engine/MessageBroker/MessageBrokerInterface.h>
#include <AACE/Engine/Core/EngineMacros.h>

namespace aasb {
namespace engine {
namespace deviceUsage {

// String to identify log entries originating from this file.
static const std::string TAG("aasb.propertyManager.AASBDeviceUsageEngineService");

// Minimum version this module supports
static const aace::engine::core::Version minRequiredVersion = VERSION("4.0");

// register the service
REGISTER_SERVICE(AASBDeviceUsageEngineService);

AASBDeviceUsageEngineService::AASBDeviceUsageEngineService(const aace::engine::core::ServiceDescription& description) :
        aace::engine::messageBroker::MessageHandlerEngineService(description, minRequiredVersion, {"DeviceUsage"}) {
}

bool AASBDeviceUsageEngineService::postRegister() {
    try {
        auto aasbServiceInterface =
            getContext()->getServiceInterface<aace::engine::messageBroker::MessageBrokerServiceInterface>(
                "aace.messageBroker");
        ThrowIfNull(aasbServiceInterface, "invalidAASBServiceInterface");

        // DeviceUsage
        if (isInterfaceEnabled("DeviceUsage")) {
            auto deviceUsage = AASBDeviceUsage::create(aasbServiceInterface->getMessageBroker());
            ThrowIfNull(deviceUsage, "invalidDeviceUsageHandler");
            getContext()->registerPlatformInterface(deviceUsage);
        }

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

}  // namespace deviceUsage
}  // namespace engine
}  // namespace aasb

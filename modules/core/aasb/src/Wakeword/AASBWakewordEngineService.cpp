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

#include <AASB/Engine/Wakeword/AASBWakeword.h>
#include <AASB/Engine/Wakeword/AASBWakewordEngineService.h>
#include <AACE/Engine/MessageBroker/MessageBrokerInterface.h>
#include <AACE/Engine/Core/EngineMacros.h>

namespace aasb {
namespace engine {
namespace wakeword {

// String to identify log entries originating from this file.
static const std::string TAG("aasb.Wakeword.AASBWakewordEngineService");

// Minimum version this module supports
static const aace::engine::core::Version minRequiredVersion = VERSION("4.0");

// register the service
REGISTER_SERVICE(AASBWakewordEngineService);

AASBWakewordEngineService::AASBWakewordEngineService(const aace::engine::core::ServiceDescription& description) :
        aace::engine::messageBroker::MessageHandlerEngineService(description, minRequiredVersion, {"Wakeword"}) {
}

bool AASBWakewordEngineService::postRegister() {
    try {
        auto aasbServiceInterface =
            getContext()->getServiceInterface<aace::engine::messageBroker::MessageBrokerServiceInterface>(
                "aace.messageBroker");
        ThrowIfNull(aasbServiceInterface, "invalidAASBServiceInterface");

        // Wakeword
        if (isInterfaceEnabled("Wakeword")) {
            auto wakeword = AASBWakeword::create(aasbServiceInterface->getMessageBroker());
            ThrowIfNull(wakeword, "invalidWakewordHandler");
            getContext()->registerPlatformInterface(wakeword);
        }

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

}  // namespace wakeword
}  // namespace engine
}  // namespace aasb

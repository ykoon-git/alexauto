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

#include <typeinfo>

#include "AACE/Engine/Location/LocationEngineService.h"
#include "AACE/Engine/Core/EngineMacros.h"

namespace aace {
namespace engine {
namespace location {

// String to identify log entries originating from this file.
static const std::string TAG("aace.location.LocationEngineService");

// register the service
REGISTER_SERVICE(LocationEngineService)

LocationEngineService::LocationEngineService(const aace::engine::core::ServiceDescription& description) :
        aace::engine::core::EngineService(description) {
}

bool LocationEngineService::registerPlatformInterface(
    std::shared_ptr<aace::core::PlatformInterface> platformInterface) {
    try {
        ReturnIf(registerPlatformInterfaceType<aace::location::LocationProvider>(platformInterface), true);
        return false;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "registerPlatformInterface").d("reason", ex.what()));
        return false;
    }
}

bool LocationEngineService::registerPlatformInterfaceType(
    std::shared_ptr<aace::location::LocationProvider> locationProviderPlatformInterface) {
    try {
        ThrowIfNotNull(m_locationProviderEngineImpl, "platformInterfaceAlreadyRegistered");

        m_locationProviderEngineImpl = LocationProviderEngineImpl::create(locationProviderPlatformInterface);
        ThrowIfNull(m_locationProviderEngineImpl, "createLocationProviderEngineImplFailed");

        ThrowIfNot(
            registerServiceInterface<LocationServiceInterface>(m_locationProviderEngineImpl),
            "registerLocationServiceInterfaceFailed");

        locationProviderPlatformInterface->setEngineInterface(m_locationProviderEngineImpl);
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "registerPlatformInterfaceType<LocationProvider>").d("reason", ex.what()));
        return false;
    }
}

bool LocationEngineService::shutdown() {
    if (m_locationProviderEngineImpl != nullptr) {
        m_locationProviderEngineImpl->shutdown();
        m_locationProviderEngineImpl.reset();
    }
    return true;
}

}  // namespace location
}  // namespace engine
}  // namespace aace

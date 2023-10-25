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

#include "AACE/Location/LocationProvider.h"

namespace aace {
namespace location {

LocationProvider::~LocationProvider() = default;  // key function

std::string LocationProvider::getCountry() {
    return "";
}

void LocationProvider::locationServiceAccessChanged(LocationServiceAccess access) {
    if (m_locationProviderEngineInterface != nullptr) {
        m_locationProviderEngineInterface->onLocationServiceAccessChanged(access);
    }
}

void LocationProvider::setEngineInterface(
    std::shared_ptr<LocationProviderEngineInterface> locationProviderEngineInterface) {
    m_locationProviderEngineInterface = locationProviderEngineInterface;
}

}  // namespace location
}  // namespace aace

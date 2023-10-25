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

#include "AACE/Engine/CarControl/ToggleController.h"

#include <AVSCommon/AVS/CapabilitySemantics/CapabilitySemantics.h>
#include <ToggleController/ToggleControllerAttributeBuilder.h>

#include "AACE/Engine/Core/EngineMacros.h"

namespace aace {
namespace engine {
namespace carControl {

/// String to identify log entries originating from this file.
static const std::string TAG("aace.engine.carControl.ToggleController");

std::shared_ptr<ToggleController> ToggleController::create(
    const json& controllerConfig,
    const std::string& endpointId,
    const std::string& interface,
    const AssetStore& assetStore) {
    try {
        std::string instance = controllerConfig.at("instance");
        ThrowIf(instance.empty(), "missingInstance");

        ThrowIfNot(controllerConfig.contains("capabilityResources"), "missingCapabilityResources");
        alexaClientSDK::avsCommon::utils::Optional<alexaClientSDK::avsCommon::avs::CapabilityResources>
            capabilityResources = getResources(controllerConfig.at("capabilityResources"), assetStore);
        ThrowIfNot(capabilityResources.hasValue(), "failedToParseCapabilityResourcesConfig");
        auto attributeBuilder =
            alexaClientSDK::capabilityAgents::toggleController::ToggleControllerAttributeBuilder::create();
        attributeBuilder->withCapabilityResources(capabilityResources.value());

        if (controllerConfig.contains("semantics")) {
            auto& semanticsJson = controllerConfig.at("semantics");
            alexaClientSDK::avsCommon::utils::Optional<
                alexaClientSDK::avsCommon::avs::capabilitySemantics::CapabilitySemantics>
                semantics = getSemantics(semanticsJson);
            ThrowIfNot(semantics.hasValue(), "failedToParseSemanticsConfig");
            attributeBuilder->withSemantics(semantics.value());
        }

        auto attributes = attributeBuilder->build();
        ThrowIfNot(attributes.hasValue(), "invalidAttributes");
        auto controller = std::shared_ptr<ToggleController>(
            new ToggleController(endpointId, interface, instance, attributes.value()));
        ThrowIfNull(controller, "createToggleControllerFailed");
        return controller;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return nullptr;
    }
}

ToggleController::ToggleController(
    const std::string& endpointId,
    const std::string& interface,
    const std::string& instance,
    ToggleControllerAttributes attributes) :
        PrimitiveController(endpointId, interface, instance), m_attributes(attributes) {
}

void ToggleController::build(
    std::shared_ptr<CarControlServiceInterface> carControlServiceInterface,
    std::unique_ptr<EndpointBuilderInterface>& builder) {
    m_carControlServiceInterface = carControlServiceInterface;
    builder->withToggleController(shared_from_this(), getInstance(), m_attributes, false, false, false);
}

std::pair<alexaClientSDK::avsCommon::avs::AlexaResponseType, std::string> ToggleController::setToggleState(
    const bool state,
    const AlexaStateChangeCauseType cause) {
    try {
        AACE_DEBUG(LX(TAG)
                       .d("cause", alexaClientSDK::avsCommon::sdkInterfaces::alexaStateChangeCauseTypeToString(cause))
                       .sensitive("endpointId", getEndpointId())
                       .sensitive("instance", getInstance())
                       .sensitive("state", state));
        if (state) {
            ThrowIfNot(
                m_carControlServiceInterface->turnToggleControllerOn(getEndpointId(), getInstance()),
                "turnToggleControllerOnFailed");
        } else {
            ThrowIfNot(
                m_carControlServiceInterface->turnToggleControllerOff(getEndpointId(), getInstance()),
                "turnToggleControllerOffFailed");
        }
        return std::make_pair(AlexaResponseType::SUCCESS, "");
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return std::make_pair(AlexaResponseType::INTERNAL_ERROR, "Something went wrong");
    }
}

std::pair<
    alexaClientSDK::avsCommon::avs::AlexaResponseType,
    alexaClientSDK::avsCommon::utils::Optional<ToggleController::ToggleState>>
ToggleController::getToggleState() {
    bool state;
    if (m_carControlServiceInterface->isToggleControllerOn(getEndpointId(), getInstance(), state)) {
        return std::make_pair(
            AlexaResponseType::SUCCESS,
            alexaClientSDK::avsCommon::utils::Optional<ToggleState>(ToggleState{
                state, alexaClientSDK::avsCommon::utils::timing::TimePoint::now(), std::chrono::milliseconds(0)}));
    } else {
        return std::make_pair(
            AlexaResponseType::INTERNAL_ERROR, alexaClientSDK::avsCommon::utils::Optional<ToggleState>());
    }
}

bool ToggleController::addObserver(
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::toggleController::ToggleControllerObserverInterface>
        observer) {
    AACE_WARN(LX(TAG).m("Unexpected addObserver()"));
    return false;
}

void ToggleController::removeObserver(
    const std::shared_ptr<
        alexaClientSDK::avsCommon::sdkInterfaces::toggleController::ToggleControllerObserverInterface>& observer) {
    AACE_WARN(LX(TAG).m("Unexpected addObserver()"));
}

}  // namespace carControl
}  // namespace engine
}  // namespace aace

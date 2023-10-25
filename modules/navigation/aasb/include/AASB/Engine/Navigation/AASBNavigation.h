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

#ifndef AASB_ENGINE_NAVIGATION_AASB_NAVIGATION_H
#define AASB_ENGINE_NAVIGATION_AASB_NAVIGATION_H

#include <AACE/Navigation/Navigation.h>
#include <AACE/Engine/MessageBroker/MessageBrokerInterface.h>

#include <string>
#include <memory>

namespace aasb {
namespace engine {
namespace navigation {

class AASBNavigation
        : public aace::navigation::Navigation
        , public std::enable_shared_from_this<AASBNavigation> {
private:
    AASBNavigation() = default;

    bool initialize(std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker);

public:
    static std::shared_ptr<AASBNavigation> create(
        std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker);

    // aace::navigation::Navigation
    void showPreviousWaypoints() override;
    void navigateToPreviousWaypoint() override;
    void showAlternativeRoutes(AlternateRouteType alternateRouteType) override;
    void controlDisplay(ControlDisplay controlDisplay) override;
    bool cancelNavigation() override;
    std::string getNavigationState() override;
    void startNavigation(const std::string& payload) override;
    void announceManeuver(const std::string& payload) override;
    void announceRoadRegulation(RoadRegulation roadRegulation) override;

private:
    std::weak_ptr<aace::engine::messageBroker::MessageBrokerInterface> m_messageBroker;

    std::string m_cachedNavState = R"({"shapes":[],"state":"NOT_NAVIGATING","waypoints":[]})";
};

}  // namespace navigation
}  // namespace engine
}  // namespace aasb

#endif  // AASB_ENGINE_NAVIGATION_AASB_NAVIGATION_H

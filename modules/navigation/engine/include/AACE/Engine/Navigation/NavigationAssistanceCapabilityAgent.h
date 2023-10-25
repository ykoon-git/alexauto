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

#ifndef AACE_ENGINE_NAVIGATION_NAVIGATION_ASSISTANCE_CAPABILITY_AGENT_H
#define AACE_ENGINE_NAVIGATION_NAVIGATION_ASSISTANCE_CAPABILITY_AGENT_H

#include <memory>
#include <unordered_set>

#include <AVSCommon/AVS/AgentId.h>
#include <AVSCommon/AVS/CapabilityAgent.h>
#include <AVSCommon/Utils/RequiresShutdown.h>
#include <AVSCommon/Utils/Threading/Executor.h>
#include <AVSCommon/SDKInterfaces/CapabilityConfigurationInterface.h>
#include <AVSCommon/AVS/CapabilityConfiguration.h>
#include <AVSCommon/SDKInterfaces/ContextManagerInterface.h>
#include <AVSCommon/SDKInterfaces/MessageSenderInterface.h>

#include <AACE/Engine/Metrics/DurationDataPointBuilder.h>
#include <AACE/Engine/Metrics/MetricRecorderServiceInterface.h>

#include "NavigationHandlerInterface.h"

namespace aace {
namespace engine {
namespace navigation {
namespace navigationassistance {

class NavigationAssistanceCapabilityAgent
        : public alexaClientSDK::avsCommon::avs::CapabilityAgent
        , public alexaClientSDK::avsCommon::sdkInterfaces::CapabilityConfigurationInterface
        , public alexaClientSDK::avsCommon::utils::RequiresShutdown
        , public std::enable_shared_from_this<NavigationAssistanceCapabilityAgent> {
public:
    static std::shared_ptr<NavigationAssistanceCapabilityAgent> create(
        const std::shared_ptr<aace::engine::navigation::NavigationHandlerInterface>& navigationHandler,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface>&
            exceptionSender,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface>& messageSender,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface>& contextManager,
        const std::shared_ptr<aace::engine::metrics::MetricRecorderServiceInterface>& metricRecorder);

    /**
     * Destructor.
     */
    virtual ~NavigationAssistanceCapabilityAgent() = default;

    /// @name CapabilityAgent/DirectiveHandlerInterface Functions
    /// @{
    void handleDirectiveImmediately(std::shared_ptr<alexaClientSDK::avsCommon::avs::AVSDirective> directive) override;
    void preHandleDirective(std::shared_ptr<DirectiveInfo> info) override;
    void handleDirective(std::shared_ptr<DirectiveInfo> info) override;
    void cancelDirective(std::shared_ptr<DirectiveInfo> info) override;
    alexaClientSDK::avsCommon::avs::DirectiveHandlerConfiguration getConfiguration() const override;
    /// @}};

    std::unordered_set<std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityConfiguration>>
    getCapabilityConfigurations() override;

    /**
     * Send success event corresponding to the @c EventName
     *
     * @param [in] event The @c EventName corresponding to the type of event to be sent.
     */
    void navigationEvent(aace::navigation::NavigationEngineInterface::EventName event);

    /**
     * Send failure event corresponding to the @c ErrorType
     *
     * @param [in] type The @c ErrorType corresponding to the type of error to be sent.
     * @param [in] code The @c ErrorCode corresponding to the code of error to be sent.
     * @param [in] description The @c string corresponding to the description of error to be sent.
     */
    void navigationError(
        aace::navigation::NavigationEngineInterface::ErrorType type,
        aace::navigation::NavigationEngineInterface::ErrorCode code,
        const std::string& description);

private:
    NavigationAssistanceCapabilityAgent(
        const std::shared_ptr<aace::engine::navigation::NavigationHandlerInterface>& navigationHandler,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface>&
            exceptionSender,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface>& messageSender,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface>& contextManager,
        const std::shared_ptr<aace::engine::metrics::MetricRecorderServiceInterface>& metricRecorder);

    // @name RequiresShutdown Functions
    /// @{
    void doShutdown() override;
    /// @}

    /**
     * Send @c ExceptionEncountered and report a failure to handle the @c AVSDirective.
     *
     * @param [in] info The @c AVSDirective that encountered the error and ancillary information.
     * @param [in] type The type of Exception that was encountered.
     * @param [in] message The error message to include in the ExceptionEncountered message.
     */
    void sendExceptionEncounteredAndReportFailed(
        std::shared_ptr<DirectiveInfo> info,
        const std::string& message,
        alexaClientSDK::avsCommon::avs::ExceptionErrorType type =
            alexaClientSDK::avsCommon::avs::ExceptionErrorType::INTERNAL_ERROR);

    /**
     * Remove a directive from the map of message IDs to @c DirectiveInfo instances.
     *
     * @param [in] info The @c DirectiveInfo containing the @c AVSDirective whose message ID is to be removed.
     */
    void removeDirective(std::shared_ptr<DirectiveInfo> info);

    /**
     * Send the handling completed notification and clean up the resources.
     *
     * @param [in] info The @c DirectiveInfo containing the @c AVSDirective and the @c DirectiveHandlerResultInterface.
     */
    void setHandlingCompleted(std::shared_ptr<DirectiveInfo> info);

    /**
     * This function handles a @c AnnounceManeuver directive.
     *
     * @param [in] info The @c DirectiveInfo containing the @c AVSDirective and the @c DirectiveHandlerResultInterface.
     */
    void handleAnnounceManeuverDirective(std::shared_ptr<DirectiveInfo> info);

    /**
     * This function handles a @c AnnounceRoadRegulation directive.
     *
     * @param [in] info The @c DirectiveInfo containing the @c AVSDirective and the @c DirectiveHandlerResultInterface.
     */
    void handleAnnounceRoadRegulationDirective(std::shared_ptr<DirectiveInfo> info);

    /**
     * This function handles any unknown directives received by the @c Navigation capability agent.
     *
     * @param [in] info The @c DirectiveInfo containing the @c AVSDirective and the @c DirectiveHandlerResultInterface.
     */
    void handleUnknownDirective(std::shared_ptr<DirectiveInfo> info);

    // Executor functions for sending navigation success and failure events.
    void executeNavigationEvent(aace::navigation::NavigationEngineInterface::EventName event);
    void executeNavigationError(
        aace::navigation::NavigationEngineInterface::ErrorType type,
        aace::navigation::NavigationEngineInterface::ErrorCode code,
        const std::string& description);

    // Helper functions to convert from navigation enum to string
    std::string getMode(aace::navigation::NavigationEngineInterface::EventName event);
    std::string getMode(aace::navigation::NavigationEngineInterface::ErrorType type);

    // Convert ErrorCode enum to string based on the error type
    std::string getRoadRegulationErrorCode(aace::navigation::NavigationEngineInterface::ErrorCode code);
    std::string getManeuverErrorCode(aace::navigation::NavigationEngineInterface::ErrorCode code);

    // Send a success event to notify AVS of successful handling of @c AnnounceManeuver, @c AnnounceRoadRegulation directive by the platform interface.
    void announceManeuverSucceeded(std::string maneuverType);
    void announceRoadRegulationSucceeded(std::string maneuverType);

    // Send a failure event to notify AVS of failed handling of @c AnnounceManeuver, @c AnnounceRoadRegulation directives by the platform interface.
    void announceRoadRegulationFailed(std::string code, std::string description, std::string maneuverType);
    void announceManeuverFailed(std::string code, std::string description, std::string maneuverType);

    /**
     * Set the agent for events based on directive.
     * 
     * @param [in] directiveName The name of the directive.
     * @param [in] agentId The @c AgentId::IdType of the agent to be set.
     */
    void setEventAgentByDirective(
        const std::string& directiveName,
        alexaClientSDK::avsCommon::avs::AgentId::IdType agentId);

    /**
     * Get the agent to tag the event.
     * 
     * @param [in] eventName The name of the event.
     */
    alexaClientSDK::avsCommon::avs::AgentId::IdType getEventAgent(const std::string& eventName);

    /**
     * @name Executor Thread Variables
     *
     * These member variables are only accessed by functions in the @c m_executor worker thread, and do not require any
     * synchronization.
     */
    /// @{
    /// A handler to be notified when a @c AnnounceManeuver, @ AnnounceRoadRegulation directive is received
    std::shared_ptr<NavigationHandlerInterface> m_navigationHandler;
    /// @}

    /// Set of capability configurations that will get published using the Capabilities API
    std::unordered_set<std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityConfiguration>>
        m_capabilityConfigurations;

    /// This is the worker thread for the @c Navigation CA.
    alexaClientSDK::avsCommon::utils::threading::Executor m_executor;

    /// The @c ContextManager that needs to be updated of the state.
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface> m_contextManager;

    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface> m_messageSender;

    /// Map that stores mapping of event name to agent id. Read and write access should be serialized by executor.
    std::unordered_map<std::string, alexaClientSDK::avsCommon::avs::AgentId::IdType> m_eventAgentMap;

    /// The metric recorder.
    std::shared_ptr<aace::engine::metrics::MetricRecorderServiceInterface> m_metricRecorder;

    /// Duration builder for navigation announcement latency metrics
    aace::engine::metrics::DurationDataPointBuilder m_announcementDurationData;
};

}  // namespace navigationassistance
}  // namespace navigation
}  // namespace engine
}  // namespace aace
#endif  // AACE_ENGINE_NAVIGATION_NAVIGATION_ASSISTANCE_CAPABILITY_AGENT_H

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
#include <iostream>
#include <stdexcept>

#include <string>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <AVSCommon/Utils/JSON/JSONUtils.h>

#include <AACE/Engine/Core/EngineMacros.h>
#include <AACE/Engine/Metrics/CounterDataPointBuilder.h>
#include <AACE/Engine/Metrics/MetricEventBuilder.h>
#include <AACE/Engine/Metrics/StringDataPointBuilder.h>

#include "AACE/Engine/Navigation/NavigationCapabilityAgent.h"

namespace aace {
namespace engine {
namespace navigation {

using AgentId = alexaClientSDK::avsCommon::avs::AgentId;
using NamespaceAndName = alexaClientSDK::avsCommon::avs::NamespaceAndName;
using namespace aace::engine::metrics;

// String to identify log entries originating from this file.
static const std::string TAG("aace.navigation.NavigationCapabilityAgent");

/// The namespace for this capability agent.
static const std::string NAMESPACE{"Navigation"};

/// The StartNavigation directive signature.
static const NamespaceAndName START_NAVIGATION{NAMESPACE, "StartNavigation", AgentId::AGENT_ID_ALL};

/// The ShowPreviousWaypoints directive signature.
static const NamespaceAndName SHOW_PREVIOUS_WAYPOINTS{NAMESPACE, "ShowPreviousWaypoints", AgentId::AGENT_ID_ALL};

/// The NavigateToPreviousWaypoint directive signature.
static const NamespaceAndName NAVIGATE_TO_PREVIOUS_WAYPOINT{NAMESPACE,
                                                            "NavigateToPreviousWaypoint",
                                                            AgentId::AGENT_ID_ALL};

/// The CancelNavigation directive signature.
static const NamespaceAndName CANCEL_NAVIGATION{NAMESPACE, "CancelNavigation", AgentId::AGENT_ID_ALL};

/// The NavigationState context signature.
static const NamespaceAndName NAVIGATION_STATE{NAMESPACE, "NavigationState", AgentId::AGENT_ID_ALL};

/// Navigation interface type
static const std::string NAVIGATION_CAPABILITY_INTERFACE_TYPE = "AlexaInterface";
/// Navigation interface name
static const std::string NAVIGATION_CAPABILITY_INTERFACE_NAME = "Navigation";
/// Navigation interface version
static const std::string NAVIGATION_CAPABILITY_INTERFACE_VERSION = "2.0";
/// Navigation interface provider name key
static const std::string CAPABILITY_INTERFACE_NAVIGATION_PROVIDER_NAME_KEY = "provider";

/// NavigationState state accepted values
static const std::string NAVIGATION_STATE_NAVIGATING = "NAVIGATING";
static const std::string NAVIGATION_STATE_NOT_NAVIGATING = "NOT_NAVIGATING";
static const std::string NAVIGATION_STATE_UNKNOWN = "UNKNOWN";

// Waypoint Type accepted values
static const std::string WAYPOINT_TYPE_SOURCE = "SOURCE";
static const std::string WAYPOINT_TYPE_INTERIM = "INTERIM";
static const std::string WAYPOINT_TYPE_DESTINATION = "DESTINATION";

/// Default when provided NavigationState is empty
// clang-format off
static const std::string DEFAULT_NAVIGATION_STATE_PAYLOAD = R"({
	"state": ")" + NAVIGATION_STATE_NOT_NAVIGATING + R"(",
	"waypoints": [],
	"shapes": []
})";

//// max number of shapes allowable in context
static const int MAXIMUM_SHAPES_IN_CONTEXT = 100;

// Navigation Event Strings
static const std::string START_NAVIGATION_SUCCESS = "StartNavigationSuccess";
static const std::string SHOW_PREVIOUS_WAYPOINTS_SUCCESS = "ShowPreviousWaypointsSuccess";
static const std::string NAVIGATE_TO_PREVIOUS_WAYPOINTS_SUCCESS = "NavigateToPreviousWaypointSuccess";
static const std::string START_NAVIGATION_ERROR = "StartNavigationError";
static const std::string SHOW_PREVIOUS_WAYPOINTS_ERROR = "ShowPreviousWaypointsError";
static const std::string NAVIGATE_TO_PREVIOUS_WAYPOINT_ERROR = "NavigateToPreviousWaypointError";

/// Prefix for metrics emitted from the Navigation CA
static const std::string METRIC_PREFIX = "NAVIGATION-";

/// Navigation latency metric
static const std::string METRIC_NAVIGATION_LATENCY = "NavigationControlLatencyValue";

/// Navigation success count metric
static const std::string METRIC_NAVIGATION_SUCCESS = "NavigationControlSuccessCount";

/// Navigation type metric dimension
static const std::string METRIC_NAVIGATION_CONTROL_TYPE = "NavigationControlEventType";

/// Navigation error count metric
static const std::string METRIC_NAVIGATION_ERROR = "NavigationControlErrorCount";

/// Navigation error code metric dimension
static const std::string METRIC_NAVIGATION_ERROR_CODE = "NavigationControlErrorCode";

/**
 * Creates and records a metric.
 * 
 * @param metricRecorder The @c MetricRecorderInterface that records Metric
 *        events
 * @param activityName The activity name of the metric
 * @param agentId The ID of the agent associated with the metric
 * @param dataPoints The @c DataPoint objects to include in the MetricEvent
*/
static void submitMetric(
    const std::shared_ptr<MetricRecorderServiceInterface>& metricRecorder,
    alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
    const std::string& activityName,
    const std::vector<DataPoint>& dataPoints) {
    auto metricBuilder = MetricEventBuilder{}.withSourceName(activityName);
    metricBuilder.withAgentId(agentId);
    metricBuilder.addDataPoints(dataPoints);
    auto metric = metricBuilder.build();
    try {
        recordMetric(metricRecorder, metricBuilder.build());
    } catch (std::invalid_argument& ex) {
        AACE_ERROR(LX(TAG).m("Failed to record metric").d("reason", ex.what()));
    }
}

/**
 * Creates and records a metric for successful Navigation operations.
 * 
 * @param metricRecorder The @c MetricRecorderInterface that records Metric
 *        events
 * @param agentId The ID of the agent associated with the event
 * @param latencyDataPoint The @c DataPoint containing the navigation operation
 *        latency
 * @param eventType The type of the navigation operation. Use the directive
 *        name.
*/
static void submitNavigationControlSuccessMetric(
    const std::shared_ptr<MetricRecorderServiceInterface>& metricRecorder,
    alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
    const DataPoint& latencyDataPoint,
    const std::string& eventType) {
    std::vector<DataPoint> dps = {
        latencyDataPoint,
        CounterDataPointBuilder{}.withName(METRIC_NAVIGATION_SUCCESS).increment(1).build(),
        CounterDataPointBuilder{}.withName(METRIC_NAVIGATION_ERROR).increment(0).build(),
        StringDataPointBuilder{}
            .withName(METRIC_NAVIGATION_CONTROL_TYPE)
            .withValue(eventType)
            .build()};
    submitMetric(metricRecorder, agentId, METRIC_PREFIX + eventType, dps);
}

/**
 * Creates and records a metric for unsuccessful Navigation operations.
 * 
 * @param metricRecorder The @c MetricRecorderInterface that records Metric
 *        events
 * @param agentId The ID of the agent associated with the event
 * @param latencyDataPoint The @c DataPoint containing the navigation operation
 *        latency
 * @param eventType The type of the navigation operation. Use the directive
 *        name.
 * @param error The error reason. Use accepted values from events.
*/
static void submitNavigationControlErrorMetric(
    const std::shared_ptr<MetricRecorderServiceInterface>& metricRecorder,
    alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
    const DataPoint& latencyDataPoint,
    const std::string& eventType,
    const std::string& error) {
    std::vector<DataPoint> dps = {
        latencyDataPoint,
        CounterDataPointBuilder{}.withName(METRIC_NAVIGATION_SUCCESS).increment(0).build(),
        CounterDataPointBuilder{}.withName(METRIC_NAVIGATION_ERROR).increment(1).build(),
        StringDataPointBuilder{}
            .withName(METRIC_NAVIGATION_CONTROL_TYPE)
            .withValue(eventType)
            .build(),
        StringDataPointBuilder{}
            .withName(METRIC_NAVIGATION_ERROR_CODE)
            .withValue(error)
            .build()};
    submitMetric(metricRecorder, agentId, METRIC_PREFIX + eventType, dps);
}

/**
 * Creates the Navigation capability configuration.
 *
 * @return The Navigation capability configuration.
 */
static std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityConfiguration> getNavigationCapabilityConfiguration( const std::string& navigationProviderName );

std::shared_ptr<NavigationCapabilityAgent> NavigationCapabilityAgent::create( const std::shared_ptr<NavigationHandlerInterface>& navigationHandler, const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface>& exceptionSender, const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface>& messageSender,
const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface>& contextManager, const std::shared_ptr<aace::engine::metrics::MetricRecorderServiceInterface>& metricRecorder, const std::string& navigationProviderName )
{
    try
    {
        ThrowIfNull( navigationHandler, "nullNavigationHandler" );
        ThrowIfNull( exceptionSender, "nullExceptionSender" );
        ThrowIfNull( messageSender, "nullMessageSender" );
        ThrowIfNull( contextManager, "nullContextManager" );
        ThrowIfNull( metricRecorder, "nullMetricRecorder" );

        auto navigationCapabilityAgent = std::shared_ptr<NavigationCapabilityAgent>( new NavigationCapabilityAgent( navigationHandler, exceptionSender, contextManager, messageSender, metricRecorder, navigationProviderName ) );

        ThrowIfNull( navigationCapabilityAgent, "nullNavigationCapabilityAgent" );

        contextManager->setStateProvider( NAVIGATION_STATE, navigationCapabilityAgent );

        return navigationCapabilityAgent;
    }
    catch( std::exception& ex ) {
        AACE_ERROR(LX(TAG,"create").d("reason", ex.what()));
        return nullptr;
    }
}

void NavigationCapabilityAgent::handleDirectiveImmediately( std::shared_ptr<alexaClientSDK::avsCommon::avs::AVSDirective> directive ) {
    handleDirective( std::make_shared<DirectiveInfo>( directive, nullptr ) );
}

void NavigationCapabilityAgent::preHandleDirective( std::shared_ptr<DirectiveInfo> info )
{
    // Intentional no-op (removeDirective() can only be called from handleDirective() and cancelDirective() functions).
}

void NavigationCapabilityAgent::handleDirective( std::shared_ptr<DirectiveInfo> info ) {
    try
    {
        ThrowIfNot( info && info->directive, "nullDirectiveInfo" );
        if ( info->directive->getName() == SHOW_PREVIOUS_WAYPOINTS.name ) {
            handleShowPreviousWaypointsDirective( info );
        }
        else if ( info->directive->getName() == NAVIGATE_TO_PREVIOUS_WAYPOINT.name ) {
            handleNavigateToPreviousWaypointDirective( info );
        }
        else if( info->directive->getName() == CANCEL_NAVIGATION.name ) {
            handleCancelNavigationDirective( info );
        }
        else if( info->directive->getName() == START_NAVIGATION.name ) {
            handleStartNavigationDirective( info );
        }
        else {
            handleUnknownDirective( info );
        }
    }
    catch( std::exception& ex ) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
    }
}

void NavigationCapabilityAgent::cancelDirective( std::shared_ptr<DirectiveInfo> info ) {
    removeDirective( info );
}

alexaClientSDK::avsCommon::avs::DirectiveHandlerConfiguration NavigationCapabilityAgent::getConfiguration() const
{
    alexaClientSDK::avsCommon::avs::DirectiveHandlerConfiguration configuration;
    auto audioVisualBlockingPolicy = alexaClientSDK::avsCommon::avs::BlockingPolicy( alexaClientSDK::avsCommon::avs::BlockingPolicy::MEDIUMS_AUDIO_AND_VISUAL, true  );
    configuration[CANCEL_NAVIGATION] = audioVisualBlockingPolicy;
    configuration[START_NAVIGATION] = audioVisualBlockingPolicy;
    configuration[SHOW_PREVIOUS_WAYPOINTS] = audioVisualBlockingPolicy;
    configuration[NAVIGATE_TO_PREVIOUS_WAYPOINT] = audioVisualBlockingPolicy;
    return configuration;
}

NavigationCapabilityAgent::NavigationCapabilityAgent(
        const std::shared_ptr<aace::engine::navigation::NavigationHandlerInterface>& navigationHandler,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface>& exceptionSender,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface>& contextManager,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface>& messageSender,
        const std::shared_ptr<aace::engine::metrics::MetricRecorderServiceInterface>& metricRecorder,
        const std::string& navigationProviderName ) :
            alexaClientSDK::avsCommon::avs::CapabilityAgent{ NAMESPACE, exceptionSender },
            alexaClientSDK::avsCommon::utils::RequiresShutdown{"NavigationCapabilityAgent"},
            m_navigationHandler{ navigationHandler },
            m_contextManager{ contextManager },
            m_messageSender{ messageSender },
            m_metricRecorder{metricRecorder} {
        m_capabilityConfigurations.insert( getNavigationCapabilityConfiguration( navigationProviderName ) );
}

std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityConfiguration> getNavigationCapabilityConfiguration( const std::string& navigationProviderName ) {

    std::unordered_map<std::string, std::string> configMap;
    configMap.insert( {alexaClientSDK::avsCommon::avs::CAPABILITY_INTERFACE_TYPE_KEY, NAVIGATION_CAPABILITY_INTERFACE_TYPE} );
    configMap.insert( {alexaClientSDK::avsCommon::avs::CAPABILITY_INTERFACE_NAME_KEY, NAVIGATION_CAPABILITY_INTERFACE_NAME} );
    configMap.insert( {alexaClientSDK::avsCommon::avs::CAPABILITY_INTERFACE_VERSION_KEY, NAVIGATION_CAPABILITY_INTERFACE_VERSION} );

    if ( navigationProviderName.empty() ) {
        AACE_WARN(LX(TAG,"getNavigationCapabilityConfigurationWarning").d("reason", "navigationProviderNameEmpty"));
        return std::make_shared<alexaClientSDK::avsCommon::avs::CapabilityConfiguration>( configMap );
    }

    configMap.insert( {alexaClientSDK::avsCommon::avs::CAPABILITY_INTERFACE_CONFIGURATIONS_KEY, "{ \"" + CAPABILITY_INTERFACE_NAVIGATION_PROVIDER_NAME_KEY + "\" : \"" + navigationProviderName + "\" }"} );

    return std::make_shared<alexaClientSDK::avsCommon::avs::CapabilityConfiguration>( configMap );
}

void NavigationCapabilityAgent::doShutdown() {
    m_executor.shutdown();
    m_contextManager->setStateProvider( NAVIGATION_STATE, nullptr );
    m_navigationHandler.reset();
    m_messageSender.reset();
    m_contextManager.reset();
}

void NavigationCapabilityAgent::sendExceptionEncounteredAndReportFailed( std::shared_ptr<DirectiveInfo> info, const std::string& message, alexaClientSDK::avsCommon::avs::ExceptionErrorType type )
{
    m_exceptionEncounteredSender->sendExceptionEncountered( info->directive->getUnparsedDirective(), type, message );

    if( info && info->result ) {
        info->result->setFailed( message );
    }

    removeDirective( info );
}

void NavigationCapabilityAgent::removeDirective( std::shared_ptr<DirectiveInfo> info ) {
    /*
     * Check result too, to catch cases where DirectiveInfo was created locally, without a nullptr result.
     * In those cases there is no messageId to remove because no result was expected.
     */
    if( info->directive && info->result ) {
        alexaClientSDK::avsCommon::avs::CapabilityAgent::removeDirective( info->directive->getMessageId() );
    }
}

void NavigationCapabilityAgent::setHandlingCompleted( std::shared_ptr<DirectiveInfo> info )
{
    if( info && info->result ) {
        info->result->setCompleted();
    }

    removeDirective( info );
}

void NavigationCapabilityAgent::handleStartNavigationDirective( std::shared_ptr<DirectiveInfo> info )
{
    std::string payload = info->directive->getPayload();
    rapidjson::Document json;
    rapidjson::ParseResult result = json.Parse( &payload[0]);
    if( !result ) {
        AACE_ERROR(LX(TAG, "handleStartNavigationDirective").d("reason", rapidjson::GetParseError_En(result.Code())).d("messageId", info->directive->getMessageId()));
        sendExceptionEncounteredAndReportFailed( info, "Unable to parse payload", alexaClientSDK::avsCommon::avs::ExceptionErrorType::UNEXPECTED_INFORMATION_RECEIVED);
        return;
    }
    if( !json.HasMember( "waypoints" ) ) {
        AACE_ERROR(LX(TAG, "handleStartNavigationDirective").d("reason", "missing waypoints list"));
        sendExceptionEncounteredAndReportFailed( info, "Missing waypoints list", alexaClientSDK::avsCommon::avs::ExceptionErrorType::UNEXPECTED_INFORMATION_RECEIVED);
        return;
    }

    auto agentId = info->directive->getAgentId();
    m_executor.submit([this, info, agentId]() {
        m_startNavDurationData.startTimer();
        m_navigationHandler->startNavigation(agentId, info->directive->getPayload());
        setHandlingCompleted( info );
    });
}

void NavigationCapabilityAgent::handleShowPreviousWaypointsDirective( std::shared_ptr<DirectiveInfo> info )
{
    auto agentId = info->directive->getAgentId();
    m_executor.submit([this, info, agentId]() {
        m_showWaypointsDurationData.startTimer();
        m_navigationHandler->showPreviousWaypoints(agentId);
        setHandlingCompleted( info );
    });
}

void NavigationCapabilityAgent::handleNavigateToPreviousWaypointDirective( std::shared_ptr<DirectiveInfo> info )
{
    auto agentId = info->directive->getAgentId();
    m_executor.submit([this, info, agentId]() {
        m_navToPreviousDurationData.startTimer();
        m_navigationHandler->navigateToPreviousWaypoint(agentId);
        setHandlingCompleted( info );
    });
}

void NavigationCapabilityAgent::handleCancelNavigationDirective( std::shared_ptr<DirectiveInfo> info )
{
    auto agentId = info->directive->getAgentId();
    m_executor.submit([this, info, agentId]() {
        m_navigationHandler->cancelNavigation(agentId);
        setHandlingCompleted( info );
    });
}

void NavigationCapabilityAgent::handleUnknownDirective( std::shared_ptr<DirectiveInfo> info )
{
    AACE_ERROR( LX(TAG)
                    .d("reason", "unknownDirective")
                    .d("namespace", info->directive->getNamespace())
                    .d("name", info->directive->getName()) );

    m_executor.submit([this, info] {
        const std::string exceptionMessage =
            "unexpected directive " + info->directive->getNamespace() + ":" + info->directive->getName();

        sendExceptionEncounteredAndReportFailed( info, exceptionMessage, alexaClientSDK::avsCommon::avs::ExceptionErrorType::UNEXPECTED_INFORMATION_RECEIVED );
    });
}

void NavigationCapabilityAgent::provideState( const NamespaceAndName& stateProviderName, const unsigned int stateRequestToken ) {
     m_executor.submit( [this, stateProviderName, stateRequestToken] {
        executeProvideState( stateProviderName, stateRequestToken );
    });
}

void NavigationCapabilityAgent::navigationEvent( AgentId::IdType agentId, aace::navigation::NavigationEngineInterface::EventName event )
{
    m_executor.submit( [this, agentId, event] {
        executeNavigationEvent( agentId, event );
    });
}

void NavigationCapabilityAgent::navigationError( AgentId::IdType agentId, aace::navigation::NavigationEngineInterface::ErrorType type, aace::navigation::NavigationEngineInterface::ErrorCode code, const std::string& description )
{
    m_executor.submit( [this, agentId, type, code, description] {
        executeNavigationError( agentId, type, code, description );
    });
}

void NavigationCapabilityAgent::executeProvideState( const NamespaceAndName& stateProviderName, const unsigned int stateRequestToken )
{
    try
    {
        ThrowIfNull( m_contextManager, "contextManagerIsNull" );
        bool payloadChanged = false;
        std::string payload;
        auto agentId = stateProviderName.getAgentId();

        payload = m_navigationHandler->getNavigationState(agentId);

        if( payload.empty() && m_navigationStatePayload.compare(DEFAULT_NAVIGATION_STATE_PAYLOAD) != 0 ) {
            payload = DEFAULT_NAVIGATION_STATE_PAYLOAD;
            payloadChanged = true;
        } else if ( !payload.empty() && payload.compare( m_navigationStatePayload ) != 0 ){
            payloadChanged = true;
        }

        if( payloadChanged ) {
            if ( isNavigationStateValid( payload )  ) {
                // set the context NavigationState
                ThrowIf( m_contextManager->setState( NAVIGATION_STATE, m_navigationStatePayload, alexaClientSDK::avsCommon::avs::StateRefreshPolicy::SOMETIMES, stateRequestToken ) != alexaClientSDK::avsCommon::sdkInterfaces::SetStateResult::SUCCESS, "contextManagerSetStateFailed" );
            }
        } else {
            // send empty if no change
            ThrowIf( m_contextManager->setState( NAVIGATION_STATE, "", alexaClientSDK::avsCommon::avs::StateRefreshPolicy::SOMETIMES, stateRequestToken ) != alexaClientSDK::avsCommon::sdkInterfaces::SetStateResult::SUCCESS, "contextManagerSetStateEmptyPayloadFailed" );
        }
    }
    catch( std::exception& ex ) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
    }
}

void NavigationCapabilityAgent::executeNavigationEvent( AgentId::IdType agentId, aace::navigation::NavigationEngineInterface::EventName event )
{
    switch( event ){
        case aace::navigation::NavigationEngineInterface::EventName::NAVIGATION_STARTED:
            startNavigationSuccess(agentId);
            break;
        case aace::navigation::NavigationEngineInterface::EventName::PREVIOUS_WAYPOINTS_SHOWN:
            showPreviousWaypointsSuccess(agentId);
            break;
        case aace::navigation::NavigationEngineInterface::EventName::PREVIOUS_NAVIGATION_STARTED:
            navigateToPreviousWaypointSuccess(agentId);
            break;
        default:
            AACE_ERROR(LX(TAG).d("reason","invalidEventType"));
            break;
    }
}

void NavigationCapabilityAgent::executeNavigationError( AgentId::IdType agentId, aace::navigation::NavigationEngineInterface::ErrorType type, aace::navigation::NavigationEngineInterface::ErrorCode code, const std::string& description )
{
    std::string errorCode;
    switch( type ) {
        case aace::navigation::NavigationEngineInterface::ErrorType::NAVIGATION_START_FAILED :
            errorCode = getNavigationErrorCode( code );
            startNavigationError( agentId, errorCode, description );
            break;
        case aace::navigation::NavigationEngineInterface::ErrorType::SHOW_PREVIOUS_WAYPOINTS_FAILED:
            errorCode = getWaypointErrorCode( code );
            showPreviousWaypointsError( agentId, errorCode, description );
            break;
        case aace::navigation::NavigationEngineInterface::ErrorType::PREVIOUS_NAVIGATION_START_FAILED:
            errorCode = getWaypointErrorCode( code );
            navigateToPreviousWaypointError( agentId, errorCode, description );
            break;
        default:
            AACE_ERROR(LX(TAG).d("reason","invalidErrorType"));
            break;
    }
}

std::string NavigationCapabilityAgent::getNavigationErrorCode( aace::navigation::NavigationEngineInterface::ErrorCode code ) {
    std::string errorCodeString;
    switch ( code ) {
        case aace::navigation::NavigationEngineInterface::ErrorCode::ROUTE_NOT_FOUND:
            errorCodeString = "ROUTE_NOT_FOUND";
            break;
        default:
            errorCodeString = "INTERNAL_ERROR";
            break;
    }
    return errorCodeString;
}

std::string NavigationCapabilityAgent::getWaypointErrorCode( aace::navigation::NavigationEngineInterface::ErrorCode code ) {
    std::string errorCodeString;
    switch ( code ) {
        case aace::navigation::NavigationEngineInterface::ErrorCode::NO_PREVIOUS_WAYPOINTS:
            errorCodeString = "NO_PREVIOUS_WAYPOINTS";
            break;
        default:
            errorCodeString = "INTERNAL_ERROR";
            break;
    }
    return errorCodeString;
}

//
// Navigation success event handling
//

void NavigationCapabilityAgent::startNavigationSuccess(AgentId::IdType agentId) {
    DataPoint latencyDataPoint =
            m_startNavDurationData.withName(METRIC_NAVIGATION_LATENCY).stopTimer().build();
    submitNavigationControlSuccessMetric(m_metricRecorder, agentId, latencyDataPoint, START_NAVIGATION.name);

    std::string navigationState;
    navigationState = m_navigationHandler->getNavigationState(agentId);
    rapidjson::Document context( rapidjson::kObjectType );
    rapidjson::Document payload( rapidjson::kObjectType );
    rapidjson::Document::AllocatorType& allocator = payload.GetAllocator();

    context.Parse( navigationState.c_str() );

    rapidjson::Document waypointsArray( rapidjson::kArrayType );
    if ( context.HasMember( "waypoints" ) ) {
        for (auto &point : context["waypoints"].GetArray()) {
            waypointsArray.PushBack(point, allocator);
        }
    }
    payload.AddMember( "waypoints", waypointsArray, allocator );

    rapidjson::Document shapesArray( rapidjson::kArrayType );
    if ( context.HasMember( "shapes" ) ) {
        for ( auto& point : context["shapes"].GetArray() ) {
            shapesArray.PushBack( point, allocator );
        }
    }
    payload.AddMember( "shapes", shapesArray, allocator );

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
    ThrowIfNot(payload.Accept( writer ), "failedToWriteJsonDocument" );
    auto navEvent = buildJsonEventString( START_NAVIGATION_SUCCESS, "", buffer.GetString() );
    auto request = std::make_shared<alexaClientSDK::avsCommon::avs::MessageRequest>(agentId,  navEvent.second);
    m_messageSender->sendMessage( request );
}

void NavigationCapabilityAgent::showPreviousWaypointsSuccess(AgentId::IdType agentId)
{
    DataPoint latencyDataPoint =
            m_showWaypointsDurationData.withName(METRIC_NAVIGATION_LATENCY).stopTimer().build();
    submitNavigationControlSuccessMetric(m_metricRecorder, agentId, latencyDataPoint, SHOW_PREVIOUS_WAYPOINTS.name);

    rapidjson::Document payload( rapidjson::kObjectType );
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
    ThrowIfNot(payload.Accept( writer ), "failedToWriteJsonDocument" );

    auto navEvent = buildJsonEventString( SHOW_PREVIOUS_WAYPOINTS_SUCCESS, "", buffer.GetString() );
    auto request = std::make_shared<alexaClientSDK::avsCommon::avs::MessageRequest>(agentId,  navEvent.second);
    m_messageSender->sendMessage( request );
}

void NavigationCapabilityAgent::navigateToPreviousWaypointSuccess(AgentId::IdType agentId)
{
    DataPoint latencyDataPoint =
            m_navToPreviousDurationData.withName(METRIC_NAVIGATION_LATENCY).stopTimer().build();
    submitNavigationControlSuccessMetric(m_metricRecorder, agentId, latencyDataPoint, NAVIGATE_TO_PREVIOUS_WAYPOINT.name);

    std::string navigationState;
    navigationState = m_navigationHandler->getNavigationState(agentId);
    rapidjson::Document context( rapidjson::kObjectType );
    rapidjson::Document payload( rapidjson::kObjectType );
    rapidjson::Document::AllocatorType& allocator = payload.GetAllocator();

    context.Parse( navigationState.c_str() );

    rapidjson::Document waypointObject( rapidjson::kObjectType );
    if ( context.HasMember( "waypoints" ) && context["waypoints"].GetArray().Size() > 0 ) {
        payload.AddMember( "waypoint", context["waypoints"].GetArray()[0].GetObject(), allocator );
    } else {
        payload.AddMember( "waypoint", waypointObject, allocator );
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
    ThrowIfNot(payload.Accept( writer ), "failedToWriteJsonDocument" );

    auto navEvent = buildJsonEventString( NAVIGATE_TO_PREVIOUS_WAYPOINTS_SUCCESS, "", buffer.GetString() );
    auto request = std::make_shared<alexaClientSDK::avsCommon::avs::MessageRequest>(agentId,  navEvent.second);
    m_messageSender->sendMessage( request );
}

//
// Navigation fail event handling
//

void NavigationCapabilityAgent::startNavigationError( AgentId::IdType agentId, std::string code, std::string description)
{
    DataPoint latencyDataPoint =
            m_startNavDurationData.withName(METRIC_NAVIGATION_LATENCY).stopTimer().build();
    submitNavigationControlErrorMetric(m_metricRecorder, agentId, latencyDataPoint, START_NAVIGATION.name, code);

    rapidjson::Document payload( rapidjson::kObjectType );
    rapidjson::Document::AllocatorType& allocator = payload.GetAllocator();

    payload.AddMember( "code", rapidjson::Value( code.c_str(), allocator ), allocator );
    payload.AddMember( "description", rapidjson::Value( description.c_str(), allocator ), allocator );

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
    ThrowIfNot(payload.Accept( writer ), "failedToWriteJsonDocument" );

    auto navEvent = buildJsonEventString( START_NAVIGATION_ERROR, "", buffer.GetString() );
    auto request = std::make_shared<alexaClientSDK::avsCommon::avs::MessageRequest>(agentId,  navEvent.second);
    m_messageSender->sendMessage( request );
}

void NavigationCapabilityAgent::navigateToPreviousWaypointError( AgentId::IdType agentId, std::string code, std::string description )
{
    DataPoint latencyDataPoint =
            m_navToPreviousDurationData.withName(METRIC_NAVIGATION_LATENCY).stopTimer().build();
    submitNavigationControlErrorMetric(m_metricRecorder, agentId, latencyDataPoint, NAVIGATE_TO_PREVIOUS_WAYPOINT.name, code);

    rapidjson::Document payload( rapidjson::kObjectType );
    rapidjson::Document::AllocatorType& allocator = payload.GetAllocator();
    payload.AddMember("code", rapidjson::Value( code.c_str(), allocator ), allocator );
    payload.AddMember("description", rapidjson::Value( description.c_str(), allocator ), allocator );

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
    ThrowIfNot(payload.Accept( writer ), "failedToWriteJsonDocument" );

    auto navEvent = buildJsonEventString( NAVIGATE_TO_PREVIOUS_WAYPOINT_ERROR, "", buffer.GetString() );
    auto request = std::make_shared<alexaClientSDK::avsCommon::avs::MessageRequest>(agentId,  navEvent.second);
    m_messageSender->sendMessage( request );
}

void NavigationCapabilityAgent::showPreviousWaypointsError( AgentId::IdType agentId, std::string code, std::string description )
{
    DataPoint latencyDataPoint =
            m_showWaypointsDurationData.withName(METRIC_NAVIGATION_LATENCY).stopTimer().build();
    submitNavigationControlErrorMetric(m_metricRecorder, agentId, latencyDataPoint, SHOW_PREVIOUS_WAYPOINTS.name, code);

    rapidjson::Document payload( rapidjson::kObjectType );
    rapidjson::Document::AllocatorType& allocator = payload.GetAllocator();
    payload.AddMember("code", rapidjson::Value( code.c_str(), allocator ), allocator );
    payload.AddMember("description", rapidjson::Value( description.c_str(), allocator ), allocator );

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
    ThrowIfNot(payload.Accept( writer ), "failedToWriteJsonDocument" );

    auto navEvent = buildJsonEventString( SHOW_PREVIOUS_WAYPOINTS_ERROR, "", buffer.GetString() );
    auto request = std::make_shared<alexaClientSDK::avsCommon::avs::MessageRequest>(agentId,  navEvent.second);
    m_messageSender->sendMessage( request );
}

bool NavigationCapabilityAgent::isNavigationStateValid( std::string navigationState )
{
    AACE_VERBOSE(LX(TAG).d("navigationState",navigationState));
    try
    {
        rapidjson::Document document;
        document.Parse<0>( navigationState.c_str() );

        if( document.HasParseError() ) {
            rapidjson::ParseErrorCode ok = document.GetParseError();
            AACE_ERROR(LX(TAG).d( "HasParseError", GetParseError_En(ok) ) );
            Throw( "parseError" );
        }

        ThrowIfNot( document.HasMember("state"), "stateKeyMissing");

        if( document[ "state" ].IsNull() || !document[ "state" ].IsString() ) {
            Throw( "stateNotValid" );
        }
        std::string state = document[ "state" ].GetString();
        if( ( state.compare(NAVIGATION_STATE_NAVIGATING) != 0 )
           && ( state.compare(NAVIGATION_STATE_NOT_NAVIGATING) != 0 )
           && ( state.compare(NAVIGATION_STATE_UNKNOWN) != 0 ) ) {
            Throw( "stateValueNotValid" );
        }
        if ( document.HasMember("waypoints") ) {
            if( !document[ "waypoints" ].IsArray() ) {
                Throw( "waypointsArrayNotValid" );
            }

            auto waypoints = document["waypoints"].GetArray();
            for ( size_t i = 0; i < waypoints.Size() ; i++ ) {
                auto waypoint  = waypoints[i].GetObject();
                ThrowIfNot( waypoint.HasMember("type"), "waypointTypeMissing");

                std::string waypointType = waypoint["type"].GetString();
                if( waypoint[ "type" ].IsNull() || !waypoint[ "type" ].IsString() ) {
                    Throw( "waypointTypeNotValid" );
                }
                if( ( waypointType.compare(WAYPOINT_TYPE_SOURCE) != 0 ) &&
                    ( waypointType.compare(WAYPOINT_TYPE_INTERIM) != 0 ) &&
                    ( waypointType.compare(WAYPOINT_TYPE_DESTINATION) != 0 ) ) {
                    Throw( "waypointTypeValueNotValid" );
                }
                if ( waypoint.HasMember("estimatedTimeOfArrival") ) {
                    auto estimatedTimeOfArrival = waypoint["estimatedTimeOfArrival"].GetObject();
                    ThrowIfNot( estimatedTimeOfArrival.HasMember("predicted"), "predictedTimeOfArrivalMissing");
                    if ( ( estimatedTimeOfArrival.HasMember("ideal") && !estimatedTimeOfArrival["ideal"].IsString() ) ||
                         ( estimatedTimeOfArrival.HasMember("predicted") && !estimatedTimeOfArrival["predicted"].IsString() ) ) {

                        Throw( "estimatedTimeOfArrivalNotString" );
                    }
                }
                if ( waypoint.HasMember("address") ) {
                    auto address = waypoint["address"].GetObject();
                    if ( ( address.HasMember("addressLine1") && !address["addressLine1"].IsString() ) ||
                         ( address.HasMember("addressLine2") && !address["addressLine2"].IsString() ) ||
                         ( address.HasMember("addressLine3") && !address["addressLine3"].IsString() ) ||
                         ( address.HasMember("city") && !address["city"].IsString() ) ||
                         ( address.HasMember("stateOrRegion") && !address["stateOrRegion"].IsString() ) ||
                         ( address.HasMember("countryCode") && !address["countryCode"].IsString() ) ||
                         ( address.HasMember("districtOrCounty") && !address["districtOrCounty"].IsString() ) ||
                         ( address.HasMember("postalCode") && !address["postalCode"].IsString() )
                    ) {
                        Throw( "AddressNotString" );
                    }
                }
                if ( waypoint.HasMember("name") ) {
                    if ( waypoint[ "name" ].IsNull() || !waypoint[ "name" ].IsString() ) {
                        Throw( "waypointNameNotValid" );
                    }
                }

                ThrowIfNot( waypoint.HasMember("coordinate"), "waypointcoordinateMissing");
                auto coordinate = waypoint["coordinate"].GetArray();
                if ( coordinate[0].IsNull() ) {
                    Throw( "LatitudeNotValid" );
                }
                if ( coordinate[1].IsNull() ) {
                    Throw( "LongitudeNotValid" );
                }
                if ( waypoint.HasMember("pointOfInterest") ) {
                    auto poi = waypoint["pointOfInterest"].GetObject();
                    if ( !poi.HasMember("id") && !poi.HasMember("name") && !poi.HasMember("phoneNumber") ) {
                        waypoint.EraseMember("pointOfInterest");
                    }
                }
            }
        }
        ThrowIfNot( document.HasMember("shapes"), "shapesKeyMissing" );

        if( !document[ "shapes" ].IsArray() ) {
            Throw( "shapesArrayNotValid" );
        }
        if ( document[ "waypoints" ].Size() != 0 && document[ "shapes" ].Size() < 2 ) {
            AACE_WARN(LX(TAG).d("shapes","Shapes should not be less than 2 for local POI"));
        }

        if( document[ "shapes" ].Size() > MAXIMUM_SHAPES_IN_CONTEXT ) {
            AACE_WARN(LX(TAG, "isNavigationStateValid").d("shapes", "Too many shapes in payload. Only using first 100."));
            ThrowIfNot( document[ "shapes" ].Erase(document[ "shapes" ].Begin() + MAXIMUM_SHAPES_IN_CONTEXT, document[ "shapes" ].End()), "unable to operate on shapes Array" );
        }
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
        document.Accept( writer );
        navigationState = buffer.GetString();

        // set current navigation state payload
        m_navigationStatePayload = navigationState;
        return true;
    } catch( std::exception& ex ) {
        AACE_ERROR(LX(TAG).d("reason",ex.what() ));
        return false;
    }
}

std::unordered_set<std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityConfiguration>> NavigationCapabilityAgent::getCapabilityConfigurations() {
    return m_capabilityConfigurations;
}

} // aace::engine::navigation
} // aace::engine
} // aace


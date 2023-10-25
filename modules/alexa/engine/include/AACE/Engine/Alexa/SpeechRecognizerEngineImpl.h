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

#ifndef AACE_ENGINE_ALEXA_SPEECH_RECOGNIZER_ENGINE_IMPL_H
#define AACE_ENGINE_ALEXA_SPEECH_RECOGNIZER_ENGINE_IMPL_H

#include <memory>
#include <string>
#include <unordered_set>

#include <acsdk/MultiAgentInterface/AgentManagerInterface.h>
#include <acsdk/MultiAgentInterface/Connection/AgentConnectionObserverInterface.h>
#include <AIP/AudioInputProcessor.h>
#include <AIP/AudioProvider.h>
#include <AVSCommon/AVS/AgentInitiator.h>
#include <AVSCommon/AVS/CapabilityChangeNotifierInterface.h>
#include <AVSCommon/AVS/DialogUXStateAggregator.h>
#include <AVSCommon/SDKInterfaces/AudioInputProcessorObserverInterface.h>
#include <AVSCommon/SDKInterfaces/AuthObserverInterface.h>
#include <AVSCommon/SDKInterfaces/DialogUXStateObserverInterface.h>
#include <AVSCommon/SDKInterfaces/ConnectionStatusObserverInterface.h>
#include <AVSCommon/SDKInterfaces/DirectiveSequencerInterface.h>
#include <AVSCommon/SDKInterfaces/Endpoints/EndpointCapabilitiesRegistrarInterface.h>
#include <AVSCommon/SDKInterfaces/ExceptionEncounteredSenderInterface.h>
#include <AVSCommon/SDKInterfaces/FocusManagerInterface.h>
#include <AVSCommon/SDKInterfaces/KeyWordObserverInterface.h>
#include <AVSCommon/SDKInterfaces/MessageSenderInterface.h>
#include <AVSCommon/SDKInterfaces/ConnectionStatusObserverInterface.h>

#include <AVSCommon/SDKInterfaces/SystemSoundPlayerInterface.h>
#include <AVSCommon/Utils/Metrics/MetricRecorderInterface.h>
#include <AVSCommon/Utils/Threading/Executor.h>
#include <ACL/AVSConnectionManager.h>
#include <ContextManager/ContextManager.h>
#include <acsdkKWDImplementations/AbstractKeywordDetector.h>
#include <SpeechEncoder/SpeechEncoder.h>

#include <AACE/Alexa/SpeechRecognizer.h>
#include <AACE/Alexa/AlexaEngineInterfaces.h>
#include <AACE/Engine/Alexa/AssistantInfoManager.h>
#include <AACE/Engine/Arbitrator/ArbitratorObserverInterface.h>
#include <AACE/Engine/Arbitrator/ArbitratorServiceInterface.h>
#include <AACE/Engine/Audio/AudioManagerInterface.h>
#include <AACE/Engine/Core/EngineMacros.h>
#include "AACE/Engine/PropertyManager/PropertyManagerServiceInterface.h"
#include "AACE/Engine/Wakeword/WakewordManagerServiceInterface.h"
#include "AACE/Engine/Wakeword/WakewordManagerDelegateInterface.h"
#include <AACE/Alexa/AlexaClient.h>

#include "InitiatorVerifier.h"
#include "WakewordEngineAdapter.h"
#include "WakewordObserverInterface.h"

namespace aace {
namespace engine {
namespace alexa {

using Initiator = aace::alexa::SpeechRecognizerEngineInterface::Initiator;

class SpeechRecognizerEngineImpl
        : public aace::alexa::SpeechRecognizerEngineInterface
        , public alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface
        , public alexaClientSDK::avsCommon::sdkInterfaces::KeyWordObserverInterface
        , public alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface
        , public alexaClientSDK::multiAgentInterface::connection::AgentConnectionObserverInterface
        , public aace::engine::wakeword::WakewordManagerDelegateInterface
        , public aace::engine::arbitrator::ArbitratorObserverInterface
        , public alexaClientSDK::avsCommon::sdkInterfaces::DialogUXStateObserverInterface
        , public alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface
        , public alexaClientSDK::avsCommon::utils::RequiresShutdown
        , public std::enable_shared_from_this<SpeechRecognizerEngineImpl> {
private:
    SpeechRecognizerEngineImpl(
        std::shared_ptr<aace::alexa::SpeechRecognizer> speechRecognizerPlatformInterface,
        const alexaClientSDK::avsCommon::utils::AudioFormat& audioFormat);

    bool initialize(
        std::shared_ptr<aace::engine::audio::AudioManagerInterface> audioManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::endpoints::EndpointCapabilitiesRegistrarInterface>
            capabilitiesRegistrar,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::DirectiveSequencerInterface> directiveSequencer,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface> contextManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> audioFocusManager,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::DialogUXStateAggregator> dialogUXStateAggregator,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface> exceptionSender,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::UserInactivityMonitorInterface> userInactivityMonitor,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::LocaleAssetsManagerInterface>& assetsManager,
        class DeviceSettingsDelegate& deviceSettingsDelegate,
        std::shared_ptr<alexaClientSDK::acl::AVSConnectionManager> connectionManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SystemSoundPlayerInterface> systemSoundPlayer,
        std::shared_ptr<aace::engine::propertyManager::PropertyManagerServiceInterface> propertyManager,
        std::shared_ptr<alexaClientSDK::avsCommon::utils::metrics::MetricRecorderInterface> metricRecorder,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityChangeNotifierInterface> capabilityChangeNotifier,
        std::shared_ptr<alexaClientSDK::speechencoder::SpeechEncoder> speechEncoder,
        std::shared_ptr<aace::engine::alexa::WakewordEngineAdapter> wakewordEngineAdapter,
        std::shared_ptr<aace::engine::wakeword::WakewordManagerServiceInterface> wakewordService,
        const std::vector<std::shared_ptr<aace::engine::alexa::InitiatorVerifier>>& initiatorVerifier,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> visualFocusManager,
        std::shared_ptr<alexaClientSDK::multiAgentInterface::AgentManagerInterface> agentManager = nullptr,
        std::shared_ptr<aace::engine::arbitrator::ArbitratorServiceInterface> arbitratorService = nullptr
        );

public:
    static std::shared_ptr<SpeechRecognizerEngineImpl> create(
        std::shared_ptr<aace::alexa::SpeechRecognizer> speechRecognizerPlatformInterface,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::endpoints::EndpointCapabilitiesRegistrarInterface>
            capabilitiesRegistrar,
        const alexaClientSDK::avsCommon::utils::AudioFormat& audioFormat,
        std::shared_ptr<aace::engine::audio::AudioManagerInterface> audioManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::DirectiveSequencerInterface> directiveSequencer,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface> contextManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> focusManager,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::DialogUXStateAggregator> dialogUXStateAggregator,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface> exceptionSender,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::UserInactivityMonitorInterface> userInactivityMonitor,
        const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::LocaleAssetsManagerInterface>& assetsManager,
        class DeviceSettingsDelegate& deviceSettingsDelegate,
        std::shared_ptr<alexaClientSDK::acl::AVSConnectionManager> connectionManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SystemSoundPlayerInterface> systemSoundPlayer,
        std::shared_ptr<aace::engine::propertyManager::PropertyManagerServiceInterface> propertyManager,
        std::shared_ptr<alexaClientSDK::avsCommon::utils::metrics::MetricRecorderInterface> metricRecorder,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityChangeNotifierInterface> capabilityChangeNotifier,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> visualFocusManager,
        std::shared_ptr<alexaClientSDK::speechencoder::SpeechEncoder> speechEncoder = nullptr,
        std::shared_ptr<aace::engine::alexa::WakewordEngineAdapter> wakewordEngineAdapter = nullptr,
        std::shared_ptr<aace::engine::wakeword::WakewordManagerServiceInterface> wakewordService = nullptr,
        const std::vector<std::shared_ptr<aace::engine::alexa::InitiatorVerifier>>& initiatorVerifiers =
            std::vector<std::shared_ptr<aace::engine::alexa::InitiatorVerifier>>(),
        std::shared_ptr<alexaClientSDK::multiAgentInterface::AgentManagerInterface> agentManager = nullptr,
        std::shared_ptr<aace::engine::arbitrator::ArbitratorServiceInterface> arbitratorService = nullptr);

    /// @name @c aace::alexa::SpeechRecognizerEngineInterface functions
    /// @{
    bool onStartCapture(Initiator initiator, uint64_t keywordBegin, uint64_t keywordEnd, const std::string& keyword)
        override;
    bool onStopCapture() override;
    /// @}

    // keyword detection
    bool isWakewordEnabled();
    bool isWakewordSupported();
    bool enableWakewordDetection();
    bool disableWakewordDetection();

    /// @name @c aace::core::wakeword::WakewordManagerDelegateInterface functions
    /// @{
    bool enable3PWakewordDetection(const std::string& wakeword, bool state) override;
    bool is3PWakewordEnabled(const std::string& wakeword) override;
    /// @}

    /// @name @c alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface functions
    /// @{
    void onStateChanged(
        alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
        alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface::State state) override;
    /// @}

    /// @name @c alexaClientSDK::avsCommon::sdkInterfaces::KeyWordObserverInterface functions
    /// @{
    void onKeyWordDetected(
        std::shared_ptr<alexaClientSDK::avsCommon::avs::AudioInputStream> stream,
        std::string keyword,
        alexaClientSDK::avsCommon::avs::AudioInputStream::Index beginIndex =
            KeyWordObserverInterface::UNSPECIFIED_INDEX,
        alexaClientSDK::avsCommon::avs::AudioInputStream::Index endIndex = KeyWordObserverInterface::UNSPECIFIED_INDEX,
        std::shared_ptr<const std::vector<char>> KWDMetadata = nullptr) override;
    /// @}

    // Observers states for notifying the wakeword
    void addObserver(std::shared_ptr<WakewordObserverInterface> observer);
    void removeObserver(std::shared_ptr<WakewordObserverInterface> observer);

    /// @name @c alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface functions
    /// @{
    void onConnectionStatusChanged(
        const alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status status,
        const alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::ChangedReason reason)
        override;
    void onConnectionStatusChanged(
        const alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status status,
        const std::vector<
            alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::EngineConnectionStatus>&
            engineStatuses) override;
    /// @}

    /// @name @c alexaClientSDK::multiAgentInterface::connection::AgentConnectionObserverInterface functions
    /// @{
    void onAgentAvailabilityStateChanged(
        alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
        AvailabilityState status,
        const std::string& reason) override;
    /// @}

    /// @name @c aace::engine::arbitrator::ArbitratorObserverInterface functions
    /// @{
    void onDialogTerminated(const std::string& assistantId, const std::string& dialogId, const std::string& reason)
        override;
    void onAgentStateUpdated(
        const std::string& assistantId,
        const std::string& name,
        aace::arbitrator::ArbitratorEngineInterface::AgentState state) override;
    /// @}

    /// @name @c alexaClientSDK::avsCommon::sdkInterfaces::DialogUXStateObserverInterface functions
    /// @{
    void onDialogUXStateChanged(alexaClientSDK::avsCommon::avs::AgentId::IdType agentId, DialogUXState newState)
        override;
    /// @}

    /// @name @c alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface functions
    /// @{
    void onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State state,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error error) override;
    /// @}

protected:
    virtual void doShutdown() override;

private:
    bool startCapture(
        std::shared_ptr<alexaClientSDK::capabilityAgents::aip::AudioProvider> audioProvider,
        alexaClientSDK::avsCommon::avs::AgentInitiator initiator,
        alexaClientSDK::avsCommon::avs::AudioInputStream::Index begin =
            alexaClientSDK::capabilityAgents::aip::AudioInputProcessor::INVALID_INDEX,
        alexaClientSDK::avsCommon::avs::AudioInputStream::Index keywordEnd =
            alexaClientSDK::capabilityAgents::aip::AudioInputProcessor::INVALID_INDEX,
        const std::string& keyword = "");

    bool initializeAudioInputStream();

    /**
     * Blocks up to the specified @a duration until the @ SpeechRecognizerEngineImpl expecting audio state
     * transitions to the caller's expected state.
     * Do not call this function on a thread holding @c m_expectingAudioMutex.
     *
     * @param expectingAudio Whether to expect audio. 
     *        @c true to wait for @ SpeechRecognizerEngineImpl to be expecting audio from the open audio input channel,
     *        else @c false to wait for @ SpeechRecognizerEngineImpl to stop expecting audio.
     * @param duration The duration to wait in seconds.
     * @return @c true if @a expectingAudio state was reached within @a duration, else @c false
     */
    bool waitForExpectingAudioState(bool expectingAudio, const std::chrono::seconds duration = std::chrono::seconds(3));

    /**
     * Returns whether the SpeechRecognizerEngineImpl is currently expecting audio to be delivered from an input
     * channel via a call to @c write. Do not call this function on a thread holding @c m_expectingAudioMutex.
     */
    bool isExpectingAudio();
    /**
     * Returns whether the SpeechRecognizerEngineImpl is currently expecting audio to be delivered from an input
     * channel via a call to @c write. Only call this function on a thread holding @c m_expectingAudioMutex.
     */
    bool isExpectingAudioLocked();

    /**
     * Gets the current audio input channel ID.
     * Do not call this function on a thread holding @c m_expectingAudioMutex.
     * 
     * @return @c m_currentChannelId
     */
    aace::engine::audio::AudioInputChannelInterface::ChannelId getCurrentChannelId();

    bool startAudioInput();
    bool stopAudioInput();
    ssize_t write(const int16_t* data, const size_t size);

    bool m_wakeWordAdapterEnabled = false;
    bool enable3PWakewordAdapter();
    bool disable3PWakewordAdapter();
    bool is3PAssistantActive(const std::string& name);
    bool is3PWakewordConfigured(const std::string& name);
    /**
     * @brief Mutex to protect m_enabledWakewordSet
     * 
     */
    std::mutex m_wakewordEnableMutex;

    /**
     * @brief Mutex to protect WakewordAdapter
     * 
     */
    std::mutex m_wakewordAdapterMutex;

    std::shared_ptr<aace::engine::wakeword::WakewordManagerServiceInterface> m_wakewordService;
    std::unordered_set<std::string> m_enabledWakewordSet;
    std::unordered_set<std::string> m_wakewordConfig;

    std::shared_ptr<aace::engine::arbitrator::ArbitratorServiceInterface> m_arbitratorService;
    bool m_isAgentRegistered = false;
    std::mutex m_registeredAgentDialogIdMutex;
    std::string m_registeredAgentDialogId = "";

    std::map<std::string, bool> getAlexaAgentDialogStateRules();

private:
    std::shared_ptr<aace::alexa::SpeechRecognizer> m_speechRecognizerPlatformInterface;
    std::shared_ptr<alexaClientSDK::capabilityAgents::aip::AudioInputProcessor> m_audioInputProcessor;

    alexaClientSDK::avsCommon::utils::AudioFormat m_audioFormat;
    std::shared_ptr<alexaClientSDK::avsCommon::avs::AudioInputStream> m_audioInputStream;
    std::unique_ptr<alexaClientSDK::avsCommon::avs::AudioInputStream::Writer> m_audioInputWriter;

    std::shared_ptr<aace::engine::audio::AudioInputChannelInterface> m_audioInputChannel;
    /**
     * The current audio input channel ID. Access is serialized by @c m_expectingAudioMutex.
     */
    aace::engine::audio::AudioInputChannelInterface::ChannelId m_currentChannelId =
        aace::engine::audio::AudioInputChannelInterface::INVALID_CHANNEL;
    /**
     * Mutex to serialize access to the expecting audio condition, i.e. @c m_currentChannelId and any functions changing
     * the condition.
     */
    std::mutex m_expectingAudioMutex;
    std::condition_variable m_expectingAudioState_cv;

    unsigned int m_wordSize;

    std::shared_ptr<aace::engine::alexa::WakewordEngineAdapter> m_wakewordEngineAdapter;
    bool m_wakewordEnabled = false;
    bool m_initialWakewordEnabledState = true;

    std::vector<std::shared_ptr<aace::engine::alexa::InitiatorVerifier>> m_initiatorVerifiers;
    alexaClientSDK::avsCommon::utils::threading::Executor m_executor;

    // the aip state
    AudioInputProcessorObserverInterface::State m_state;

    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::DirectiveSequencerInterface> m_directiveSequencer;

    std::shared_ptr<alexaClientSDK::avsCommon::avs::DialogUXStateAggregator> m_dialogUXStateAggregator;

    /**
     * Observers to notify of wake word detection.
     */
    std::unordered_set<std::shared_ptr<WakewordObserverInterface>> m_observers;
    /**
     * Mutex to serialize access to @c m_observers.
     */
    std::mutex m_observerMutex;

    aace::alexa::AlexaClient::ConnectionStatus m_connectionStatus;

    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> m_audioFocusManager;

    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> m_visualFocusManager;

    /**
     * Mutex to serialize access to @c m_availableAgents.
     */
    std::mutex m_agentAvailabilityMutex;
    std::unordered_set<alexaClientSDK::avsCommon::avs::AgentId::IdType> m_availableAgents;
    std::shared_ptr<alexaClientSDK::multiAgentInterface::AgentManagerInterface> m_agentManager;
};

static inline alexaClientSDK::avsCommon::avs::AgentInitiator convertInitiator(Initiator initiator) {
    switch (initiator) {
        case Initiator::HOLD_TO_TALK:
            return alexaClientSDK::avsCommon::avs::AgentInitiator::PRESS_AND_HOLD;
            break;
        case Initiator::TAP_TO_TALK:
            return alexaClientSDK::avsCommon::avs::AgentInitiator::TAP;
            break;
        case Initiator::WAKEWORD:
            return alexaClientSDK::avsCommon::avs::AgentInitiator::WAKEWORD;
            break;
        default:
            throw("Unknown initiator.");
    }
}

static inline std::string convertDialogStateToString(
    alexaClientSDK::avsCommon::sdkInterfaces::DialogUXStateObserverInterface::DialogUXState dialogState) {
    using DialogUXState = alexaClientSDK::avsCommon::sdkInterfaces::DialogUXStateObserverInterface::DialogUXState;
    switch (dialogState) {
        case DialogUXState::IDLE:
            return "IDLE";
            break;
        case DialogUXState::LISTENING:
            return "LISTENING";
            break;
        case DialogUXState::EXPECTING:
            return "EXPECTING";
            break;
        case DialogUXState::THINKING:
            return "THINKING";
            break;
        case DialogUXState::SPEAKING:
            return "SPEAKING";
            break;
        case DialogUXState::FINISHED:
            return "FINISHED";
            break;
        default:
            throw("Unknown DialogState.");
    }
}

}  // namespace alexa
}  // namespace engine
}  // namespace aace

#endif  // AACE_ENGINE_ALEXA_SPEECH_RECOGNIZER_ENGINE_IMPL_H

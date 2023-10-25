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

#include <climits>
#include <exception>

#include "AACE/Alexa/AlexaProperties.h"
#include "AACE/Engine/Alexa/DeviceSettingsDelegate.h"
#include "AACE/Engine/Alexa/SpeechRecognizerEngineImpl.h"
#include "AACE/Engine/Alexa/WakewordObservableInterface.h"
#include "AACE/Engine/Alexa/WakewordObserverInterface.h"
#include "AACE/Engine/Core/EngineMacros.h"
#include "AACE/Engine/Utils/Agent/AgentId.h"
#include "AACE/Engine/Utils/String/StringUtils.h"

namespace aace {
namespace engine {
namespace alexa {

using namespace aace::engine::utils::string;

/// The maximum number of readers of the stream.
static const size_t MAX_READERS = 10;

/// The amount of audio data to keep in the ring buffer.
static const std::chrono::seconds AMOUNT_OF_AUDIO_DATA_IN_BUFFER = std::chrono::seconds(15);

/// The amount of time for wake-word verification
static const std::chrono::milliseconds VERIFICATION_TIMEOUT = std::chrono::milliseconds(500);

// String to identify log entries originating from this file.
static const std::string TAG("aace.alexa.SpeechRecognizerEngineImpl");

static const std::string ASSISTANT_3P_STATE_ACTIVE = "ACTIVE";

/// For metric calculation we want to detect an invalid wakeword duration (20 seconds max based on 16Khz voice rate)
static const ssize_t MAX_WAKEWORD_SAMPLE_DURATION = 320000;

// Evaluates the correct startOfSpeechTimestamp by subtracting how long ago the wakeword started from steady clock now.
static void computeStartOfSpeechTimestamp(
    const alexaClientSDK::avsCommon::avs::AudioInputStream::Index wakewordBeginIndex,
    const alexaClientSDK::avsCommon::avs::AudioInputStream::Index wakewordEndIndex,
    const unsigned int sampleRateHz,
    const std::shared_ptr<alexaClientSDK::avsCommon::avs::AudioInputStream>& stream,
    std::chrono::steady_clock::time_point& startOfSpeechTimestamp) {
    // Create a reader to get the current index position
    static const auto startWithNewData = true;
    if (not stream) {
        AACE_WARN(LX(TAG).d(
            "computeStartOfSpeechTimestamp",
            "Streams could not be obtained, skipping startOfSpeech timestamp calculation"));
        return;
    }

    auto reader = stream->createReader(
        alexaClientSDK::avsCommon::avs::AudioInputStream::Reader::Policy::NONBLOCKING, startWithNewData);
    if (not reader) {
        AACE_WARN(LX(TAG).d(
            "computeStartOfSpeechTimestamp",
            "Stream reader could not be obtained, skipping startOfSpeech timestamp calculation"));
        return;
    }

    // Get the current index position
    ssize_t currentIndex = reader->tell();
    reader->close();

    /* This block will be entered if bytes have not yet started/resumed streaming to the audioInput stream. This is
     * normal when using an implementation using an external wakeword engine. For instance, the currentIndex starts
     * out at 0 upon engine start for external wakeword until after startCapture returns, which triggers
     * startAudioInput. In the external wakeword setup, we are entering this method at a point in time where the
     * wakeword has just been detected and audio samples are not yet ingested. Therefore, in this case, we need to
     * use wakeword end index as the basis for the startOfSpeech timestamp calculation.
     */
    if (currentIndex <= (ssize_t)wakewordBeginIndex) {
        // Set current index depending on whether the indexes supplied are valid
        if (wakewordEndIndex > wakewordBeginIndex &&
            (wakewordEndIndex - wakewordBeginIndex) < MAX_WAKEWORD_SAMPLE_DURATION) {
            currentIndex = wakewordEndIndex;
        } else {
            AACE_WARN(LX(TAG).d(
                "computeStartOfSpeechTimestamp",
                "Wakeword indexes appear to be invalid, defaulting to wakewordBeginIndex for startOfSpeech timestamp "
                "calculation"));
            currentIndex = wakewordBeginIndex;
        }
    }

    // Start with the current time in the startOfSpeechTimestamp corresponding with the currentIndex from the reader.
    startOfSpeechTimestamp = std::chrono::steady_clock::now();

    // Translate the currentIndex position to a time duration elapsed since the end of wakeword.
    const auto sampleRatePerMillisec = sampleRateHz / 1000;

    const auto timeSinceStartOfWW =
        std::chrono::milliseconds((currentIndex - wakewordBeginIndex) / sampleRatePerMillisec);

    // Adjust the start of speech timestamp to be the start of the WW.
    startOfSpeechTimestamp -= timeSinceStartOfWW;
    AACE_DEBUG(LX(TAG)
                   .d("timeSinceStartOfWW", timeSinceStartOfWW.count())
                   .d("startOfSpeechTimestampWithoutPreRollMS",
                      std::chrono::duration_cast<std::chrono::milliseconds>(startOfSpeechTimestamp.time_since_epoch())
                          .count()));
}

SpeechRecognizerEngineImpl::SpeechRecognizerEngineImpl(
    std::shared_ptr<aace::alexa::SpeechRecognizer> speechRecognizerPlatformInterface,
    const alexaClientSDK::avsCommon::utils::AudioFormat& audioFormat) :
        alexaClientSDK::avsCommon::utils::RequiresShutdown(TAG),
        m_speechRecognizerPlatformInterface(speechRecognizerPlatformInterface),
        m_audioFormat(audioFormat),
        m_wordSize(audioFormat.sampleSizeInBits / CHAR_BIT),
        m_state(alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface::State::IDLE) {
}

bool SpeechRecognizerEngineImpl::initialize(
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
    DeviceSettingsDelegate& deviceSettingsDelegate,
    std::shared_ptr<alexaClientSDK::acl::AVSConnectionManager> connectionManager,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SystemSoundPlayerInterface> systemSoundPlayer,
    std::shared_ptr<aace::engine::propertyManager::PropertyManagerServiceInterface> propertyManager,
    std::shared_ptr<alexaClientSDK::avsCommon::utils::metrics::MetricRecorderInterface> metricRecorder,
    std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityChangeNotifierInterface> capabilityChangeNotifier,
    std::shared_ptr<alexaClientSDK::speechencoder::SpeechEncoder> speechEncoder,
    std::shared_ptr<aace::engine::alexa::WakewordEngineAdapter> wakewordEngineAdapter,
    std::shared_ptr<aace::engine::wakeword::WakewordManagerServiceInterface> wakewordService,
    const std::vector<std::shared_ptr<aace::engine::alexa::InitiatorVerifier>>& initiatorVerifiers,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> visualFocusManager,
    std::shared_ptr<alexaClientSDK::multiAgentInterface::AgentManagerInterface> agentManager,
    std::shared_ptr<aace::engine::arbitrator::ArbitratorServiceInterface> arbitratorService) {
    try {
        // create the audio channel
        m_audioInputChannel = audioManager->openAudioInputChannel(
            "SpeechRecognizer", aace::engine::audio::AudioManagerInterface::AudioInputType::VOICE);
        ThrowIfNull(m_audioInputChannel, "invalidAudioInputChannel");

        // create the wakeword confirmation setting
        ThrowIfNot(
            deviceSettingsDelegate.configureWakeWordConfirmationSetting(), "createWakeWordConfirmationSettingFailed");

        // create the speech confirmation setting
        ThrowIfNot(
            deviceSettingsDelegate.configureSpeechConfirmationSetting(), "createSpeechConfirmationSettingFailed");

        std::shared_ptr<alexaClientSDK::settings::WakeWordsSetting> localAndWakeWordsSetting = nullptr;

        if (wakewordEngineAdapter != nullptr && assetsManager->getDefaultSupportedWakeWords().empty() == false) {
            ThrowIfNot(
                deviceSettingsDelegate.configureLocaleAndWakeWordsSettings(assetsManager),
                "createLocaleAndWakeWordsSettingFailed");
        } else {
            ThrowIfNot(deviceSettingsDelegate.configureLocaleSetting(assetsManager), "createLocaleSettingFailed");
        }

        m_audioFocusManager = audioFocusManager;
        m_visualFocusManager = visualFocusManager;

        m_agentManager = agentManager;
        if (m_agentManager != nullptr) {
            m_agentManager->addAgentConnectionObserverInterface(
                alexaClientSDK::avsCommon::avs::AgentId::AGENT_ID_ALL, shared_from_this());
        }

        m_audioInputProcessor = alexaClientSDK::capabilityAgents::aip::AudioInputProcessor::create(
            directiveSequencer,
            messageSender,
            contextManager,
            audioFocusManager,
            dialogUXStateAggregator,
            exceptionSender,
            userInactivityMonitor,
            systemSoundPlayer,
            assetsManager,
            deviceSettingsDelegate.getWakeWordConfirmationSetting(),
            deviceSettingsDelegate.getSpeechConfirmationSetting(),
            capabilityChangeNotifier,
            deviceSettingsDelegate.getWakeWordsSetting(),
            speechEncoder,
            alexaClientSDK::capabilityAgents::aip::AudioProvider::null(),
            nullptr,
            metricRecorder,
            nullptr,
            agentManager);

        ThrowIfNull(m_audioInputProcessor, "couldNotCreateAudioInputProcessor");
        ThrowIfNot(initializeAudioInputStream(), "initializeAudioInputStreamFailed");

        // add dialog state observer to aip
        m_audioInputProcessor->addObserver(shared_from_this());
        m_audioInputProcessor->addObserver(dialogUXStateAggregator);
        m_dialogUXStateAggregator = dialogUXStateAggregator;

        // register capability with the default endpoint
        capabilitiesRegistrar->withCapability(m_audioInputProcessor, m_audioInputProcessor);

        m_wakewordEngineAdapter = wakewordEngineAdapter;
        if (m_wakewordEngineAdapter != nullptr) {
            auto defaultLocale = propertyManager->getProperty(aace::alexa::property::LOCALE);
            ThrowIfNot(
                m_wakewordEngineAdapter->initialize(defaultLocale, m_audioInputStream, m_audioFormat),
                "wakewordInitializeFailed");
            m_wakewordEngineAdapter->addKeyWordObserver(shared_from_this());
        }

        m_wakewordService = wakewordService;
        m_directiveSequencer = directiveSequencer;
        m_initiatorVerifiers = initiatorVerifiers;

        //Get configured Wakewordlist
        m_wakewordConfig = m_wakewordService->getConfigured3PWakewords();

        // get the initialize wakeword enabled state from the platform interface
        m_wakewordEnabled = isWakewordSupported() && m_initialWakewordEnabledState;

        // add as arbitrator observer
        m_arbitratorService = arbitratorService;
        m_arbitratorService->addObserver(shared_from_this());

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "initialize").d("reason", ex.what()));
        return false;
    }
}

std::shared_ptr<SpeechRecognizerEngineImpl> SpeechRecognizerEngineImpl::create(
    std::shared_ptr<aace::alexa::SpeechRecognizer> speechRecognizerPlatformInterface,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::endpoints::EndpointCapabilitiesRegistrarInterface>
        capabilitiesRegistrar,
    const alexaClientSDK::avsCommon::utils::AudioFormat& audioFormat,
    std::shared_ptr<aace::engine::audio::AudioManagerInterface> audioManager,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::DirectiveSequencerInterface> directiveSequencer,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface> contextManager,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> audioFocusManager,
    std::shared_ptr<alexaClientSDK::avsCommon::avs::DialogUXStateAggregator> dialogUXStateAggregator,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface> exceptionSender,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::UserInactivityMonitorInterface> userInactivityMonitor,
    const std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::LocaleAssetsManagerInterface>& assetsManager,
    DeviceSettingsDelegate& deviceSettingsDelegate,
    std::shared_ptr<alexaClientSDK::acl::AVSConnectionManager> connectionManager,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SystemSoundPlayerInterface> systemSoundPlayer,
    std::shared_ptr<aace::engine::propertyManager::PropertyManagerServiceInterface> propertyManager,
    std::shared_ptr<alexaClientSDK::avsCommon::utils::metrics::MetricRecorderInterface> metricRecorder,
    std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityChangeNotifierInterface> capabilityChangeNotifier,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> visualFocusManager,
    std::shared_ptr<alexaClientSDK::speechencoder::SpeechEncoder> speechEncoder,
    std::shared_ptr<aace::engine::alexa::WakewordEngineAdapter> wakewordEngineAdapter,
    std::shared_ptr<aace::engine::wakeword::WakewordManagerServiceInterface> wakewordService,
    const std::vector<std::shared_ptr<aace::engine::alexa::InitiatorVerifier>>& initiatorVerifiers,
    std::shared_ptr<alexaClientSDK::multiAgentInterface::AgentManagerInterface> agentManager,
    std::shared_ptr<aace::engine::arbitrator::ArbitratorServiceInterface> arbitratorService) {
    std::shared_ptr<SpeechRecognizerEngineImpl> speechRecognizerEngineImpl = nullptr;

    try {
        ThrowIfNull(speechRecognizerPlatformInterface, "invlaidSpeechRecognizerPlatformInterface");
        ThrowIfNull(audioManager, "invalidAudioManager");
        ThrowIfNull(capabilitiesRegistrar, "invalidCapabilitiesRegistrar");
        ThrowIfNull(directiveSequencer, "invalidDirectiveSequencer");
        ThrowIfNull(messageSender, "invalidMessageSender");
        ThrowIfNull(audioFocusManager, "invalidAudioFocusManager");
        ThrowIfNull(contextManager, "invalidContextManager");
        ThrowIfNull(dialogUXStateAggregator, "invalidDialogUXStateAggregator");
        ThrowIfNull(exceptionSender, "invalidExceptionSender");
        ThrowIfNull(userInactivityMonitor, "invalidUserInactivityMonitor");
        ThrowIfNull(connectionManager, "invalidConnectionManager");
        ThrowIfNull(systemSoundPlayer, "invalidSystemSoundPlayer");
        ThrowIfNull(metricRecorder, "invalidMetricsRecorder");
        ThrowIfNull(capabilityChangeNotifier, "invalidCapabilityChangeNotifier");
        ThrowIfNull(wakewordService, "invalidwakewordService");
        ThrowIfNull(arbitratorService, "invalidArbitratorService");
        ThrowIfNull(visualFocusManager, "invalidVisualFocusManager");

        speechRecognizerEngineImpl = std::shared_ptr<SpeechRecognizerEngineImpl>(
            new SpeechRecognizerEngineImpl(speechRecognizerPlatformInterface, audioFormat));

        ThrowIfNot(
            speechRecognizerEngineImpl->initialize(
                audioManager,
                capabilitiesRegistrar,
                directiveSequencer,
                messageSender,
                contextManager,
                audioFocusManager,
                dialogUXStateAggregator,
                exceptionSender,
                userInactivityMonitor,
                assetsManager,
                deviceSettingsDelegate,
                connectionManager,
                systemSoundPlayer,
                propertyManager,
                metricRecorder,
                capabilityChangeNotifier,
                speechEncoder,
                wakewordEngineAdapter,
                wakewordService,
                initiatorVerifiers,
                visualFocusManager,
                agentManager,
                arbitratorService),
            "initializeSpeechRecognizerEngineImplFailed");

        // set the platform engine interface reference
        speechRecognizerPlatformInterface->setEngineInterface(speechRecognizerEngineImpl);

        return speechRecognizerEngineImpl;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "create").d("reason", ex.what()));
        if (speechRecognizerEngineImpl != nullptr) {
            speechRecognizerEngineImpl->shutdown();
        }
        return nullptr;
    }
}

void SpeechRecognizerEngineImpl::doShutdown() {
    m_executor.shutdown();

    if (m_audioInputWriter != nullptr) {
        m_audioInputWriter->close();
    }

    if (m_audioInputProcessor != nullptr) {
        m_audioInputProcessor->removeObserver(shared_from_this());
        m_audioInputProcessor->removeObserver(m_dialogUXStateAggregator);
        m_audioInputProcessor->shutdown();
    }

    if (m_wakewordEngineAdapter != nullptr) {
        m_wakewordEngineAdapter->removeKeyWordObserver(shared_from_this());
    }

    if (m_speechRecognizerPlatformInterface != nullptr) {
        m_speechRecognizerPlatformInterface->setEngineInterface(nullptr);
    }

    if (m_audioInputChannel != nullptr) {
        m_audioInputChannel->doShutdown();
        m_audioInputChannel.reset();
    }

    if (m_agentManager != nullptr) {
        m_agentManager->removeAgentConnectionObserverInterface(
            alexaClientSDK::avsCommon::avs::AgentId::AGENT_ID_ALL, shared_from_this());
        m_agentManager.reset();
    }

    if (m_arbitratorService != nullptr) {
        m_arbitratorService->removeObserver(shared_from_this());
    }

    m_initiatorVerifiers.clear();
}

bool SpeechRecognizerEngineImpl::initializeAudioInputStream() {
    try {
        size_t size = alexaClientSDK::avsCommon::avs::AudioInputStream::calculateBufferSize(
            m_audioFormat.sampleRateHz * AMOUNT_OF_AUDIO_DATA_IN_BUFFER.count(), m_wordSize, MAX_READERS);
        auto buffer = std::make_shared<alexaClientSDK::avsCommon::avs::AudioInputStream::Buffer>(size);
        ThrowIfNull(buffer, "couldNotCreateAudioInputBuffer");

        // create the audio input stream
        m_audioInputStream = alexaClientSDK::avsCommon::avs::AudioInputStream::create(buffer, m_wordSize, MAX_READERS);
        ThrowIfNull(m_audioInputStream, "couldNotCreateAudioInputStream");

        // create the audio input writer
        m_audioInputWriter = m_audioInputStream->createWriter(
            alexaClientSDK::avsCommon::avs::AudioInputStream::Writer::Policy::NONBLOCKABLE);
        ThrowIfNull(m_audioInputWriter, "couldNotCreateAudioInputWriter");

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "initializeAudioInputStream").d("reason", ex.what()));
        m_audioInputStream.reset();
        m_audioInputWriter.reset();
        return false;
    }
}

bool SpeechRecognizerEngineImpl::waitForExpectingAudioState(bool expectingAudio, const std::chrono::seconds duration) {
    std::unique_lock<std::mutex> lock(m_expectingAudioMutex);

    return m_expectingAudioState_cv.wait_for(
        lock, duration, [this, expectingAudio]() { return expectingAudio == isExpectingAudioLocked(); });
}

bool SpeechRecognizerEngineImpl::startAudioInput() {
    AACE_VERBOSE(LX(TAG));
    std::unique_lock<std::mutex> lock(m_expectingAudioMutex);
    try {
        // if we are already expecting audio then don't attempt to start the audio
        if (isExpectingAudioLocked()) {
            AACE_WARN(LX(TAG, "startAudioInput").d("reason", "alreadyExpectingAudio"));
            return true;
        }

        // notify the platform that we are expecting audio... if the platform returns
        // and error then we reset the expecting audio state and throw an exception
        std::weak_ptr<SpeechRecognizerEngineImpl> wp = shared_from_this();

        m_currentChannelId = m_audioInputChannel->start([wp](const int16_t* data, const size_t size) {
            if (auto sp = wp.lock()) {
                sp->write(data, size);
            } else {
                AACE_ERROR(LX(TAG, "startAudioInput").d("reason", "invalidWeakPtrReference"));
            }
        });

        // throw an exception if we failed to start the audio input channel
        ThrowIf(
            m_currentChannelId == aace::engine::audio::AudioInputChannelInterface::INVALID_CHANNEL,
            "audioInputChannelStartFailed");

        // notify mutex that the expecting audio state has changed
        m_expectingAudioState_cv.notify_all();

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "startAudioInput").d("reason", ex.what()).d("id", m_currentChannelId));
        return false;
    }
}

bool SpeechRecognizerEngineImpl::stopAudioInput() {
    AACE_VERBOSE(LX(TAG));
    std::unique_lock<std::mutex> lock(m_expectingAudioMutex);
    try {
        ThrowIf(
            m_currentChannelId == aace::engine::audio::AudioInputChannelInterface::INVALID_CHANNEL,
            "invalidAudioChannelId");
        m_audioInputChannel->stop(m_currentChannelId);

        // reset the channel id
        m_currentChannelId = aace::engine::audio::AudioInputChannelInterface::INVALID_CHANNEL;

        // notify expecting audio wait condition that the state has changed
        m_expectingAudioState_cv.notify_all();

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "stopAudioInput").d("reason", ex.what()).d("id", m_currentChannelId));
        return false;
    }
}

bool SpeechRecognizerEngineImpl::isExpectingAudio() {
    std::unique_lock<std::mutex> lock(m_expectingAudioMutex);
    return isExpectingAudioLocked();
}

bool SpeechRecognizerEngineImpl::isExpectingAudioLocked() {
    return m_currentChannelId != aace::engine::audio::AudioInputChannelInterface::INVALID_CHANNEL;
}

aace::engine::audio::AudioInputChannelInterface::ChannelId SpeechRecognizerEngineImpl::getCurrentChannelId() {
    std::unique_lock<std::mutex> lock(m_expectingAudioMutex);
    return m_currentChannelId;
}

// SpeechRecognizer
bool SpeechRecognizerEngineImpl::onStartCapture(
    Initiator initiator,
    uint64_t keywordBegin,
    uint64_t keywordEnd,
    const std::string& keyword) {
    AACE_INFO(LX(TAG).d("initiator", initiator));

    std::lock_guard<std::mutex> lock(m_agentAvailabilityMutex);
    if (m_connectionStatus != aace::alexa::AlexaClient::ConnectionStatus::CONNECTED && m_availableAgents.empty()) {
        AACE_WARN(LX(TAG)
                      .d("reason", "No agent is connected")
                      .d("connectionStatus", m_connectionStatus)
                      .d("numAvailalbeAgents", m_availableAgents.size()));
        return false;
    }

    // request dialog for registered agent from arbitrator if we are not already the active assistant
    if (m_isAgentRegistered && m_registeredAgentDialogId.empty()) {
        std::string mode = (initiator == Initiator::WAKEWORD) ? "WAKEWORD" : "GESTURE";
        auto assistantId = std::to_string(aace::engine::utils::agent::AGENT_ID_ALEXA);

        std::string dialogId;
        std::string reason;
        AACE_DEBUG(LX(TAG).m("Start dialog for").d("assistantId", assistantId));
        bool dialogStarted = m_arbitratorService->startDialog(assistantId, mode, dialogId, reason);
        if (!dialogStarted) {
            AACE_WARN(LX(TAG).d("Dialog denied by Arbitrator", reason));
            return false;
        }
        AACE_INFO(LX(TAG).m("Dialog started").d("dialogId", dialogId).d("assistantId", assistantId));
        // set dialog Id
        std::lock_guard<std::mutex> lock(m_registeredAgentDialogIdMutex);
        m_registeredAgentDialogId = dialogId;
    }

    try {
        ThrowIf(
            m_state ==
                alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface::State::RECOGNIZING,
            "alreadyRecognizing");

        // create a new audio provider for the specified initiator type
        std::shared_ptr<alexaClientSDK::capabilityAgents::aip::AudioProvider> audioProvider;

        if (initiator != Initiator::WAKEWORD) {
            for (const auto& initiatorVerifier : m_initiatorVerifiers) {
                if (initiatorVerifier && initiatorVerifier->shouldBlock(convertInitiator(initiator))) {
                    AACE_WARN(LX(TAG, "onStartCapture: Cancelled by Initiator Verifier for PTT"));
                    return false;
                }
            }
        }

        switch (initiator) {
            case Initiator::HOLD_TO_TALK:
                audioProvider = std::make_shared<alexaClientSDK::capabilityAgents::aip::AudioProvider>(
                    m_audioInputStream,
                    m_audioFormat,
                    alexaClientSDK::capabilityAgents::aip::ASRProfile::CLOSE_TALK,
                    false,
                    true,
                    false);
                break;

            case Initiator::TAP_TO_TALK:
                audioProvider = std::make_shared<alexaClientSDK::capabilityAgents::aip::AudioProvider>(
                    m_audioInputStream,
                    m_audioFormat,
                    alexaClientSDK::capabilityAgents::aip::ASRProfile::NEAR_FIELD,
                    true,
                    true,
                    true);
                break;

            case Initiator::WAKEWORD:
                ThrowIf(keywordBegin == SpeechRecognizerEngineInterface::UNSPECIFIED_INDEX, "invalidKeywordBeginIndex");
                ThrowIf(keywordEnd == SpeechRecognizerEngineInterface::UNSPECIFIED_INDEX, "invalidKeywordEndIndex");
                ThrowIf(keyword.empty(), "invalidKeyword");
                audioProvider = std::make_shared<alexaClientSDK::capabilityAgents::aip::AudioProvider>(
                    m_audioInputStream,
                    m_audioFormat,
                    alexaClientSDK::capabilityAgents::aip::ASRProfile::NEAR_FIELD,
                    true,
                    false,
                    true);
                break;
        }

        // start the recognize event
        ThrowIfNot(
            startCapture(audioProvider, convertInitiator(initiator), keywordBegin, keywordEnd, keyword),
            "startCaptureFailed");

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "onStartCapture")
                       .d("reason", ex.what())
                       .d("initiator", initiator)
                       .d("state", m_state)
                       .d("id", getCurrentChannelId()));
        return false;
    }
}

bool SpeechRecognizerEngineImpl::onStopCapture() {
    AACE_INFO(LX(TAG));
    try {
        ThrowIfNot(m_audioInputProcessor->stopCapture().get(), "stopCaptureFailed");
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()).d("id", getCurrentChannelId()));
        return false;
    }
}

ssize_t SpeechRecognizerEngineImpl::write(const int16_t* data, const size_t size) {
    try {
        ThrowIfNot(waitForExpectingAudioState(true), "audioNotExpected");
        ThrowIfNull(m_audioInputWriter, "nullAudioInputWriter");
        ssize_t result = m_audioInputWriter->write(data, size);
        ThrowIf(result < 0, "errorWritingData");
        return result;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()).d("id", getCurrentChannelId()));
        return -1;
    }
}

void SpeechRecognizerEngineImpl::addObserver(std::shared_ptr<WakewordObserverInterface> observer) {
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_observers.insert(observer);
}

void SpeechRecognizerEngineImpl::removeObserver(std::shared_ptr<WakewordObserverInterface> observer) {
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_observers.erase(observer);
}

void SpeechRecognizerEngineImpl::onKeyWordDetected(
    std::shared_ptr<alexaClientSDK::avsCommon::avs::AudioInputStream> stream,
    std::string keyword,
    alexaClientSDK::avsCommon::avs::AudioInputStream::Index beginIndex,
    alexaClientSDK::avsCommon::avs::AudioInputStream::Index endIndex,
    std::shared_ptr<const std::vector<char>> KWDMetadata) {
    //Verify if this is external wakeword and it is enabled and call platform interface
    if (is3PWakewordEnabled(keyword)) {
        AACE_INFO(LX(TAG).d("Active 3P Wakeword is detected", keyword));
        m_wakewordService->updateOnWakewordDetected(keyword, beginIndex, endIndex);
        return;
    }
    //Check if confgured 3P wakeword detected and return
    if (is3PWakewordConfigured(keyword)) {
        AACE_INFO(LX(TAG).d(" Disabled 3P Wakeword detected ", keyword));
        return;
    }

    //if alexawakeword is disabled then return (assumption is that m_wakewordEnabled
    //applies to any AVS coformant agent wakeword here, if its name specific
    //shoudBlock should be used. But enabling through alexaproperty seems to
    //apply to any AVS alexa wakeword
    if (m_wakewordEnabled == false) {
        AACE_INFO(LX(TAG).d(" Disabled AVS wakeword detected ", keyword));
        return;
    }
    if (m_state == AudioInputProcessorObserverInterface::State::IDLE) {
        m_executor.submit([this, beginIndex, endIndex, keyword] {
            for (const auto& initiatorVerifier : m_initiatorVerifiers) {
                if (initiatorVerifier && initiatorVerifier->shouldBlock(keyword, VERIFICATION_TIMEOUT)) {
                    AACE_WARN(LX(TAG, "onKeyWordDetected: Cancelled by Initiator Verifier for wakeword"));
                    return;
                }
            }

            // Only notifies platform interface after passing wakeword verifiers,
            // otherwise earcon might still play even if keyword is dropped
            m_speechRecognizerPlatformInterface->wakewordDetected(keyword);

            // notify observers about the wakeword detected
            std::lock_guard<std::mutex> lock(m_observerMutex);

            for (const auto& next : m_observers) {
                next->wakewordDetected(keyword);
            }

            onStartCapture(Initiator::WAKEWORD, beginIndex, endIndex, keyword);
        });
    }
}

void SpeechRecognizerEngineImpl::onConnectionStatusChanged(
    const alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status status,
    const alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::ChangedReason reason) {
    // no-op
}

void SpeechRecognizerEngineImpl::onConnectionStatusChanged(
    const alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status status,
    const std::vector<
        alexaClientSDK::avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::EngineConnectionStatus>&
        engineStatuses) {
    AACE_VERBOSE(LX(TAG).d("status", status).d("engineCount", engineStatuses.size()));
    m_connectionStatus = static_cast<aace::alexa::AlexaClient::ConnectionStatus>(status);
}

void SpeechRecognizerEngineImpl::onAgentAvailabilityStateChanged(
    alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
    AvailabilityState status,
    const std::string& reason) {
    std::string availability = status == AvailabilityState::AVAILABLE ? "AVAILABLE" : "UNAVAILABLE";
    AACE_INFO(LX(TAG).d("id", agentId).d("status", availability).d("reason", reason));

    std::lock_guard<std::mutex> lock(m_agentAvailabilityMutex);
    if (status == AvailabilityState::AVAILABLE) {
        m_availableAgents.insert(agentId);
    } else {
        m_availableAgents.erase(agentId);
    }
}

bool SpeechRecognizerEngineImpl::startCapture(
    std::shared_ptr<alexaClientSDK::capabilityAgents::aip::AudioProvider> audioProvider,
    alexaClientSDK::avsCommon::avs::AgentInitiator initiator,
    alexaClientSDK::avsCommon::avs::AudioInputStream::Index begin,
    alexaClientSDK::avsCommon::avs::AudioInputStream::Index keywordEnd,
    const std::string& keyword) {
    try {
        AACE_VERBOSE(LX(TAG));
        // ask the aip to start recognizing input
        auto startOfSpeechTimestamp = std::chrono::steady_clock::now();
        if (alexaClientSDK::avsCommon::avs::AgentInitiator::WAKEWORD == initiator) {
            computeStartOfSpeechTimestamp(
                begin, keywordEnd, audioProvider->format.sampleRateHz, audioProvider->stream, startOfSpeechTimestamp);
        }
        ThrowIfNot(
            m_audioInputProcessor
                ->recognize(*audioProvider, initiator, startOfSpeechTimestamp, begin, keywordEnd, keyword)
                .get(),
            "recognizeFailed");
        // notify the platform that we are expecting audio... if the platform returns
        // and error then we reset the expecting audio state and throw an exception
        if (isExpectingAudio() == false) {
            ThrowIfNot(startAudioInput(), "startAudioInputFailed");
        }

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()).d("id", getCurrentChannelId()));
        m_audioInputProcessor->resetState();
        return false;
    }
}

bool SpeechRecognizerEngineImpl::isWakewordSupported() {
    return m_wakewordEngineAdapter != nullptr;
}

bool SpeechRecognizerEngineImpl::isWakewordEnabled() {
    return m_wakewordEnabled;
}

bool SpeechRecognizerEngineImpl::enableWakewordDetection() {
    AACE_INFO(LX(TAG));
    try {
        // check to make sure wakeword is supported
        ThrowIfNot(isWakewordSupported(), "wakewordNotSupported");

        // check if wakeword detection is already enabled, and we are not in the initial
        // wakeword enable state (this is the first time we are enabling!)
        ReturnIf(m_wakewordEnabled && m_initialWakewordEnabledState == false, true);

        m_wakewordEnabled = true;
        m_initialWakewordEnabledState = false;

        std::lock_guard<std::mutex> lock_adaptor(m_wakewordAdapterMutex);
        // check if adapater is already enabled
        ReturnIf(m_wakeWordAdapterEnabled, true);

        // enable the wakeword engine adapter
        ThrowIfNot(m_wakewordEngineAdapter->enable(), "enableFailed");

        // set the wakeword enabled and expecting audio flags to true
        m_wakeWordAdapterEnabled = true;
        // tell the platform interface to start providing audio input
        ThrowIfNot(startAudioInput(), "startAudioInputFailed");
        return true;
    } catch (std::exception& ex) {
        //setExpectingAudioState( false );
        AACE_ERROR(LX(TAG, "enableWakewordDetection").d("reason", ex.what()));
        m_wakewordEnabled = false;
        std::lock_guard<std::mutex> lock_adaptor(m_wakewordAdapterMutex);
        m_wakeWordAdapterEnabled = false;
        return false;
    }
}

bool SpeechRecognizerEngineImpl::disableWakewordDetection() {
    AACE_INFO(LX(TAG));
    bool success = true;
    try {
        // check if wakeword detection is already disabled
        ReturnIfNot(m_wakewordEnabled, true);

        // check to make sure wakeword is supported
        ThrowIfNot(isWakewordSupported(), "wakewordNotSupported");

        m_wakewordEnabled = false;
        // check if 3P wakeword is enabled
        std::lock_guard<std::mutex> lock(m_wakewordEnableMutex);
        if (m_enabledWakewordSet.size() > 0) return true;

        // tell the platform to stop providing audio input
        ThrowIfNot(stopAudioInput(), "stopAudioInputFailed");
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "disableWakewordDetection").d("reason", ex.what()));
        m_wakewordEnabled = false;
        success = false;
    }
    std::lock_guard<std::mutex> lock_adaptor(m_wakewordAdapterMutex);
    m_wakewordEngineAdapter->disable();
    m_wakeWordAdapterEnabled = false;
    return success;
}

void SpeechRecognizerEngineImpl::onStateChanged(
    alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
    alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface::State state) {
    AACE_VERBOSE(LX(TAG).d("agentId", agentId).d("state", state));

    m_state = state;

    // state changed to BUSY means that either the StopCapture directive has been received
    // or the speech recognizer was stopped manually
    if (state == alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface::State::BUSY) {
        if (isExpectingAudio()) {
            m_speechRecognizerPlatformInterface->endOfSpeechDetected();
            if (m_wakewordEnabled == false) {
                if (stopAudioInput() == false) {
                    AACE_ERROR(LX(TAG, "handleAudioStateChanged").d("reason", "stopAudioInputFailed"));
                }
            }
        }
    } else if (state == alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface::State::IDLE) {
        if (isExpectingAudio() && m_wakewordEnabled == false) {
            if (stopAudioInput() == false) {
                AACE_ERROR(LX(TAG, "handleAudioStateChanged").d("reason", "stopAudioInputFailed"));
            }
        }
    } else if (
        state ==
        alexaClientSDK::avsCommon::sdkInterfaces::AudioInputProcessorObserverInterface::State::EXPECTING_SPEECH) {
        // call platform interface if we are starting the recognizer and the
        // wakeword engine was not already enabled...
        if (m_wakewordEnabled == false) {
            ThrowIfNot(startAudioInput(), "startAudioInputFailed");
        }
    }
}

bool SpeechRecognizerEngineImpl::enable3PWakewordDetection(const std::string& name, bool value) {
    AACE_INFO(LX(TAG));
    bool success = true;

    //Check if requested wakeword is in configured 3P wakeword list
    if (!is3PWakewordConfigured(name)) {
        AACE_ERROR(LX(TAG, "WakewordNotConfiguredAs3PWakeword").d("wakewod", name));
        return false;
    }
    if (value) {
        //Enable 3P Wakeword
        ReturnIf(is3PWakewordEnabled(name), true);
        success = enable3PWakewordAdapter();
        if (success) {
            std::lock_guard<std::mutex> lock(m_wakewordEnableMutex);
            m_enabledWakewordSet.insert(toUpper(name));
        }
    } else {
        //Disable 3P Wakeword
        std::lock_guard<std::mutex> lock(m_wakewordEnableMutex);
        if (m_enabledWakewordSet.find(name) != m_enabledWakewordSet.end()) {
            m_enabledWakewordSet.erase(toUpper(name));

            if (m_enabledWakewordSet.size() == 0) {
                success = disable3PWakewordAdapter();
            }
        } else {
            //already disabled
            AACE_WARN(LX(TAG, "Wakeword already disabled").d("wakewod", name));
            success = true;
        }
    }
    return success;
}

bool SpeechRecognizerEngineImpl::enable3PWakewordAdapter() {
    std::lock_guard<std::mutex> lock(m_wakewordAdapterMutex);
    try {
        // check to make sure wakeword is supported
        ThrowIfNot(isWakewordSupported(), "wakewordNotSupported");

        // check if adapater is already active
        ReturnIf(m_wakeWordAdapterEnabled, true);

        // enable the wakeword engine adapter
        ThrowIfNot(m_wakewordEngineAdapter->enable(), "enableFailed");

        // tell the platform interface to start providing audio input
        ThrowIfNot(startAudioInput(), "startAudioInputFailed");

        m_wakeWordAdapterEnabled = true;
        return true;
    } catch (std::exception& ex) {
        //setExpectingAudioState( false );
        AACE_ERROR(LX(TAG, "enable3PWakewordDetection").d("reason", ex.what()));
        m_wakeWordAdapterEnabled = false;
        return false;
    }
}

bool SpeechRecognizerEngineImpl::disable3PWakewordAdapter() {
    bool success = true;
    std::lock_guard<std::mutex> lock(m_wakewordAdapterMutex);
    try {
        // check if wakeword detection is already disabled
        ReturnIfNot(m_wakeWordAdapterEnabled, true);

        //If Alexa wakeword is enabled then don't disable the adapter.
        ReturnIf(m_wakewordEnabled, true);

        // check to make sure wakeword is supported
        ThrowIfNot(isWakewordSupported(), "wakewordNotSupported");

        // tell the platform to stop providing audio input
        ThrowIfNot(stopAudioInput(), "stopAudioInputFailed");
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "disableWakewordDetection").d("reason", ex.what()));
        success = false;
    }
    m_wakewordEngineAdapter->disable();
    m_wakeWordAdapterEnabled = false;
    return success;
}

bool SpeechRecognizerEngineImpl::is3PWakewordEnabled(const std::string& name) {
    AACE_INFO(LX(TAG));
    std::lock_guard<std::mutex> lock(m_wakewordEnableMutex);
    return (m_enabledWakewordSet.find(toUpper(name)) != m_enabledWakewordSet.end());
}

bool SpeechRecognizerEngineImpl::is3PWakewordConfigured(const std::string& name) {
    return (m_wakewordConfig.find(toUpper(name)) != m_wakewordConfig.end());
}

void SpeechRecognizerEngineImpl::onDialogTerminated(
    const std::string& assistantId,
    const std::string& dialogId,
    const std::string& reason) {
    AACE_DEBUG(LX(TAG)
                   .m("Dialog terminated by Arbitrator for")
                   .d("assistantId", assistantId)
                   .d("dialogId", dialogId)
                   .d("reason", reason));
    if (dialogId == m_registeredAgentDialogId) {
        // reset dialog Id
        std::lock_guard<std::mutex> lock(m_registeredAgentDialogIdMutex);
        m_registeredAgentDialogId = "";

        //reset AVS state and stop audio and video focus in case we have.
        //NOTE: also need to call cancelAlexaDialog once that code is checked in
        if (m_audioFocusManager != nullptr) m_audioFocusManager->stopForegroundActivity();

        if (m_visualFocusManager != nullptr) m_visualFocusManager->stopForegroundActivity();
        m_audioInputProcessor->resetState();
    }
}

void SpeechRecognizerEngineImpl::onAgentStateUpdated(
    const std::string& assistantId,
    const std::string& name,
    aace::arbitrator::ArbitratorEngineInterface::AgentState state) {
    // no-op
}

// AuthObserverInterface
void SpeechRecognizerEngineImpl::onAuthStateChange(
    alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State state,
    alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error error) {
    std::stringstream authState;
    std::stringstream authError;
    authState << state;
    authError << error;
    AACE_INFO(LX(TAG).d("state", authState.str()).d("error", authError.str()));
    // register/deregister agent with Arbitrator
    try {
        auto assistantId = std::to_string(aace::engine::utils::agent::AGENT_ID_ALEXA);
        if (state == alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED) {
            if (!m_isAgentRegistered) {
                ThrowIfNull(m_arbitratorService, "invalidArbitratorServiceInterface");
                ThrowIfNot(
                    m_arbitratorService->registerAgent(
                        assistantId, ALEXA_ASSISTANT_NAME, getAlexaAgentDialogStateRules()),
                    "registerAlexaAgentFailed");
                m_isAgentRegistered = true;
                AACE_DEBUG(LX(TAG).m("Registered Agent").d("assistantId", assistantId));
            }
        } else {
            if (m_isAgentRegistered) {
                ThrowIfNull(m_arbitratorService, "invalidArbitratorServiceInterface");
                ThrowIfNot(m_arbitratorService->deregisterAgent(assistantId), "deregisterAlexaAgentFailed");
                m_isAgentRegistered = false;
            }
        }
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG, "create").d("reason", "ex.what()"));
    }
}

// DialogUXStateObserverInterface
void SpeechRecognizerEngineImpl::onDialogUXStateChanged(
    alexaClientSDK::avsCommon::avs::AgentId::IdType agentId,
    DialogUXState newState) {
    AACE_INFO(LX(TAG).d("agentId", agentId).d("newState", newState));
    try {
        // set dialog state with the arbitrator
        auto assistantId = std::to_string(aace::engine::utils::agent::AGENT_ID_ALEXA);
        AACE_DEBUG(
            LX(TAG).m("Registered Agent").d("assistantId", assistantId).d("dialogId", m_registeredAgentDialogId));
        if (m_isAgentRegistered && !m_registeredAgentDialogId.empty()) {
            m_arbitratorService->setDialogState(
                assistantId, m_registeredAgentDialogId, convertDialogStateToString(newState));
            // stop the dialog with Arbitrator if state is IDLE or FINISHED
            if (newState == DialogUXState::IDLE || newState == DialogUXState::FINISHED) {
                m_arbitratorService->stopDialog(assistantId, m_registeredAgentDialogId);
                m_registeredAgentDialogId = "";
            }
        }
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
    }
}

std::map<std::string, bool> SpeechRecognizerEngineImpl::getAlexaAgentDialogStateRules() {
    std::map<std::string, bool> dialogStateRules;
    dialogStateRules.insert(std::pair<std::string, bool>(convertDialogStateToString(DialogUXState::IDLE), true));
    dialogStateRules.insert(std::pair<std::string, bool>(convertDialogStateToString(DialogUXState::LISTENING), false));
    dialogStateRules.insert(std::pair<std::string, bool>(convertDialogStateToString(DialogUXState::THINKING), false));
    dialogStateRules.insert(std::pair<std::string, bool>(convertDialogStateToString(DialogUXState::SPEAKING), true));
    dialogStateRules.insert(std::pair<std::string, bool>(convertDialogStateToString(DialogUXState::EXPECTING), false));
    dialogStateRules.insert(std::pair<std::string, bool>(convertDialogStateToString(DialogUXState::FINISHED), true));

    return dialogStateRules;
}

}  // namespace alexa
}  // namespace engine
}  // namespace aace

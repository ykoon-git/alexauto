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

#ifndef AACE_ENGINE_ALEXA_SPEECH_SYNTHESIZER_ENGINE_IMPL_H
#define AACE_ENGINE_ALEXA_SPEECH_SYNTHESIZER_ENGINE_IMPL_H

#include <memory>
#include <string>

#include <acsdk/MultiAgentInterface/AgentManagerInterface.h>
#include <AVSCommon/AVS/Attachment/AttachmentManagerInterface.h>
#include <AVSCommon/AVS/DialogUXStateAggregator.h>
#include <AVSCommon/SDKInterfaces/ContextManagerInterface.h>
#include <AVSCommon/SDKInterfaces/DirectiveSequencerInterface.h>
#include <AVSCommon/SDKInterfaces/Endpoints/EndpointCapabilitiesRegistrarInterface.h>
#include <AVSCommon/SDKInterfaces/ExceptionEncounteredSenderInterface.h>
#include <AVSCommon/SDKInterfaces/FocusManagerInterface.h>
#include <AVSCommon/SDKInterfaces/MessageSenderInterface.h>
#include <AVSCommon/SDKInterfaces/SpeechSynthesizerObserverInterface.h>
#include <AVSCommon/Utils/Metrics/MetricRecorderInterface.h>
#include <Captions/CaptionManagerInterface.h>
#include <ContextManager/ContextManager.h>
#include <SpeechSynthesizer/SpeechSynthesizer.h>

#include <AACE/Alexa/SpeechSynthesizer.h>
#include <AACE/Engine/Audio/AudioManagerInterface.h>

#include "AudioChannelEngineImpl.h"

namespace aace {
namespace engine {
namespace alexa {

class SpeechSynthesizerEngineImpl : public AudioChannelEngineImpl {
private:
    SpeechSynthesizerEngineImpl(std::shared_ptr<aace::alexa::SpeechSynthesizer> speechSynthesizerPlatformInterface);

    bool initialize(
        std::shared_ptr<aace::engine::audio::AudioOutputChannelInterface> audioOutputChannel,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::endpoints::EndpointCapabilitiesRegistrarInterface>
            capabilitiesRegistrar,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> focusManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface> contextManager,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::attachment::AttachmentManagerInterface> attachmentManager,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::DialogUXStateAggregator> dialogUXStateAggregator,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerManagerInterface> speakerManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface> exceptionSender,
        std::shared_ptr<alexaClientSDK::avsCommon::utils::metrics::MetricRecorderInterface> metricRecorder,
        std::shared_ptr<alexaClientSDK::captions::CaptionManagerInterface> captionManager,
        std::shared_ptr<alexaClientSDK::multiAgentInterface::AgentManagerInterface> agentManager = nullptr);

public:
    static std::shared_ptr<SpeechSynthesizerEngineImpl> create(
        std::shared_ptr<aace::alexa::SpeechSynthesizer> speechSynthesizerPlatformInterface,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::endpoints::EndpointCapabilitiesRegistrarInterface>
            capabilitiesRegistrar,
        std::shared_ptr<aace::engine::audio::AudioManagerInterface> audioManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface> focusManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ContextManagerInterface> contextManager,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::attachment::AttachmentManagerInterface> attachmentManager,
        std::shared_ptr<alexaClientSDK::avsCommon::avs::DialogUXStateAggregator> dialogUXStateAggregator,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerManagerInterface> speakerManager,
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface> exceptionSender,
        std::shared_ptr<alexaClientSDK::avsCommon::utils::metrics::MetricRecorderInterface> metricRecorder,
        std::shared_ptr<alexaClientSDK::captions::CaptionManagerInterface> captionManager = nullptr,
        std::shared_ptr<alexaClientSDK::multiAgentInterface::AgentManagerInterface> agentManager = nullptr);

    std::shared_ptr<alexaClientSDK::capabilityAgents::speechSynthesizer::SpeechSynthesizer>
    getSpeechSynthesizerCapabilityAgent() {
        return m_speechSynthesizerCapabilityAgent;
    }

protected:
    virtual void doShutdown() override;

private:
    std::shared_ptr<aace::alexa::SpeechSynthesizer> m_speechSynthesizerPlatformInterface;
    std::shared_ptr<alexaClientSDK::capabilityAgents::speechSynthesizer::SpeechSynthesizer>
        m_speechSynthesizerCapabilityAgent;
};

}  // namespace alexa
}  // namespace engine
}  // namespace aace

#endif  // AACE_ENGINE_ALEXA_SPEECH_SYNTHESIZER_ENGINE_IMPL_H

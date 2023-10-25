/*
 * Copyright Amazon.com, Inc. and its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: LicenseRef-.amazon.com.-ASL-1.0
 *
 * Licensed under the Amazon Software License (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/asl/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <climits>
#include <AACE/Engine/Core/EngineMacros.h>
#include <AACE/Engine/Audio/AudioManagerInterface.h>
#include <AACE/Engine/Utils/JSON/JSON.h>
#include <AACE/Engine/Alexa/WakewordEngineManager.h>
#include "AACE/Engine/PropertyManager/PropertyManagerServiceInterface.h"
#include <AVSCommon/Utils/AudioFormat.h>
#include "AACE/Alexa/AlexaProperties.h"

#include "LoopbackDetectorEngineService.h"
#include "LoopbackDetector.h"

namespace aace {
namespace engine {
namespace loopbackDetector {

using namespace alexaClientSDK::avsCommon::utils;

// String to identify log entries originating from this file.
static const std::string TAG("aace.loopback.LoopbackDetectorEngineService");

static const std::string FACTORY_NAME("aace.loopbackDetector");

// register the service
REGISTER_SERVICE(LoopbackDetectorEngineService);

LoopbackDetectorEngineService::LoopbackDetectorEngineService(const core::ServiceDescription& description) :
        core::EngineService(description) {
}

bool LoopbackDetectorEngineService::configure(std::shared_ptr<std::istream> configuration) {
    try {
        auto document = aace::engine::utils::json::parse(configuration);
        ThrowIfNull(document, "parseConfigurationStreamFailed");

        auto configRoot = document->GetObject();

        if (configRoot.HasMember("wakewordEngine") && configRoot["wakewordEngine"].IsString()) {
            m_wakewordEngineName = configRoot["wakewordEngine"].GetString();
        }

        return true;
    } catch (std::exception& ex) {
        AACE_WARN(LX(TAG, "configure").d("reason", ex.what()));
        return false;
    }
}

bool LoopbackDetectorEngineService::preRegister() {
    try {
        auto alexaEngineService = getContext()->getService<alexa::AlexaEngineService>();
        ThrowIfNull(alexaEngineService, "AlexaEngineService is not available");

        auto initiatorVerifierFactory = [this]() {
            if (!m_loopbackDetector) {
                prepareVerifier();
            }
            // We need the caller to use the overloaded methods in InitiatorVerifier interface rather than LoopbackDetector
            std::shared_ptr<aace::engine::alexa::InitiatorVerifier> initiatorVerifier = m_loopbackDetector;
            return initiatorVerifier;
        };

        ThrowIfNot(
            alexaEngineService->registerServiceFactory<alexa::InitiatorVerifier>(initiatorVerifierFactory),
            "Failed to register factory");

        return true;
    } catch (std::exception& ex) {
        AACE_WARN(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool LoopbackDetectorEngineService::prepareVerifier() {
    try {
        auto alexaEngineService = getContext()->getService<alexa::AlexaEngineService>();
        ThrowIfNull(alexaEngineService, "AlexaEngineService is not available");

        auto wwManager = alexaEngineService->getServiceInterface<alexa::WakewordEngineManager>();
        ThrowIfNull(wwManager, "WakewordEngineManager has not been registered");

        auto secondaryAdapter =
            wwManager->createAdapter(alexa::WakewordEngineManager::AdapterType::SECONDARY, m_wakewordEngineName);

        AudioFormat audioFormat;
        audioFormat.sampleRateHz = 16000;
        audioFormat.sampleSizeInBits = 2 * CHAR_BIT;
        audioFormat.numChannels = 1;
        audioFormat.endianness = AudioFormat::Endianness::LITTLE;
        audioFormat.encoding = AudioFormat::Encoding::LPCM;
        audioFormat.layout = AudioFormat::Layout::INTERLEAVED;

        auto audioManager = getContext()->getServiceInterface<audio::AudioManagerInterface>("aace.audio");
        auto propertyManager =
            getContext()->getServiceInterface<aace::engine::propertyManager::PropertyManagerServiceInterface>(
                "aace.propertyManager");
        ThrowIfNull(propertyManager, "nullPropertyManagerServiceInterface");
        auto locale = propertyManager->getProperty(aace::alexa::property::LOCALE);

        m_loopbackDetector = LoopbackDetector::create(locale, audioFormat, audioManager, secondaryAdapter);
        ThrowIfNull(m_loopbackDetector, "Failed to create LoopbackDetector");

        return true;
    } catch (std::exception& ex) {
        AACE_WARN(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool LoopbackDetectorEngineService::shutdown() {
    if (m_loopbackDetector) {
        m_loopbackDetector->shutdown();
    }
    m_loopbackDetector.reset();
    return true;
}

}  // namespace loopbackDetector
}  // namespace engine
}  // namespace aace

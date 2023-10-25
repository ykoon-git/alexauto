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

#ifndef AASB_ENGINE_AUDIO_AASB_AUDIO_ENGINE_SERVICE_H
#define AASB_ENGINE_AUDIO_AASB_AUDIO_ENGINE_SERVICE_H

#include <AACE/Engine/Audio/AudioEngineService.h>
#include <AACE/Engine/MessageBroker/MessageBrokerEngineService.h>
#include <AACE/Engine/MessageBroker/MessageHandlerEngineService.h>

namespace aasb {
namespace engine {
namespace audio {

class AASBAudioEngineService : public aace::engine::messageBroker::MessageHandlerEngineService {
public:
    DESCRIBE(
        "aasb.audio",
        VERSION("1.0"),
        DEPENDS(aace::engine::messageBroker::MessageBrokerEngineService),
        DEPENDS(aace::engine::audio::AudioEngineService))

private:
    AASBAudioEngineService(const aace::engine::core::ServiceDescription& description);

public:
    virtual ~AASBAudioEngineService() = default;

private:
    bool postRegister() override;
};

}  // namespace audio
}  // namespace engine
}  // namespace aasb

#endif  // AASB_ENGINE_AUDIO_AASB_AUDIO_ENGINE_SERVICE_H

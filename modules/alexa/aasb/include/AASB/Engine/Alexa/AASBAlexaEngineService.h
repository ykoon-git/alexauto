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

#ifndef AASB_ENGINE_ALEXA_AASB_ALEXA_ENGINE_SERVICE_H
#define AASB_ENGINE_ALEXA_AASB_ALEXA_ENGINE_SERVICE_H

#include <unordered_map>

#include <AACE/Engine/MessageBroker/MessageHandlerEngineService.h>
#include <AASB/Engine/Audio/AASBAudioEngineService.h>
#include <AASB/Engine/Arbitrator/AASBArbitratorEngineService.h>
#include <AASB/Engine/Wakeword/AASBWakewordEngineService.h>

#include "AASBLocalMediaSource.h"

namespace aasb {
namespace engine {
namespace alexa {

class AASBAlexaEngineService : public aace::engine::messageBroker::MessageHandlerEngineService {
public:
    DESCRIBE(
        "aasb.alexa",
        VERSION("1.0"),
        DEPENDS(aace::engine::messageBroker::MessageBrokerEngineService),
        DEPENDS(aasb::engine::audio::AASBAudioEngineService),
        DEPENDS(aasb::engine::arbitrator::AASBArbitratorEngineService),
        DEPENDS(aasb::engine::wakeword::AASBWakewordEngineService))

private:
    AASBAlexaEngineService(const aace::engine::core::ServiceDescription& description);

    bool registerLocalMediaSourceMessageHandlers(
        std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker);

protected:
    bool postRegister() override;
    bool configureMessageInterface(const std::string& name, bool enabled, std::istream& configuration) override;

private:
    bool configureLocalMediaSource(std::istream& configuration);

public:
    virtual ~AASBAlexaEngineService() = default;

private:
    struct EnumHash {
        template <typename T>
        std::size_t operator()(T t) const {
            return static_cast<std::size_t>(t);
        }
    };

    std::unordered_map<aace::alexa::LocalMediaSource::Source, std::shared_ptr<AASBLocalMediaSource>, EnumHash>
        m_localMediaSourceMap;
};

}  // namespace alexa
}  // namespace engine
}  // namespace aasb

#endif

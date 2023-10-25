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

#ifndef AASB_ENGINE_MESSAGING_AASB_MESSAGING_ENGINE_SERVICE_H
#define AASB_ENGINE_MESSAGING_AASB_MESSAGING_ENGINE_SERVICE_H

#include <AACE/Engine/MessageBroker/MessageHandlerEngineService.h>
#include <AACE/Engine/MessageBroker/MessageBrokerEngineService.h>

namespace aasb {
namespace engine {
namespace messaging {

class AASBMessagingEngineService : public aace::engine::messageBroker::MessageHandlerEngineService {
public:
    DESCRIBE("aasb.messaging", VERSION("1.0"), DEPENDS(aace::engine::messageBroker::MessageBrokerEngineService))

private:
    AASBMessagingEngineService(const aace::engine::core::ServiceDescription& description);

protected:
    bool postRegister() override;

public:
    virtual ~AASBMessagingEngineService() = default;
};

}  // namespace messaging
}  // namespace engine
}  // namespace aasb

#endif  // AASB_ENGINE_MESSAGING_AASB_MESSAGING_ENGINE_SERVICE_H

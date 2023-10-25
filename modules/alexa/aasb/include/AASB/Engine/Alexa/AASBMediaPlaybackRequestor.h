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

#ifndef AASB_ENGINE_ALEXA_AASB_MEDIA_PLAYBACK_REQUESTOR
#define AASB_ENGINE_ALEXA_AASB_MEDIA_PLAYBACK_REQUESTOR

#include <AACE/Alexa/MediaPlaybackRequestor.h>
#include <AACE/Engine/MessageBroker/MessageBrokerInterface.h>

namespace aasb {
namespace engine {
namespace alexa {

class AASBMediaPlaybackRequestor
        : public aace::alexa::MediaPlaybackRequestor
        , public std::enable_shared_from_this<AASBMediaPlaybackRequestor> {
private:
    AASBMediaPlaybackRequestor() = default;

    bool initialize(std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker);

public:
    static std::shared_ptr<AASBMediaPlaybackRequestor> create(
        std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker);

    // aace::alexa::MediaPlaybackRequestor
    void mediaPlaybackResponse(MediaPlaybackRequestStatus mediaPlaybackRequestStatus);

private:
    std::weak_ptr<aace::engine::messageBroker::MessageBrokerInterface> m_messageBroker;
};

}  // namespace alexa
}  // namespace engine
}  // namespace aasb

#endif  //AASB_ENGINE_ALEXA_AASB_MEDIA_PLAYBACK_REQUESTOR

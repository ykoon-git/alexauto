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

#ifndef AACE_ENGINE_CONNECTIVITY_INITIATE_DATA_PLAN_SUBSCRIPTION_REQUEST_H
#define AACE_ENGINE_CONNECTIVITY_INITIATE_DATA_PLAN_SUBSCRIPTION_REQUEST_H

#include <atomic>
#include <future>
#include <string>

#include <AVSCommon/AVS/MessageRequest.h>
#include <AVSCommon/SDKInterfaces/MessageRequestObserverInterface.h>

namespace aace {
namespace engine {
namespace connectivity {

/**
 * This class extends @c MessageRequest to fulfill a promise upon sending completion.
 * Currently @c MessageRequestObserverInterface callbacks do not return an identifier. This makes it difficult
 * to associate callbacks when multiple requests are sent.
 * The future allows multiple messages to be sent, and their returns to be differentiated.
 */
class ConnectivityMessageRequest : public alexaClientSDK::avsCommon::avs::MessageRequest {
public:
    /**
     * @copyDoc avsCommon::avs::MessageRequest()
     *
     * Construct a @c MessageRequest that will fulfill the promise upon completion.
     *
     * @param jsonContent The JSON content to be sent to AVS.
     * @param messageCompletePromise The promise to set with the results.
     */
    ConnectivityMessageRequest(const std::string& jsonContent, std::promise<bool> messageCompletePromise);

    /// @note We do not override exceptionEncountered because sendCompleted is still expected on server exceptions.

    /// @name MessageRequest functions.
    /// @{
    void sendCompleted(
        alexaClientSDK::avsCommon::sdkInterfaces::MessageRequestObserverInterface::Status status) override;
    /// @}

private:
    /// Guard that we only set the promise once.
    std::atomic<bool> m_isPromiseSet;

    /// The promise to set once the request returns.
    std::promise<bool> m_messageCompletePromise;
};

}  // namespace connectivity
}  // namespace engine
}  // namespace aace

#endif  // AACE_ENGINE_CONNECTIVITY_INITIATE_DATA_PLAN_SUBSCRIPTION_REQUEST_H

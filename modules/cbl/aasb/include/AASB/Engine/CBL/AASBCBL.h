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
#ifndef AASB_ENGINE_CBL_AASB_CBL_H
#define AASB_ENGINE_CBL_AASB_CBL_H

#include <AACE/CBL/CBL.h>
#include <AACE/Engine/MessageBroker/MessageBrokerInterface.h>

#include <string>
#include <memory>
#include <future>

namespace aasb {
namespace engine {
namespace cbl {

class AASBCBL
        : public aace::cbl::CBL
        , public std::enable_shared_from_this<AASBCBL> {
private:
    AASBCBL() = default;

    bool initialize(std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker);

public:
    static std::shared_ptr<AASBCBL> create(
        std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker);

    // aace::cbl::CBL
    void cblStateChanged(CBLState state, CBLStateChangedReason reason, const std::string& url, const std::string& code)
        override;
    void clearRefreshToken() override;
    void setRefreshToken(const std::string& refreshToken) override;
    std::string getRefreshToken() override;
    void setUserProfile(const std::string& name, const std::string& email) override;

private:
    std::weak_ptr<aace::engine::messageBroker::MessageBrokerInterface> m_messageBroker;

    std::string m_cachedRefreshToken;
};

}  // namespace cbl
}  // namespace engine
}  // namespace aasb

#endif  // AASB_ENGINE_CBL_AASB_CBL_H
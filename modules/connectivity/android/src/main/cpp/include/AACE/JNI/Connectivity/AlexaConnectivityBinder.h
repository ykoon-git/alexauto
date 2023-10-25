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

#ifndef AACE_JNI_CONNECTIVITY_CONNECTIVITY_BINDER_H
#define AACE_JNI_CONNECTIVITY_CONNECTIVITY_BINDER_H

#include <AACE/Connectivity/AlexaConnectivity.h>
#include <AACE/JNI/Core/PlatformInterfaceBinder.h>

namespace aace {
namespace jni {
namespace connectivity {

class AlexaConnectivityHandler : public aace::connectivity::AlexaConnectivity {
public:
    AlexaConnectivityHandler(jobject obj);

    // aace::connectivity::AlexaConnectivity interface
    std::string getConnectivityState() override;
    std::string getIdentifier() override;
    void connectivityEventResponse(const std::string& token, StatusCode statusCode) override;

private:
    JObject m_obj;
};

class AlexaConnectivityBinder : public aace::jni::core::PlatformInterfaceBinder {
public:
    AlexaConnectivityBinder(jobject obj);

    std::shared_ptr<aace::core::PlatformInterface> getPlatformInterface() override {
        return m_alexaConnectivity;
    }

    std::shared_ptr<AlexaConnectivityHandler> getAlexaConnectivity() {
        return m_alexaConnectivity;
    }

private:
    std::shared_ptr<AlexaConnectivityHandler> m_alexaConnectivity;
};

//
// JStatusCode
//
class JAlexaConnectivityHandlerStatusCodeConfig : public EnumConfiguration<AlexaConnectivityHandler::StatusCode> {
public:
    using T = AlexaConnectivityHandler::StatusCode;

    const char* getClassName() override {
        return "com/amazon/aace/connectivity/AlexaConnectivity$StatusCode";
    }

    std::vector<std::pair<T, std::string>> getConfiguration() override {
        return {{T::SUCCESS, "SUCCESS"}, {T::FAIL, "FAIL"}};
    }
};

using JStatusCode = JEnum<AlexaConnectivityHandler::StatusCode, JAlexaConnectivityHandlerStatusCodeConfig>;

}  // namespace connectivity
}  // namespace jni
}  // namespace aace

#endif  // AACE_JNI_CONNECTIVITY_CONNECTIVITY_BINDER_H

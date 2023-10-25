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

#ifndef AACE_JNI_ALEXA_DO_NOT_DISTURB_BINDER_H
#define AACE_JNI_ALEXA_DO_NOT_DISTURB_BINDER_H

#include <AACE/Alexa/DoNotDisturb.h>
#include <AACE/JNI/Core/PlatformInterfaceBinder.h>

namespace aace {
namespace jni {
namespace alexa {

class DoNotDisturbHandler : public aace::alexa::DoNotDisturb {
public:
    DoNotDisturbHandler(jobject obj);

    // aace::alexa::DoNotDisturb
    void setDoNotDisturb(const bool doNotDisturb) override;

private:
    JObject m_obj;
};

class DoNotDisturbBinder : public aace::jni::core::PlatformInterfaceBinder {
public:
    DoNotDisturbBinder(jobject obj);

    std::shared_ptr<aace::core::PlatformInterface> getPlatformInterface() override {
        return m_doNotDisturbHandler;
    }

    std::shared_ptr<DoNotDisturbHandler> getDoNotDisturb() {
        return m_doNotDisturbHandler;
    }

private:
    std::shared_ptr<DoNotDisturbHandler> m_doNotDisturbHandler;
};

}  // namespace alexa
}  // namespace jni
}  // namespace aace

#endif  // AACE_JNI_ALEXA_DO_NOT_DISTURB_BINDER_H

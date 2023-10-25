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

#ifndef AACE_ENGINE_AACE_EXTERNAL_MEDIA_ADAPTER_REGISTRATION_INTERFACE_H
#define AACE_ENGINE_AACE_EXTERNAL_MEDIA_ADAPTER_REGISTRATION_INTERFACE_H

#include <string>

namespace aace {
namespace engine {
namespace alexa {

class ExternalMediaAdapterRegistrationInterface {
public:
    virtual ~ExternalMediaAdapterRegistrationInterface() = default;

public:
    virtual std::string getAgent() = 0;
    virtual std::string getPlayerId() = 0;
};

}  // namespace alexa
}  // namespace engine
}  // namespace aace

#endif  // AACE_ENGINE_AACE_EXTERNAL_MEDIA_ADAPTER_REGISTRATION_INTERFACE_H

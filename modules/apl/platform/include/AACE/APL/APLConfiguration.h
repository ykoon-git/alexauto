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

#ifndef AACE_APL_APL_CONFIGURATION_H
#define AACE_APL_APL_CONFIGURATION_H

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <chrono>

#include <AACE/Core/EngineConfiguration.h>

#include "AACE/APL/APLEngineInterface.h"

/** @file */

namespace aace {
namespace apl {
namespace config {

/**
 * A factory interface for creating APL configuration objects
 */
class APLConfiguration {
public:
    /**
     * enum specifying the configurable AlexaPresentation timeout.
     */
    enum class AlexaPresentationTimeoutType {
        /**
         *  RenderDocument timeout in ms for display card timeout.
         */
        DISPLAY_DOCUMENT_INTERACTION_IDLE_TIMEOUT
    };

    /**
    * Identifies a AlexaPresentationTimeout configuration with a type and value pair.
    */
    using AlexaPresentationTimeout = std::pair<AlexaPresentationTimeoutType, std::chrono::milliseconds>;

    /**
     * Factory method used to programmatically generate Alexa Presentation configuration data.
     * This is an optional configuration. Following are the accepted keys and their description.
     * - displayDocumentInteractionIdleTimeout If present, the timeout in ms for display card timeout.
     * The data generated by this method is equivalent to providing the following JSON
     * values in a configuration file:
     *
     * @code{.json}
     * {
     *   "alexaPresentationCapabilityAgent": {
     *      "displayDocumentInteractionIdleTimeout": <TIMEOUT_IN_MS>
     *   }
     * }
     * @endcode
     *
     * @param [in] timeoutList A list of @c AlexaPresentationTimeout type and value pairs
     *
     */
    static std::shared_ptr<aace::core::config::EngineConfiguration> createAlexaPresentationTimeoutConfig(
        const std::vector<AlexaPresentationTimeout>& timeoutList);
};

}  // namespace config
}  // namespace apl
}  // namespace aace

#endif  // AACE_APL_APL_CONFIGURATION_H

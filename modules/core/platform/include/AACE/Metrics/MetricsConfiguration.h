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

#ifndef AACE_METRICS_METRICS_CONFIGURATION_H
#define AACE_METRICS_METRICS_CONFIGURATION_H

#include "AACE/Core/EngineConfiguration.h"

namespace aace {
namespace metrics {
namespace config {

/**
 * @c MetricsConfiguration is a utility class to create metrics service
 * @c aace::core::config::EngineConfiguration objects.
 */
class MetricsConfiguration {
public:
    /**
     * Factory function used to programmatically generate device info
     * configuration for the metrics Engine service. The data generated by this
     * function is equivalent to providing the following JSON object in a
     * configuration file:
     *
     * @code{.json}
     * {
     *      "aace.metrics" : {
     *          "metricDeviceIdTag": "<ALPHANUMERIC_TAG>"
     *      }
     * }
     * @endcode
     *
     * @param [in] metricDeviceIdTag A tag that Auto SDK Engine will use in
     *             combination with DSN to generate a unique anonymous device
     *             identifier. Neither Alexa nor Auto SDK will store this tag
     *             and hence cannot reverse the hash to identify a single DSN
     *             from an individual metric. The @c metricDeviceIdTag may be
     *             any nonempty alphanumeric string that does not change across
     *             device reboots, factory resets, app data reset, or software
     *             updates. The recommended value is a 32 character string that
     *             is not the DSN or VIN. The value may be unique to an
     *             individual vehicle, provided it is stable, but it is not
     *             required to be unique.
     */
    static std::shared_ptr<aace::core::config::EngineConfiguration> createMetricsTagConfig(
        const std::string& metricDeviceIdTag);

    /**
     * Factory function used to programmatically generate storage configuration
     * for the metrics Engine service. The data generated by this function
     * is equivalent to providing the following JSON object in a configuration
     * file:
     *
     * @code{.json}
     * {
     *      "aace.metrics" : {
     *          "metricStoragePath": "</some/path/to/metrics/directory/>"
     *      }
     * }
     * @endcode
     *
     * @param [in] storagePath An absolute path to a directory where metrics
     *             may be stored prior to upload. The directory must exist and
     *             should not be used for any other purpose.
     */
    static std::shared_ptr<aace::core::config::EngineConfiguration> createMetricsStorageConfig(
        const std::string& storagePath);
};

}  // namespace config
}  // namespace metrics
}  // namespace aace

#endif  // AACE_METRICS_METRICS_CONFIGURATION_H

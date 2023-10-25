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

#include <AACE/Engine/Core/EngineMacros.h>

#include <AACE/Engine/DeviceUsage/DeviceUsageEngineImpl.h>
#include <AACE/Engine/Metrics/CounterDataPointBuilder.h>
#include <AACE/Engine/Metrics/DurationDataPointBuilder.h>
#include <AACE/Engine/Metrics/StringDataPointBuilder.h>
#include <AACE/Engine/Metrics/MetricEventBuilder.h>
#include <AACE/Engine/Utils/Agent/AgentId.h>

#include <nlohmann/json.hpp>

namespace aace {
namespace engine {
namespace deviceUsage {

using nlohmann::json;
using namespace aace::engine::metrics;
using namespace aace::engine::utils::agent;

/// String to identify log entries originating from this file.
static const std::string TAG("aace.deviceUsage.DeviceUsageEngineImpl");

DeviceUsageEngineImpl::DeviceUsageEngineImpl(
    std::shared_ptr<aace::deviceUsage::DeviceUsage> deviceUsagePlatformInterface,
    std::shared_ptr<MetricRecorderServiceInterface> metricRecorder) :
        m_deviceUsagePlatformInterface{deviceUsagePlatformInterface}, m_metricRecorder{metricRecorder} {
}

std::shared_ptr<DeviceUsageEngineImpl> DeviceUsageEngineImpl::create(
    std::shared_ptr<aace::deviceUsage::DeviceUsage> deviceUsagePlatformInterface,
    std::shared_ptr<aace::engine::core::EngineContext> engineContext) {
    AACE_INFO(LX(TAG));
    try {
        ThrowIfNull(deviceUsagePlatformInterface, "invalidPlatformInterface");
        ThrowIfNull(engineContext, "invalidEngineContext");

        auto metricRecorder =
            engineContext->getServiceInterface<aace::engine::metrics::MetricRecorderServiceInterface>("aace.metrics");
        ThrowIfNull(metricRecorder, "nullMetricRecorder");

        auto deviceUsageEngineImpl = std::shared_ptr<DeviceUsageEngineImpl>(
            new DeviceUsageEngineImpl(deviceUsagePlatformInterface, metricRecorder));

        // Set the platform engine interface reference.
        deviceUsagePlatformInterface->setEngineInterface(deviceUsageEngineImpl);

        return deviceUsageEngineImpl;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return nullptr;
    }
}

void DeviceUsageEngineImpl::onReportNetworkDataUsage(const std::string& usage) {
    AACE_INFO(LX(TAG));
    try {
        json dataUsage = json::parse(usage);

        ThrowIf(dataUsage.empty(), "emptyJSON");

        ThrowIfNot(
            (dataUsage.contains("startTimeStamp") && dataUsage["startTimeStamp"].is_number()), "startTimeStampInvalid");
        ThrowIfNot(
            (dataUsage.contains("endTimeStamp") && dataUsage["endTimeStamp"].is_number()), "endTimeStampInvalid");

        long startTimeStamp = dataUsage["startTimeStamp"];
        long endTimeStamp = dataUsage["endTimeStamp"];

        ThrowIfNot(dataUsage.contains("bytesUsage"), "missingBytesUsage");

        auto bytesUsage = dataUsage["bytesUsage"];
        ThrowIf(bytesUsage.empty(), "emptyBytesUsage");

        ThrowIfNot((bytesUsage.contains("rxBytes") && bytesUsage["rxBytes"].is_number()), "rxBytesInvalid");
        ThrowIfNot((bytesUsage.contains("txBytes") && bytesUsage["txBytes"].is_number()), "txBytesInvalid");

        long receivedBytes = bytesUsage["rxBytes"];
        long transmittedBytes = bytesUsage["txBytes"];
        long totalBytes = receivedBytes + transmittedBytes;

        AACE_DEBUG(LX(TAG)
                       .m("Bytes usage is : ")
                       .d("rxBytes", receivedBytes)
                       .d("txBytes", transmittedBytes)
                       .d("totalBytes", totalBytes));

        auto networkInterfaceType = dataUsage["networkInterfaceType"];
        ThrowIf(networkInterfaceType.empty(), "emptyNetworkInterfaceType");

        if (receivedBytes == 0 && transmittedBytes == 0 && totalBytes == 0) {
            AACE_DEBUG(LX(TAG).m("Not recording device usage since consumption is zero"));
        } else {
            std::vector<DataPoint> dps = {
                CounterDataPointBuilder{}.withName("rxBytes").increment(receivedBytes).build(),
                CounterDataPointBuilder{}.withName("txBytes").increment(transmittedBytes).build(),
                CounterDataPointBuilder{}.withName("totalBytes").increment(totalBytes).build(),
                CounterDataPointBuilder{}.withName("startTimeStamp").increment(startTimeStamp).build(),
                CounterDataPointBuilder{}.withName("endTimeStamp").increment(endTimeStamp).build(),
                StringDataPointBuilder{}.withName("networkInterfaceType").withValue(networkInterfaceType).build()};

            if (dataUsage.contains("dataPlanType") && dataUsage["dataPlanType"].is_string() &&
                !dataUsage["dataPlanType"].empty()) {
                std::string dataPlanType = dataUsage["dataPlanType"];
                dps.push_back(StringDataPointBuilder{}.withName("dataPlanType").withValue(dataPlanType).build());
            }

            auto metricBuilder = MetricEventBuilder()
                                     .withSourceName("NetworkDataUsageReport")
                                     .withAgentId(AGENT_ID_ALL)
                                     .addDataPoints(dps);
            recordMetric(m_metricRecorder.lock(), metricBuilder.build());
        }

    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
    }
}

void DeviceUsageEngineImpl::doShutdown() {
    AACE_INFO(LX(TAG));
    if (m_deviceUsagePlatformInterface != nullptr) {
        m_deviceUsagePlatformInterface->setEngineInterface(nullptr);
        m_deviceUsagePlatformInterface.reset();
    }
}

}  // namespace deviceUsage
}  // namespace engine
}  // namespace aace

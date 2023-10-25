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

#include <string>
#include <unordered_map>
#include <forward_list>
#ifndef NO_SIGPIPE
#include <csignal>
#endif

#include <AACE/Engine/Metrics/CounterDataPointBuilder.h>
#include <AACE/Engine/Metrics/StringDataPointBuilder.h>
#include <AACE/Engine/Metrics/MetricEventBuilder.h>

#include "AACE/Engine/Core/EngineImpl.h"
#include "AACE/Engine/Core/EngineService.h"
#include "AACE/Engine/Core/EngineMacros.h"
#include "AACE/Engine/Core/EngineVersion.h"
#include "AACE/Engine/Utils/JSON/JSON.h"
#include "AACE/Core/CoreProperties.h"

// default Engine constructor
std::shared_ptr<aace::core::Engine> aace::core::Engine::create() {
    return aace::engine::core::EngineImpl::create();
}

namespace aace {
namespace engine {
namespace core {

// json namespace alias
namespace json = aace::engine::utils::json;

using namespace aace::engine::metrics;

/// String to identify log entries originating from this file.
static const std::string TAG("aace.core.EngineImpl");

/// Prefix for Engine metrics
static const std::string METRIC_PREFIX = "ENGINE-";

/// Source name for Engine lifecycle event metrics
static const std::string METRIC_SOURCE = METRIC_PREFIX + "EngineLifecycleEvent";

/// Key name for Engine lifecycle event latency metrics
static const std::string METRIC_LIFECYCLE_LATENCY_KEY = "EngineLifecycleEventLatency";

/// Key name for Engine lifecycle event success count metrics
static const std::string METRIC_LIFECYCLE_EVENT_SUCCESS_COUNT = "EngineLifecycleEventSuccessCount";

/// Key name for Engine lifecycle event failure count metrics
static const std::string METRIC_LIFECYCLE_EVENT_ERROR_COUNT = "EngineLifecycleEventErrorCount";

/// Key name for Engine lifecycle stage metric dimension
static const std::string METRIC_LIFECYCLE_STAGE_TYPE_KEY = "LifecycleStage";

/// Configure stage metric dimension
static const std::string METRIC_LIFECYCLE_STAGE_CONFIG = "Configure";

/// Start stage metric dimension
static const std::string METRIC_LIFECYCLE_STAGE_START = "Start";

/// Stop stage metric dimension
static const std::string METRIC_LIFECYCLE_STAGE_STOP = "Stop";

/// Shutdown stage metric dimension
static const std::string METRIC_LIFECYCLE_STAGE_SHUTDOWN = "Shutdown";

/**
 * Records a metric with the specified data.
 * Uses the default context for Engine.
 */
static void submitLifecycleMetric(
    const std::shared_ptr<MetricRecorderServiceInterface>& recorder,
    const DataPoint& latencyDataPoint,
    const std::string& lifecycleStage,
    bool isSuccess) {
    auto metricBuilder = MetricEventBuilder()
                             .withSourceName(METRIC_SOURCE)
                             .withAlexaAgentId()
                             .withIdentityType(IdentityType::UNIQUE)
                             .withPriority(Priority::HIGH)
                             .withBufferType(BufferType::SKIP_BUFFER);
    metricBuilder.addDataPoint(latencyDataPoint);
    metricBuilder.addDataPoint(
        CounterDataPointBuilder{}.withName(METRIC_LIFECYCLE_EVENT_SUCCESS_COUNT).increment(isSuccess ? 1 : 0).build());
    metricBuilder.addDataPoint(
        CounterDataPointBuilder{}.withName(METRIC_LIFECYCLE_EVENT_ERROR_COUNT).increment(isSuccess ? 0 : 1).build());
    metricBuilder.addDataPoint(
        StringDataPointBuilder{}.withName(METRIC_LIFECYCLE_STAGE_TYPE_KEY).withValue(lifecycleStage).build());
    try {
        recordMetric(recorder, metricBuilder.build());
    } catch (std::invalid_argument& ex) {
        AACE_ERROR(LX(TAG).m("Failed to record metric").d("reason", ex.what()));
    }
}

std::shared_ptr<EngineImpl> EngineImpl::create() {
    try {
        auto engine = std::shared_ptr<EngineImpl>(new EngineImpl());

        ThrowIfNull(engine, "createEngineFailed");
        ThrowIfNot(engine->initialize(), "initializeFailed");

        return engine;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return nullptr;
    }
}

EngineImpl::~EngineImpl() {
    // shutdown the engine if initialized
    if (m_initialized) {
        shutdown();
    }
}

bool EngineImpl::initialize() {
    try {
        AACE_INFO(LX(TAG).d("engineVersion", aace::engine::core::version::getEngineVersion()));
#ifndef NO_SIGPIPE
        AACE_VERBOSE(LX(TAG).d("signal", "SIGPIPE").d("value", "SIG_IGN"));
        ThrowIf(std::signal(SIGPIPE, SIG_IGN) == SIG_ERR, "setSignalFailed");
#endif

        ThrowIf(m_initialized, "engineAlreadyInitialized");
        ThrowIfNot(checkServices(), "checkServicesFailed");

        // iterate through registered engine services and call initialize() for each module
        for (auto next : m_orderedServiceList) {
            ThrowIfNot(next->handleInitializeEngineEvent(shared_from_this()), "handleInitializeEngineEventFailed");
        }

        // get a reference to the message broker service interface
        auto messageBrokerService =
            getServiceInterface<aace::engine::messageBroker::MessageBrokerServiceInterface>("aace.messageBroker");
        ThrowIfNull(messageBrokerService, "invalidMessageBrokerServiceInterface");
        m_messageBrokerService = messageBrokerService;

        // set initialize flag
        m_initialized = true;

        ThrowIfNot(registerProperties(), "registerPropertiesFailed");
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool EngineImpl::registerProperties() {
    try {
        // get the property engine service interface from the property manager service
        auto propertyManager =
            getServiceInterface<aace::engine::propertyManager::PropertyManagerServiceInterface>("aace.propertyManager");
        ThrowIfNull(propertyManager, "nullPropertyManagerServiceInterface");

        propertyManager->registerProperty(aace::engine::propertyManager::PropertyDescription(
            aace::core::property::VERSION, nullptr, std::bind(&EngineImpl::getProperty_version, this)));
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool EngineImpl::shutdown() {
    try {
        AACE_DEBUG(LX(TAG).m("EngineShutdown"));
        if (m_initialized == false) {
            AACE_WARN(LX(TAG).m("Attempting to shutdown engine that is not initialized - doing nothing."));
            return true;
        }

        // engine must be stopped before shutdown, but continue with shutdown if failed...
        if (m_running == true && stop() == false) {
            AACE_ERROR(LX(TAG).d("reason", "stopEngineFailed"));
        }
        m_shutdownDuration.withName(METRIC_LIFECYCLE_LATENCY_KEY).startTimer();

        // Shut down MetricsEngineService last so we can record duration metric
        // for rest of Engine shutdown
        std::shared_ptr<EngineService> metricService = nullptr;
        for (auto serviceItr = m_orderedServiceList.begin(); serviceItr != m_orderedServiceList.end();) {
            if ((*serviceItr)->getDescription().getType() == "aace.metrics") {
                metricService = *serviceItr;
                m_orderedServiceList.erase(serviceItr);
                break;
            } else {
                serviceItr++;
            }
        }
        if (metricService == nullptr) {
            AACE_WARN(LX(TAG).m("Could not find MetricsEngineService in map"));
        }

        // iterate through registered engine services and call shutdown() for each module
        for (auto next : m_orderedServiceList) {
            AACE_DEBUG(LX(TAG).m(next->getDescription().getType()));

            // if shutting down the service failed throw an error but continue with
            // shutting down remaining services
            if (next->handleShutdownEngineEvent() == false) {
                AACE_ERROR(LX(TAG)
                               .d("reason", "handleShutdownEngineEventFailed")
                               .d("service", next->getDescription().getType()));
            }
        }

        // reset the engine state
        m_orderedServiceList.clear();
        m_registeredServiceMap.clear();
        m_initialized = false;
        m_configured = false;

        m_shutdownDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        if (metricRecorder == nullptr) {
            AACE_WARN(LX(TAG).m("Cannot record shutdown latency metric. Metric recorder weak_ptr expired"));
            return true;
        }
        submitLifecycleMetric(metricRecorder, m_shutdownDuration.build(), METRIC_LIFECYCLE_STAGE_SHUTDOWN, true);
        if (metricService != nullptr) {
            AACE_DEBUG(LX(TAG).m("Shutting down MetricsEngineService after recording shutdown latency"));
            metricService->handleShutdownEngineEvent();
        }
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        m_shutdownDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        ReturnIf(metricRecorder == nullptr, false);
        submitLifecycleMetric(metricRecorder, m_shutdownDuration.build(), METRIC_LIFECYCLE_STAGE_SHUTDOWN, false);
        return false;
    }
}

bool EngineImpl::configure(std::shared_ptr<aace::core::config::EngineConfiguration> configuration) {
    try {
        ThrowIfNull(configuration, "invalidConfiguration");
        return configure({configuration});
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool EngineImpl::configure(
    std::initializer_list<std::shared_ptr<aace::core::config::EngineConfiguration>> configurationList) {
    return configure(std::vector<std::shared_ptr<aace::core::config::EngineConfiguration>>(configurationList));
}

bool EngineImpl::configure(std::vector<std::shared_ptr<aace::core::config::EngineConfiguration>> configurationList) {
    try {
        AACE_DEBUG(LX(TAG).m("EngineConfigure"));
        m_configureDuration.withName(METRIC_LIFECYCLE_LATENCY_KEY).startTimer();

        ThrowIfNot(m_initialized, "engineNotInitialized");
        ThrowIf(m_running, "engineRunning");
        ThrowIf(m_configured, "engineAlreadyConfigured");
        ThrowIf(configurationList.empty(), "invalidConfigurationList");

        // create the merged configuration data
        json::Value mergedConfiguration = {};

        // iterate through configuration objects and get streams for sdk initialization and
        // merge all configuration stream together before calling service config methods
        for (auto nextStream : configurationList) {
            ThrowIfNull(nextStream, "invalidConfigurationStream");

            // parse the next configuration stream
            auto nextConfig = json::toJson(nextStream->getStream());

            // merge the document with the main configuration
            ThrowIfNot(json::isType(nextConfig, json::Type::object), "invalidConfigurationStream");
            ThrowIfNot(aace::engine::utils::json::merge(mergedConfiguration, nextConfig), "mergeConfigurationFailed");
        }

        // iterate through registered engine services and call configure() for each module
        if (mergedConfiguration.is_null() == false) {
            for (auto nextService : m_orderedServiceList) {
                auto serviceConfig =
                    json::get(mergedConfiguration, nextService->getDescription().getType(), json::Type::object);
                ThrowIfNot(
                    nextService->handleConfigureEngineEvent(
                        serviceConfig != nullptr ? json::toStream(serviceConfig) : nullptr),
                    "Service failed to configure: " + nextService->getDescription().getType());
            }
        } else {
            AACE_ERROR(LX(TAG).m("nullMergedConfiguration"));
        }

        // get a reference to the metrics service interface
        auto metricsService =
            getServiceInterface<aace::engine::metrics::MetricRecorderServiceInterface>("aace.metrics");
        ThrowIfNull(metricsService, "invalidMetricRecorderServiceInterface");
        m_metricRecorder = metricsService;

        m_configured = true;

        // iterate through registered engine modules and call handlePreRegisterEngineEvent() for each module
        for (auto next : m_orderedServiceList) {
            ThrowIfNot(next->handlePreRegisterEngineEvent(), "handlePreRegisterEngineEvent");
        }

        m_configureDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        ThrowIfNull(metricRecorder, "Metric recorder weak_ptr expired");
        submitLifecycleMetric(metricRecorder, m_configureDuration.build(), METRIC_LIFECYCLE_STAGE_CONFIG, true);
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        m_configureDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        ReturnIf(metricRecorder == nullptr, false);
        submitLifecycleMetric(metricRecorder, m_configureDuration.build(), METRIC_LIFECYCLE_STAGE_CONFIG, false);
        return false;
    }
}

bool EngineImpl::checkServices() {
    try {
        std::unordered_map<std::string, std::shared_ptr<ServiceFactory>> registeredServiceFactoryMap;
        std::vector<std::shared_ptr<ServiceFactory>> orderedServiceFactoryList;
        std::forward_list<std::shared_ptr<ServiceFactory>> unresolvedDependencyList;

        for (auto it = EngineServiceManager::registryBegin(); it != EngineServiceManager::registryEnd(); it++) {
            bool success = true;
            auto serviceFactory = it->second;
            auto desc = serviceFactory->getDescription();

            for (auto& next : desc.getDependencies()) {
                auto it = registeredServiceFactoryMap.find(next.getType());

                if (it != registeredServiceFactoryMap.end()) {
                    auto serviceFactory = it->second;
                    auto desc = serviceFactory->getDescription();
                    auto v1 = desc.getVersion();
                    auto v2 = next.getVersion();

                    ThrowIf(v1 < v2, "invalidDependencyVersion");
                } else {
                    success = false;
                    break;
                }
            }

            if (success) {
                orderedServiceFactoryList.push_back(serviceFactory);

                // add the service to the registered service map so we can resolve dependencies
                registeredServiceFactoryMap[desc.getType()] = serviceFactory;
            } else {
                unresolvedDependencyList.push_front(serviceFactory);
            }
        }

        while (unresolvedDependencyList.empty() == false) {
            bool updated = false;
            auto it = unresolvedDependencyList.begin();
            auto prev = unresolvedDependencyList.before_begin();

            while (it != unresolvedDependencyList.end()) {
                bool success = true;
                auto serviceFactory = *it;
                auto desc = serviceFactory->getDescription();

                for (auto& next : desc.getDependencies()) {
                    auto it = registeredServiceFactoryMap.find(next.getType());

                    if (it != registeredServiceFactoryMap.end()) {
                        auto serviceFactory = it->second;
                        auto desc = serviceFactory->getDescription();
                        auto v1 = desc.getVersion();
                        auto v2 = next.getVersion();

                        ThrowIf(v1 < v2, "invalidDependencyVersion");
                    } else {
                        success = false;
                        break;
                    }
                }

                if (success) {
                    orderedServiceFactoryList.push_back(serviceFactory);

                    // add the service to the registered service map so we can resolve dependencies
                    registeredServiceFactoryMap[desc.getType()] = serviceFactory;

                    // remove the item from the list
                    it = unresolvedDependencyList.erase_after(prev);

                    updated = true;
                } else {
                    prev = it++;
                }
            }

            // fail if we were not able to resolve any of the remaining dependencies
            ThrowIfNot(updated, "failedToResolveServiceDependencies");
        }

        // instantiate the engine service objects
        for (auto next : orderedServiceFactoryList) {
            auto service = next->newInstance();
            ThrowIfNull(service, "createNewServiceInstanceFailed");

            m_orderedServiceList.push_back(service);
            m_registeredServiceMap[service->getDescription().getType()] = service;
        }

        // dump list of services to log
        for (auto next : m_orderedServiceList) {
            auto desc = next->getDescription();
            auto version = desc.getVersion();
            AACE_INFO(LX(TAG).m(desc.getType()).d("v", version.toString()));
        }

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        m_orderedServiceList.clear();
        m_registeredServiceMap.clear();
        return false;
    }
}

bool EngineImpl::start() {
    try {
        AACE_DEBUG(LX(TAG).m("EngineStart"));
        m_startDuration.withName(METRIC_LIFECYCLE_LATENCY_KEY).startTimer();

        ThrowIf(m_running, "engineAlreadyRunning");
        ThrowIfNot(m_initialized, "engineNotInitialized");
        ThrowIfNot(m_configured, "engineNotConfigured");

        // postRegister and setup are called for each service the first time the engine is started
        if (m_setup == false) {
            // iterate through registered engine modules and call handlePostRegisterEngineEvent() for each module
            for (auto next : m_orderedServiceList) {
                ThrowIfNot(next->handlePostRegisterEngineEvent(), "handlePostRegisterEngineEvent");
            }

            // iterate through registered engine modules and call handleSetupEngineEvent() for each module
            for (auto next : m_orderedServiceList) {
                ThrowIfNot(next->handleSetupEngineEvent(), "handleSetupEngineEventFailed");
            }

            // set the engine setup flag to true
            m_setup = true;
        }

        // iterate through registered engine modules and call handleStartEngineEvent() for each module
        for (auto next : m_orderedServiceList) {
            ThrowIfNot(next->handleStartEngineEvent(), "handleStartEngineEventFailed");
        }

        // iterate through registered engine services and call handleEngineStartedEngineEvent() for each service
        for (auto next : m_orderedServiceList) {
            ThrowIfNot(next->handleEngineStartedEngineEvent(), "handleEngineStartedEngineEventFailed");
        }

        // set the engine running flag to true
        m_running = true;

        m_startDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        ThrowIfNull(metricRecorder, "Metric recorder weak_ptr expired");
        submitLifecycleMetric(metricRecorder, m_startDuration.build(), METRIC_LIFECYCLE_STAGE_START, true);
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        m_startDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        ReturnIf(metricRecorder == nullptr, false);
        submitLifecycleMetric(metricRecorder, m_startDuration.build(), METRIC_LIFECYCLE_STAGE_START, false);
        return false;
    }
}

bool EngineImpl::stop() {
    try {
        AACE_DEBUG(LX(TAG).m("EngineStop"));

        if (m_running == false) {
            AACE_WARN(LX(TAG).m("Attempting to stop engine that is not running - doing nothing."));
            return true;
        }
        m_stopDuration.withName(METRIC_LIFECYCLE_LATENCY_KEY).startTimer();

        // iterate through registered engine modules and call stop() for each module
        for (auto next : m_orderedServiceList) {
            ThrowIfNot(next->handleStopEngineEvent(), "handleStopEngineEventFailed");
        }

        // iterate through registered engine services and call engineStopped() for each service
        for (auto next : m_orderedServiceList) {
            ThrowIfNot(next->handleEngineStoppedEngineEvent(), "handleEngineStoppedEngineEventFailed");
        }

        // set the engine running and configured flag to false - the engine must be reconfigured before starting again
        m_running = false;

        m_stopDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        ThrowIfNull(metricRecorder, "Metric recorder weak_ptr expired");
        submitLifecycleMetric(metricRecorder, m_stopDuration.build(), METRIC_LIFECYCLE_STAGE_STOP, true);
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        m_stopDuration.stopTimer();
        auto metricRecorder = m_metricRecorder.lock();
        ReturnIf(metricRecorder == nullptr, false);
        submitLifecycleMetric(metricRecorder, m_stopDuration.build(), METRIC_LIFECYCLE_STAGE_STOP, false);
        return false;
    }
}

std::shared_ptr<EngineServiceContext> EngineImpl::getService(const std::string& type) {
    auto it = m_registeredServiceMap.find(type);
    return it != m_registeredServiceMap.end() ? std::make_shared<EngineServiceContext>(it->second) : nullptr;
}

std::shared_ptr<EngineService> EngineImpl::getServiceFromPropertyKey(const std::string& key) {
    for (auto next : m_orderedServiceList) {
        std::string kcmp = next->getDescription().getType() + ".";

        if (key.compare(0, kcmp.length(), kcmp) == 0) {
            return next;
        }
    }

    return nullptr;
}

std::string EngineImpl::getProperty_version() {
    try {
        AACE_INFO(LX(TAG));
        return aace::engine::core::version::getEngineVersion().toString();
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return "";
    }
}

bool EngineImpl::registerPlatformInterface(std::shared_ptr<aace::core::PlatformInterface> platformInterface) {
    try {
        ThrowIfNot(m_configured, "engineNotConfigured");
        ThrowIf(m_setup, "engineHasAlreadyBeenStarted");
        ThrowIfNull(platformInterface, "invalidPlatformInterface");

        // iterate through registered engine modules and call registerPlatformInterface() for each module
        for (auto next : m_orderedServiceList) {
            ReturnIf(next->handleRegisterPlatformInterfaceEngineEvent(platformInterface), true);
        }

        Throw("platformInterfaceNotRegistered");
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool EngineImpl::registerPlatformInterface(
    std::initializer_list<std::shared_ptr<aace::core::PlatformInterface>> platformInterfaceList) {
    try {
        ThrowIf(platformInterfaceList.size() == 0, "invalidPlatformInterfaceList");

        for (auto next : platformInterfaceList) {
            ThrowIfNot(registerPlatformInterface(next), "registerPlatformInterfaceFailed");
        }

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

std::shared_ptr<aace::core::MessageBroker> EngineImpl::getMessageBroker() {
    return shared_from_this();
}

void EngineImpl::publish(const std::string& message) {
    try {
        auto messageBrokerService = m_messageBrokerService.lock();
        ThrowIfNull(messageBrokerService, "invalidMessageBrokerService");
        auto messageBroker = messageBrokerService->getMessageBroker();
        ThrowIfNull(messageBroker, "invalidMessageBroker");
        messageBroker->publish(message, aace::engine::messageBroker::Message::Direction::INCOMING).send();
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
    }
}

void EngineImpl::subscribe(MessageHandler handler, const std::string& topic, const std::string& action) {
    try {
        auto messageBrokerService = m_messageBrokerService.lock();
        ThrowIfNull(messageBrokerService, "invalidMessageBrokerService");
        auto messageBroker = messageBrokerService->getMessageBroker();
        ThrowIfNull(messageBroker, "invalidMessageBroker");

        auto cb = handler;

        messageBroker->subscribe(
            topic,
            action,
            [cb](const aace::engine::messageBroker::Message& message) { cb(message.str()); },
            aace::engine::messageBroker::Message::Direction::OUTGOING);
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
    }
}

std::shared_ptr<aace::core::MessageStream> EngineImpl::openStream(
    const std::string& streamId,
    aace::core::MessageStream::Mode mode) {
    try {
        auto messageBrokerService = m_messageBrokerService.lock();
        ThrowIfNull(messageBrokerService, "invalidMessageBrokerService");
        auto streamManager = messageBrokerService->getStreamManager();
        ThrowIfNull(streamManager, "invalidStreamManager");
        return streamManager->requestStreamHandler(streamId, mode);
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return nullptr;
    }
}

}  // namespace core
}  // namespace engine
}  // namespace aace

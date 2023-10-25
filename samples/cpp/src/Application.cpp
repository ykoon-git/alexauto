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

#include "SampleApp/Application.h"
#include "SampleApp/Extension.h"

// C++ Standard Library
#ifdef __linux__
#include <linux/limits.h>  // PATH_MAX
#endif
#include <algorithm>  // std::find, std::for_each
#include <csignal>    // std::signal and SIG_ERR macro
#include <cstdlib>    // EXIT_SUCCESS and EXIT_FAILURE macros, std::atexit
// https://llvm.org/docs/CodingStandards.html#include-iostream-is-forbidden
#include <iostream>  // std::clog and std::cout
#include <fstream>
//
#include <iomanip>  // std::setw
#include <regex>    // std::regex
#include <set>      // std::set
#include <sstream>  // std::stringstream
#include <vector>   // std::vector

// Guidelines Support Library
#include <gsl/gsl-lite.hpp>

// JSON for Modern C++
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace sampleApp {

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Application
//
////////////////////////////////////////////////////////////////////////////////////////////////////

using Level = logger::LoggerHandler::Level;

Application::Application() = default;

#ifdef MONITORAIRPLANEMODEEVENTS
// Monitors airplane mode events and sends a network status changed event when the airplane mode changes
void monitorAirplaneModeEvents(
    std::shared_ptr<Activity> activity,
    std::shared_ptr<logger::LoggerHandler> loggerHandler) {
    // # Tested on Linux Ubuntu 16.04 LTS
    // $ rfkill event
    // 1567307554.400006: idx 1 type 1 op 0 soft 0 hard 0
    // 1567307554.400183: idx 2 type 2 op 0 soft 0 hard 0
    // 1567307558.915117: idx 1 type 1 op 2 soft 1 hard 0
    // 1567307558.924296: idx 2 type 2 op 2 soft 1 hard 0
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("/usr/sbin/rfkill event", "r"), pclose);
    if (!pipe || !pipe.get()) {
        loggerHandler->log(Level::ERROR, "monitorAirplaneModeEvents", "popen() failed");
        if (auto console = activity->findViewById("id:console").lock()) {
            console->printLine("popen() failed");
        }
        return;
    }
    auto currentNetworkStatus = network::NetworkInfoProviderHandler::NetworkStatus::UNKNOWN;
    std::array<char, 1024> buffer{};
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != NULL) {
        std::string value = buffer.data();
        auto hard = (std::string::npos != value.find("hard 1"));
        auto soft = (std::string::npos != value.find("soft 1"));
        auto nextworkStatus = (soft || hard) ? network::NetworkInfoProviderHandler::NetworkStatus::DISCONNECTED
                                             : network::NetworkInfoProviderHandler::NetworkStatus::CONNECTED;
        // Send a network status changed event if the airplane mode changed
        if (currentNetworkStatus != nextworkStatus) {
            currentNetworkStatus = nextworkStatus;
            std::stringstream stream;
            stream << nextworkStatus;
            value = stream.str();
            loggerHandler->log(Level::VERBOSE, "monitorAirplaneModeEvents", value);
            activity->notify(Event::onNetworkInfoProviderNetworkStatusChanged, value);
        }
    }
}
#endif  // MONITORAIRPLANEMODEEVENTS

// Parses config files so that handlers can check that required config is passed in
void parseConfigurations(const std::vector<std::string>& configFilePaths, std::vector<json>& configs) {
    for (auto const& path : configFilePaths) {
        try {
            std::ifstream ifs(path);
            json j = json::parse(ifs);
            configs.push_back(j);
        } catch (std::exception& e) {
        }
    }
}

void Application::printMenu(
    std::shared_ptr<ApplicationContext> applicationContext,
    std::shared_ptr<aace::core::Engine> engine,
    std::shared_ptr<sampleApp::propertyManager::PropertyManagerHandler> propertyManagerHandler,
    std::shared_ptr<View> console,
    const std::string& id) {
    // ACTIONS:
    //   AudioFile
    //   GoBack
    //   GoTo
    //   Help
    //   Login
    //   Logout
    //   Quit
    //   Restart
    //   Select
    //   SetLocale
    //   SetTimeZone
    //   SetLoggerLevel
    //   SetProperty
    //   notify/*
    // FORMAT:
    //   id <string>
    //   index <integer>
    //   item <array>
    //       do <string>
    //       key <string>
    //       name <string>
    //       note <string>
    //       value <any>
    //   name <string>
    //   path <string>
    //   text <object>
    auto menu = applicationContext->getMenu(id);
    Ensures(menu != nullptr);
    static const unsigned menuColumns = 80;
    auto titleRuler = std::string(menuColumns, '#');
    auto titleSpacer = '#' + std::string(menuColumns - 2, ' ') + '#';
    auto title = menu.count("name") ? menu.at("name").get<std::string>() : id;
    Ensures(menuColumns - 2 >= title.length());
    int balance = menuColumns - 2 - title.length();
    int left = balance > 1 ? balance / 2 : 0;
    int right = balance > 1 ? balance / 2 + balance % 2 : 0;
    std::stringstream stream;
    stream << std::endl
           << titleRuler << std::endl
           << titleSpacer << std::endl
           << '#' << std::string(left, ' ') << title << std::string(right, ' ') << '#' << std::endl
           << titleSpacer << std::endl
           << titleRuler << std::endl
           << std::endl;
    if (menu.count("item")) {
        unsigned keyMax = 0;
        for (auto& item : menu.at("item")) {
            if (!testMenuItem(applicationContext, item)) {
                continue;
            }
            auto key = item.at("key").get<std::string>();  // required item.key
            auto keyLength = key.length();
            if (keyMax < keyLength) {
                keyMax = keyLength;
            }
        }
        unsigned index = 0;
        for (auto& item : menu.at("item")) {
            if (!testMenuItem(applicationContext, item)) {
                continue;
            }
            auto action = item.at("do").get<std::string>();  // required item.do
            auto key = item.at("key").get<std::string>();    // required item.key
            auto name = item.at("name").get<std::string>();  // required item.name
            auto keyLength = key.length();
            auto underline = false;
            if (action == "Select") {
                underline = (menu.count("index") && menu.at("index").get<unsigned>() == index);
            } else if (action == "SetLocale") {
                auto locale = propertyManagerHandler->getProperty("aace.alexa.setting.locale");
                underline = (item.count("value") && item.at("value").get<std::string>() == locale);
            } else if (action == "SetTimeZone") {
                auto timezone = propertyManagerHandler->getProperty("aace.alexa.timezone");
                underline = (item.count("value") && item.at("value").get<std::string>() == timezone);
            } else if (action == "SetLoggerLevel") {
                if (applicationContext->isLogEnabled()) {
                    auto level = applicationContext->getLevel();
                    std::stringstream ss;
                    // Note: Fix stream issue
                    // ss << level;
                    switch (level) {
                        case Level::VERBOSE:
                            ss << "VERBOSE";
                            break;
                        case Level::INFO:
                            ss << "INFO";
                            break;
                        case Level::METRIC:
                            ss << "METRIC";
                            break;
                        case Level::WARN:
                            ss << "WARN";
                            break;
                        case Level::ERROR:
                            ss << "ERROR";
                            break;
                        case Level::CRITICAL:
                            ss << "CRITICAL";
                            break;
                    }
                    //
                    underline = (item.count("value") && item.at("value").get<std::string>() == ss.str());
                }
            } else if (action == "SetProperty") {
                auto value = item.at("value").get<std::string>();  // required item.value
                static std::regex r("^([^/]+)/(.+)", std::regex::optimize);
                std::smatch sm{};
                if (std::regex_match(value, sm, r) || ((sm.size() - 1) == 2)) {
                    underline = (propertyManagerHandler->getProperty(sm[1]) == sm[2]);
                }
            }
            if (underline) {
                stream << " [ " + key + " ]  " << std::string(keyMax - keyLength, ' ') << "\e[4m" << name << "\e[0m"
                       << std::endl;
            } else {
                stream << " [ " + key + " ]  " << std::string(keyMax - keyLength, ' ') << name << std::endl;
            }
            index++;
        }
    }
    stream << std::endl << titleRuler << std::endl;
    if (menu.count("path")) {
        auto menuFilePath = menu.at("path").get<std::string>();
        auto sdkVersion = propertyManagerHandler->getProperty(aace::core::property::VERSION);
        auto buildIdentifier = applicationContext->getBuildIdentifier();
        std::string string;
        if (!buildIdentifier.empty()) {
            string = menuFilePath + " (v" + sdkVersion + "-" + buildIdentifier + ")";
        } else {
            string = menuFilePath + " (v" + sdkVersion + ")";
        }
        int balance = menuColumns - 2 - string.length();
        int left = balance > 1 ? balance / 2 : 0;
        stream << std::string(left, ' ') << string << std::endl;
    }
    console->printLine(stream.str());
}

void Application::printMenuText(
    std::shared_ptr<ApplicationContext> applicationContext,
    std::shared_ptr<View> console,
    const std::string& menuId,
    const std::string& textId,
    std::map<std::string, std::string> variables) {
    auto menu = applicationContext->getMenu(menuId);
    if ((menu != nullptr) && menu.count("text")) {
        auto text = menu.at("text");
        if (text.is_object() && text.count(textId)) {
            text = text.at(textId);
        }
        if (text.is_primitive()) {
            text = json::array({text});
        }
        if (text.is_array()) {
            for (auto& item : text) {
                printStringLine(console, item.get<std::string>(), variables);
            }
        }
    }
}

void Application::printStringLine(
    std::shared_ptr<View> console,
    const std::string& string,
    std::map<std::string, std::string> variables) {
    auto s = string;
    static std::regex r("\\$\\{([a-zA-Z]+)\\}", std::regex::optimize);
    std::smatch sm{};
    std::stringstream stream;
    while (std::regex_search(s, sm, r)) {
        stream << sm.prefix();
        stream << variables[sm[1]];
        s = sm.suffix();
    }
    stream << s << std::endl;
    console->print(stream.str());
}

Status Application::run(std::shared_ptr<ApplicationContext> applicationContext) {
    std::mutex mutex;
    std::condition_variable conditionVariable;
    std::atomic<bool> connected{false};
    std::atomic<bool> processed{false};

    // Prepare the UI views
    std::vector<std::shared_ptr<View>> views{};

    // Create the application console view
    auto console = View::create("id:console");
    views.push_back(console);

    // Create the application card view
#ifdef AAC_ALEXA
    auto card = ContentView::create("id:card");
    views.push_back(card);

    // AlexaClientHandler view example
    views.push_back(TextView::create("id:AuthState"));
    views.push_back(TextView::create("id:ConnectionStatus"));
    views.push_back(TextView::create("id:DialogState"));

    // DoNotDisturbHandler view example
    views.push_back(TextView::create("id:DoNotDisturbState"));

    // NotificationsHandler view example
    views.push_back(TextView::create("id:IndicatorState"));
#endif

    // Create the activity object
    auto activity = Activity::create(applicationContext, views);
    Ensures(activity != nullptr);

    // Special case for test automation
    if (applicationContext->isTestAutomation()) {
        activity->registerObserver(Event::onTestAutomationConnect, [&](const std::string&) {
            connected = true;
            activity->notify(Event::onTestAutomationProcess);
            return true;
        });
        activity->registerObserver(Event::onTestAutomationProcess, [&](const std::string&) {
            if (connected) {
                auto audioFilePath = applicationContext->popAudioFilePath();
                if (!audioFilePath.empty()) {
                    console->printLine("Process:", audioFilePath);
                    if (activity->notify(Event::onSpeechRecognizerStartStreamingAudioFile, audioFilePath)) {
                        return activity->notify(Event::onSpeechRecognizerTapToTalk);
                    }
                    return false;
                }
                processed = true;
                conditionVariable.notify_one();
            }
            return false;
        });
    }

    // Create the engine object
    auto engine = aace::core::Engine::create();
    Ensures(engine != nullptr);

    // Logger (Important: Logger must be created before the other handlers)
    auto loggerHandler = logger::LoggerHandler::create(activity);
    Ensures(loggerHandler != nullptr);

    // Create configuration files for --config files path passed from the command line
    std::vector<std::shared_ptr<aace::core::config::EngineConfiguration>> configurationFiles;

    auto configFilePaths = applicationContext->getConfigFilePaths();
    for (auto& configFilePath : configFilePaths) {
        auto configurationFile = aace::core::config::ConfigurationFile::create(configFilePath);
        Ensures(configurationFile != nullptr);
        configurationFiles.push_back(configurationFile);
    }

    // Validate that configuration files are passed in
    std::vector<json> jsonConfigs;
    parseConfigurations(configFilePaths, jsonConfigs);

#ifdef AAC_CAR_CONTROL
    // ------------------------------------------------------------------------
    // In a production environment we recommend that the application builds
    // the car control configuration programatically. However, the configuration
    // can also be passed in to the application. This example builds a
    // configuration programatically if one is not passed in to the application.
    // ------------------------------------------------------------------------
    if (!carControl::CarControlHandler::checkConfiguration(jsonConfigs)) {
        console->printRuler();
        console->printLine("Car control configuration was created");
        console->printRuler();
        auto carControlConfig = carControl::CarControlDataProvider::generateCarControlConfig();
        configurationFiles.push_back(carControlConfig);
    } else {
        console->printRuler();
        console->printLine("Car control configuration found");
        console->printRuler();
    }
    // Initialize values for car control configuration controllers
    carControl::CarControlDataProvider::initialize(configurationFiles);
#endif

    // Validate extensions
    if (!extension::Manager::validateExtensions(jsonConfigs, console)) {
        console->printRuler();
        console->printLine("Auto SDK extensions failed validation");
        console->printRuler();

        // Shutdown
        if (!engine->shutdown()) {
            console->printRuler();
            console->printLine("Error: Engine could not be shutdown");
            console->printRuler();
        }

        return Status::Failure;
    }

    // Configure the engine
    auto configured = engine->configure(configurationFiles);
    if (!configured) {
        // Note: not logging anything here as loggerHandler is not available yet
        console->printLine("Error: could not be configured");
        if (!engine->shutdown()) {
            console->printLine("Error: could not be shutdown");
        }
        return Status::Failure;
    }

    // message broker for AASB messages
    auto messageBroker = engine->getMessageBroker();

    // Important: Logger, Audio IO Providers, Location Provider, and Network Info Provider are
    // core module features and must be created and registered before the other handlers.

    // Property Manager
    auto propertyManagerHandler = propertyManager::PropertyManagerHandler::create(loggerHandler, messageBroker);
    Ensures(propertyManagerHandler != nullptr);

#ifndef AAC_SYSTEM_AUDIO
    // Default Audio (Important: Audio implementation must be created before the other handlers)
    auto defaultAudioInputProvider =
        audio::AudioInputProviderHandler::create(activity, loggerHandler, messageBroker, false);
    Ensures(defaultAudioInputProvider != nullptr);
    defaultAudioInputProvider->setupUI();
    applicationContext->setAudioFileSupported(true);

    auto defaultAudioOutputProvider =
        audio::AudioOutputProviderHandler::create(activity, loggerHandler, messageBroker, false);
    Ensures(defaultAudioOutputProvider != nullptr);
    defaultAudioOutputProvider->setupUI();
#endif

    // Location Provider (Important: Location Provider must be created before the other handlers)
    auto locationProviderHandler = location::LocationProviderHandler::create(activity, loggerHandler, messageBroker);
    Ensures(locationProviderHandler != nullptr);

    // Network Info Provider (Important: Network Info Provider must be created before the other handlers)
    auto networkInfoProviderHandler =
        network::NetworkInfoProviderHandler::create(activity, loggerHandler, messageBroker);
    Ensures(networkInfoProviderHandler != nullptr);

    // Authorization
    auto authorizationHandler = authorization::AuthorizationHandler::create(activity, loggerHandler, messageBroker);
    Ensures(authorizationHandler != nullptr);
    authorizationHandler->saveDeviceInfo(jsonConfigs);

#ifdef AAC_VAD_ENABLE
    if (!arbitrator::AgentHandler::getPath(jsonConfigs)) {
        console->printRuler();
        console->printLine("Pryonlite configuration not found");
        console->printRuler();
        // Shutdown
        if (!engine->shutdown()) {
            console->printRuler();
            console->printLine("Error: Engine could not be shutdown");
            console->printRuler();
        }
        return Status::Failure;
    }
    auto agentHandler = arbitrator::AgentHandler::create(activity, loggerHandler, messageBroker);
    Ensures(agentHandler != nullptr);
#endif

#ifdef AAC_ALEXA
    // Audio Player
    auto audioPlayerHandler = alexa::AudioPlayerHandler::create(activity, loggerHandler, messageBroker);
    Ensures(audioPlayerHandler != nullptr);

    // Alexa Client
    auto alexaClientHandler = alexa::AlexaClientHandler::create(activity, loggerHandler, messageBroker);
    Ensures(alexaClientHandler != nullptr);

    // Do Not Disturb
    auto doNotDisturbHandler = alexa::DoNotDisturbHandler::create(activity, loggerHandler, messageBroker);
    Ensures(doNotDisturbHandler != nullptr);

    // Device Setup Handler
    auto deviceSetupHandler = alexa::DeviceSetupHandler::create(activity, loggerHandler, messageBroker);
    Ensures(deviceSetupHandler != nullptr);

    // Media Playback Requestor Handler
    auto mediaPlaybackRequestor = alexa::MediaPlaybackRequestorHandler::create(activity, loggerHandler, messageBroker);
    Ensures(mediaPlaybackRequestor != nullptr);

    // Equalizer Controller
    auto equalizerControllerHandler = alexa::EqualizerControllerHandler::create(activity, loggerHandler, messageBroker);
    Ensures(equalizerControllerHandler != nullptr);

    std::vector<
        std::pair<aasb::message::alexa::localMediaSource::Source, std::shared_ptr<alexa::LocalMediaSourceHandler>>>
        LocalMediaSources = {{aasb::message::alexa::localMediaSource::Source::BLUETOOTH, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::USB, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::FM_RADIO, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::AM_RADIO, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::SATELLITE_RADIO, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::LINE_IN, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::COMPACT_DISC, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::DAB, nullptr},
                             {aasb::message::alexa::localMediaSource::Source::DEFAULT, nullptr}};

    for (auto& source : LocalMediaSources) {
        source.second = alexa::LocalMediaSourceHandler::create(activity, loggerHandler, source.first, messageBroker);
        Ensures(source.second != nullptr);
    }

    // Notifications
    auto notificationsHandler = alexa::NotificationsHandler::create(activity, loggerHandler, messageBroker);
    Ensures(notificationsHandler != nullptr);

    // Playback Controller
    auto playbackControllerHandler = alexa::PlaybackControllerHandler::create(activity, loggerHandler, messageBroker);
    Ensures(playbackControllerHandler != nullptr);

    // Speech Recognizer
    // Note : Expects PropertyManager to be not null.
    auto speechRecognizerHandler =
        alexa::SpeechRecognizerHandler::create(activity, loggerHandler, propertyManagerHandler, messageBroker);
    Ensures(speechRecognizerHandler != nullptr);
    Ensures(propertyManagerHandler->setProperty(
        aace::alexa::property::WAKEWORD_ENABLED, applicationContext->isWakeWordSupported() ? "true" : "false"));

    // Speech Synthesizer
    auto speechSynthesizerHandler = alexa::SpeechSynthesizerHandler::create(activity, loggerHandler);
    Ensures(speechSynthesizerHandler != nullptr);

    // Template Runtime
    auto templateRuntimeHandler = alexa::TemplateRuntimeHandler::create(activity, loggerHandler, messageBroker);
    Ensures(templateRuntimeHandler != nullptr);

    // Alexa Speaker
    auto alexaSpeakerHandler = alexa::AlexaSpeakerHandler::create(activity, loggerHandler, messageBroker);
    Ensures(alexaSpeakerHandler != nullptr);

    // Feature Discovery
    auto featureDiscoveryHandler = alexa::FeatureDiscoveryHandler::create(activity, loggerHandler, messageBroker);
    Ensures(featureDiscoveryHandler != nullptr);
    
    // CaptionPresenter
    auto captionPresenterHandler = alexa::CaptionPresenterHandler::create(activity, loggerHandler, messageBroker);
    Ensures(captionPresenterHandler != nullptr);
#endif

    // Text To Speech Handler
#ifdef AAC_TEXT_TO_SPEECH
    auto textToSpeechHandler = textToSpeech::TextToSpeechHandler::create(activity, loggerHandler, messageBroker);
    Ensures(textToSpeechHandler != nullptr);
#endif

#ifdef AAC_CONNECTIVITY
    // Alexa Connectivity
    auto alexaConnectivityHandler =
        connectivity::AlexaConnectivityHandler::create(activity, loggerHandler, messageBroker);
    Ensures(alexaConnectivityHandler != nullptr);
#endif  // CONNECTIVITY

#ifdef AAC_MESSAGING
    // Messaging
    auto messagingHandler = messaging::MessagingHandler::create(activity, loggerHandler, messageBroker);
    Ensures(messagingHandler != nullptr);
#endif

#ifdef AAC_NAVIGATION
    // Navigation
    auto navigationHandler = navigation::NavigationHandler::create(activity, loggerHandler, messageBroker);
    Ensures(navigationHandler != nullptr);
#endif

#ifdef AAC_PHONE_CONTROL
    // Phone Call Controller
    auto phoneCallControllerHandler =
        phoneControl::PhoneCallControllerHandler::create(activity, loggerHandler, messageBroker);
    Ensures(phoneCallControllerHandler != nullptr);
#endif

#ifdef AAC_ADDRESS_BOOK
    auto addressBookHandler = addressBook::AddressBookHandler::create(activity, loggerHandler, messageBroker);
    Ensures(addressBookHandler != nullptr);
#endif

#ifdef AAC_CAR_CONTROL
    auto carControlHandler = carControl::CarControlHandler::create(activity, loggerHandler, messageBroker);
    Ensures(carControlHandler != nullptr);
#endif

    // Initialize extension handlers
    if (!extension::Manager::initializeExtensions(activity, loggerHandler, messageBroker)) {
        console->printRuler();
        console->printLine("Auto SDK extensions failed to initialize\n");
        console->printRuler();
        // Shutdown
        if (!engine->shutdown()) {
            console->printRuler();
            console->printLine("Error: Engine could not be shutdown");
            console->printRuler();
        }
        return Status::Failure;
    }

    // Start the engine
    if (engine->start()) {
        loggerHandler->log(Level::INFO, "Application:Engine", "started successfully");
    } else {
        loggerHandler->log(Level::INFO, "Application:Engine", "failed to start");
        console->printLine("Error: could not be started (check logs)");
        if (engine->shutdown()) {
            loggerHandler->log(Level::INFO, "Application:Engine", "shutdown successfully");
        } else {
            loggerHandler->log(Level::INFO, "Application:Engine", "failed to shutdown");
            console->printLine("Error: could not be shutdown (check logs)");
        }
        return Status::Failure;
    }

#ifdef MONITORAIRPLANEMODEEVENTS
    // Start a thread to monitor airplane mode events
    std::thread monitorAirplaneModeEventsThread(monitorAirplaneModeEvents, activity, loggerHandler);
    monitorAirplaneModeEventsThread.detach();
#endif  // MONITORAIRPLANEMODEEVENTS

#if defined(AAC_ALEXACOMMS) && defined(AAC_PHONE_CONTROL)
    // Workaround: Enable Phone Connection since it is needed by Alexa Comms. This limitation will go away in the future. Refer to
    // Alexa Comms README for more information.
    phoneCallControllerHandler->connectionStateChanged(
        aasb::message::phoneCallController::phoneCallController::ConnectionState::CONNECTED);
#endif  // ALEXACOMMS

    // Setup the interactive text based menu system
    setupMenu(applicationContext, engine, propertyManagerHandler, console);

    // Setup the SDK version number and print optional text for the main menu with variables
    auto VERSION = propertyManagerHandler->getProperty(aace::core::property::VERSION);
    // clang-format off
    std::map<std::string, std::string> variables{
        {"VERSION", VERSION}
    };
    // clang-format on
    console->printLine("Alexa Auto SDK", 'v' + VERSION);
    printMenuText(applicationContext, console, "main", "banner", variables);

    // Start the authorization if we were successfully authorized previously.
    // This can be disabled by supplying `disable-auto-authorization` command to sample app
    if (!applicationContext->isAutoAuthorizationDisabled()) {
        authorizationHandler->startAuth();
    }

    // Run the program
    auto status = Status::Success;
    if (applicationContext->isTestAutomation()) {
        std::unique_lock<std::mutex> lock(mutex);
        conditionVariable.wait(lock, [&processed] { return processed.load(); });
    } else {
        // Run the main loop (i.e. interactive text based menu system)
        auto id = std::string("main");
        if (applicationContext->hasMenu("user")) {
            // If not logged in, and user menu is available, run it instead of main
            id = std::string("user");
        }
        status = runMenu(applicationContext, engine, propertyManagerHandler, activity, console, id);
    }

    // Stop notifications
    activity->clearObservers();

    // Stop the engine
    if (engine->stop()) {
        loggerHandler->log(Level::INFO, "Application:Engine", "stopped successfully");
    } else {
        loggerHandler->log(Level::INFO, "Application:Engine", "failed to stop");
        console->printLine("Error: could not be stopped (check logs)");
    }

    // Shutdown the engine
    if (engine->shutdown()) {
        loggerHandler->log(Level::INFO, "Application:Engine", "shutdown successfully");
    } else {
        loggerHandler->log(Level::INFO, "Application:Engine", "failed to shutdown");
        console->printLine("Error: could not be shutdown (check logs)");
    }

    // Releases the ownership of the managed objects
    for (auto& configurationFile : configurationFiles) {
        configurationFile.reset();
    }

    // Print and return the application status
    console->printLine(status);
    return status;
}

Status Application::runMenu(
    std::shared_ptr<ApplicationContext> applicationContext,
    std::shared_ptr<aace::core::Engine> engine,
    std::shared_ptr<sampleApp::propertyManager::PropertyManagerHandler> propertyManagerHandler,
    std::shared_ptr<Activity> activity,
    std::shared_ptr<View> console,
    const std::string& id) {
    auto status = Status::Unknown;
    std::vector<std::string> stack{id};
    auto ptr = applicationContext->getMenuPtr(id);
    auto menuPtr = ptr;
    Ensures(menuPtr != nullptr);
    auto menuFilePath = menuPtr->at("path").get<std::string>();
    auto menuDirPath = applicationContext->getDirPath(menuFilePath);
    printMenu(applicationContext, engine, propertyManagerHandler, console, id);
    while (auto cin = fgetc(stdin)) {
        auto c = static_cast<unsigned char>(cin);
        static const unsigned char DELETE = 0x7F;
        static const unsigned char ENTER = '\n';
        static const unsigned char ESC = '\e';
        static const unsigned char HELP = '?';
        static const unsigned char QUIT = 'q';
        static const unsigned char STOP_ACTIVE_DOMAIN = 'x';
        static const unsigned char STOP_FOREGROUND_ACTIVITY = '!';
        static const unsigned char TALK = ' ';
        // clang-format off
        std::map<std::string, std::string> variables{
            {"KEYCLOSE", " ]"},
            {"KEYOPEN", "[ "}
        };
        // clang-format on
        unsigned char k = '\0';
        unsigned index = 0;
        // available on all menus
        switch (c) {
            case DELETE:  // nothing
                variables["KEY"] = "delete";
                break;
            case ENTER:  // nothing
                variables["KEY"] = "enter";
                break;
            case ESC:  // go back
                variables["KEY"] = "esc";
                break;
            case HELP:  // print help
                variables["KEY"] = std::string({static_cast<char>(HELP)});
                break;
            case QUIT:  // quit app
                variables["KEY"] = std::string({static_cast<char>(std::toupper(QUIT))});
                break;
            case STOP_ACTIVE_DOMAIN:  // exit the active domain
                variables["KEY"] = std::string({static_cast<char>(std::toupper(STOP_ACTIVE_DOMAIN))});
                break;
            case TALK:  // tap-to-talk convenience
                variables["KEY"] = "space";
                break;
            case STOP_FOREGROUND_ACTIVITY:
                variables["KEY"] = "!";
                break;
            default:
                variables["KEY"] = std::string({static_cast<char>(std::toupper(c))});
                // break range-based for loop
                for (auto& item : menuPtr->at("item")) {
                    auto key = item.at("key").get<std::string>();  // required item.key
                    if ((key == "delete") || (key == "DELETE")) {
                        k = DELETE;
                    } else if ((key == "enter") || (key == "ENTER")) {
                        k = ENTER;
                    } else if ((key == "esc") || (key == "ESC")) {
                        k = ESC;
                    } else {
                        k = key[0];
                    }
                    if (std::tolower(k) == std::tolower(c)) {
                        break;
                    }
                    index++;
                }
                break;
        }
        if (index == menuPtr->at("item").size()) {
            printMenuText(applicationContext, console, "main", "keyTapError", variables);
        } else {
            printMenuText(applicationContext, console, "main", "keyTapped", variables);
        }
        if (menuPtr->count("item")) {
            unsigned char k = '\0';
            unsigned index = 0;
            // break range-based for loop
            for (auto& item : menuPtr->at("item")) {
                if (!testMenuItem(applicationContext, item)) {
                    continue;
                }
                auto key = item.at("key").get<std::string>();  // required item.key
                if (key == "esc" || key == "ESC") {
                    k = ESC;
                } else {
                    k = key[0];
                }
                if (std::tolower(k) == std::tolower(c)) {
                    if (item.count("note")) {  // optional item.note
                        printStringLine(console, "Note: " + item.at("note").get<std::string>(), variables);
                    }
                    auto action = item.at("do").get<std::string>();  // required item.do
                    if (action.find("notify/") == 0) {
                        auto eventId = action.substr(7);
                        if (EventEnumerator.count(eventId)) {
                            auto event = EventEnumerator.at(eventId);
                            auto value = std::string{};
                            if (item.count("value")) {  // optional item.value
                                value = item.at("value").get<std::string>();
                            }
                            if (event == Event::onAddAddressBookPhone || event == Event::onAddAddressBookAuto ||
                                event == Event::onLoadNavigationState || event == Event::onConversationsReport) {
                                value = menuDirPath + '/' + value;
                            }
                            activity->notify(event, value);
                        } else {
                            console->printLine("Unknown eventId:", eventId);
                            status = Status::Failure;
                        }
                        break;
                    } else if (action == "AudioFile") {
                        Ensures(item.count("name") == 1);  // required item.name
                        auto name = item.at("name").get<std::string>();
                        Ensures(item.count("value") == 1);  // required item.value
                        auto value = item.at("value").get<std::string>();
                        console->printLine(name);
                        auto audioFilePath = menuDirPath + '/' + value;
                        if (activity->notify(Event::onSpeechRecognizerStartStreamingAudioFile, audioFilePath)) {
                            activity->notify(Event::onSpeechRecognizerTapToTalk);
                        }
                        break;
                    } else if (action == "GoBack") {
                        c = ESC;  // go back
                        break;
                    } else if (action == "GoTo") {
                        auto menuId = std::string{};
                        auto value = item.at("value");  // required item.value
                        if (value.is_object()) {
                            menuId = value.at("id").get<std::string>();  // required item.id
                        } else {
                            menuId = value.get<std::string>();
                        }
                        stack.push_back(menuId);
                        menuPtr = applicationContext->getMenuPtr(menuId);
                        if (menuPtr && menuPtr->is_object()) {
                            printMenu(applicationContext, engine, propertyManagerHandler, console, menuId);
                        } else {
                            console->printLine("Unknown menuId:", menuId);
                            status = Status::Failure;
                        }
                        break;
                    } else if (action == "Help") {
                        c = HELP;  // print help
                        break;
                    } else if (action == "Quit") {
                        c = QUIT;  // quit app
                        break;
                    } else if (action == "Restart") {
                        console->printLine("Are you sure you want to restart Y/n?");
                        if ('Y' == static_cast<unsigned char>(fgetc(stdin))) {
                            status = Status::Restart;
                        } else {
                            console->printLine("Aborted the restart");
                        }
                        break;
                    } else if (action == "Select") {
                        menuPtr->at("index") = index;
                        c = ESC;  // go back
                        break;
                    } else if (action == "SetLocale") {
                        auto value = item.at("value").get<std::string>();  // required item.value
                        propertyManagerHandler->setProperty("aace.alexa.setting.locale", value);
                        console->printLine("aace.alexa.setting.locale =", value);
                        c = ESC;  // go back
                        break;
                    } else if (action == "SetupCompleted") {
                        activity->notify(Event::onDeviceSetupCompleted);
                        break;
                    } else if (action == "MediaPlaybackRequested") {
                        activity->notify(Event::onMediaPlaybackRequested);
                        break;
                    } else if (action == "SetTimeZone") {
                        auto value = item.at("value").get<std::string>();  // required item.value
                        propertyManagerHandler->setProperty("aace.alexa.timezone", value);
                        console->printLine("aace.alexa.timezone =", value);
                        c = ESC;  // go back
                        break;
                    } else if (action == "SetLoggerLevel") {
                        // Note: Set level in logger handler (loggerHandler)
                        auto value = item.at("value").get<std::string>();  // required item.value
                        if (value == "VERBOSE") {
                            applicationContext->setLevel(Level::VERBOSE);
                        } else if (value == "INFO") {
                            applicationContext->setLevel(Level::INFO);
                        } else if (value == "METRIC") {
                            applicationContext->setLevel(Level::METRIC);
                        } else if (value == "WARN") {
                            applicationContext->setLevel(Level::WARN);
                        } else if (value == "ERROR") {
                            applicationContext->setLevel(Level::ERROR);
                        } else if (value == "CRITICAL") {
                            applicationContext->setLevel(Level::CRITICAL);
                        } else {
                            applicationContext->clearLevel();
                        }
                        c = ESC;  // go back
                        break;
                    } else if (action == "SetProperty") {
                        auto value = item.at("value").get<std::string>();  // required item.value
                        static std::regex r("^([^/]+)/(.+)", std::regex::optimize);
                        std::smatch sm{};
                        if (std::regex_match(value, sm, r) || ((sm.size() - 1) == 2)) {
                            propertyManagerHandler->setProperty(sm[1], sm[2]);
                            console->printLine(sm[1], "=", sm[2]);
                        }
                        c = ESC;  // go back
                        break;
                    } else {
                        console->printLine("Unknown action:", action);
                        status = Status::Failure;
                        break;
                    }
                }
                index++;
            }
            // available on all menus
            switch (c) {
                case ESC:  // go back
                    if (stack.size() > 1) {
                        stack.pop_back();
                        auto menuId = stack.back();
                        menuPtr = applicationContext->getMenuPtr(menuId);
                        printMenu(applicationContext, engine, propertyManagerHandler, console, menuId);
                    }
                    break;
                case HELP:  // print help
                    printMenu(applicationContext, engine, propertyManagerHandler, console, stack.back());
                    break;
                case QUIT:  // quit app
                    status = Status::Success;
                    break;
                case STOP_ACTIVE_DOMAIN:  // exit the active domain
                    activity->notify(Event::onStopActive);
                    break;
                case TALK:  // tap-to-talk convenience
                    activity->notify(Event::onSpeechRecognizerTapToTalk);
                    break;
                case STOP_FOREGROUND_ACTIVITY:
                    activity->notify(Event::onStopForegroundActivity);
                    break;
                default:
                    break;
            }
        }
        if (status != Status::Unknown) {
            break;
        }
    }
    return status;
}

void Application::setupMenu(
    std::shared_ptr<ApplicationContext> applicationContext,
    std::shared_ptr<aace::core::Engine> engine,
    std::shared_ptr<sampleApp::propertyManager::PropertyManagerHandler> propertyManagerHandler,
    std::shared_ptr<View> console) {
    // recursive menu registration
    std::function<std::string(
        std::shared_ptr<ApplicationContext>,
        std::shared_ptr<aace::core::Engine>,
        std::shared_ptr<sampleApp::propertyManager::PropertyManagerHandler>,
        std::shared_ptr<View>,
        json&,
        std::string&)>
        f;
    f = [&f](
            std::shared_ptr<ApplicationContext> applicationContext,
            std::shared_ptr<aace::core::Engine> engine,
            std::shared_ptr<sampleApp::propertyManager::PropertyManagerHandler> propertyManagerHandler,
            std::shared_ptr<View> console,
            json& menu,
            std::string& path) {
        menu["path"] = path;
        Ensures(menu.count("id") == 1);                      // required menu.id
        if (menu.at("id").get<std::string>() == "LOCALE") {  // reserved id: LOCALE
            auto item = json::array();
            /*
             * AVS-supported Locales:
             * https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/system.html#locales
             */
            std::string supportedLocales =
                "de-DE,en-AU,en-CA,en-GB,en-IN,en-US,es-ES,es-MX,es-US,fr-CA,fr-FR,hi-IN,it-IT,ja-JP,pt-BR,ar-SA,"
                "en-CA/fr-CA,en-IN/hi-IN,en-US/es-US,es-US/en-US,fr-CA/en-CA,hi-IN/en-IN,en-US/fr-FR,fr-FR/en-US,en-US/"
                "de-DE,de-DE/en-US,en-US/ja-JP,ja-JP/en-US,en-US/it-IT,it-IT/en-US,en-US/es-ES,es-ES/en-US";
            std::istringstream iss{supportedLocales};
            auto token = std::string();
            unsigned count = std::count(supportedLocales.begin(), supportedLocales.end(), ',') + 1;
            unsigned index = 0;
            while (std::getline(iss, token, ',')) {
                unsigned char k = '\0';
                if (count < 10) {
                    k = '1' + index;
                } else {  // Note: 'Q' conflict
                    k = 'A' + index;
                }
                auto key = std::string{static_cast<char>(k)};
                item.push_back({{"do", "SetLocale"}, {"key", key}, {"name", token}, {"value", token}});
                index++;
            }
            if (menu.count("item") && menu.at("item").is_array()) {  // optional menu.item array
                item.insert(std::end(item), std::begin(menu.at("item")), std::end(menu.at("item")));
            }
            menu.at("item") = item;
        } else if (menu.count("item") && menu.at("item").is_array()) {  // optional menu.item array
            for (auto& item : menu.at("item")) {
                if (item.count("do") && item.at("do") == "GoTo") {
                    if (item.count("value") && item.at("value").is_object()) {
                        item.at("value") =
                            f(applicationContext, engine, propertyManagerHandler, console, item.at("value"), path);
                    }
                }
            }
        }
        auto id = menu.at("id").get<std::string>();
        applicationContext->registerMenu(id, menu);
        return id;
    };
    auto paths = applicationContext->getMenuFilePaths();
    for (auto& path : paths) {
        // read a JSON file
        std::ifstream i(path);
        json menu;
        i >> menu;
        // // write prettified JSON to another file
        // std::ofstream o(path + ".json");
        // o << std::setw(4) << menu << std::endl;
        if (menu.is_object()) {
            menu = json::array({menu});
        }
        if (menu.is_array()) {
            for (auto& item : menu) {
                f(applicationContext, engine, propertyManagerHandler, console, item, path);
            }
        } else {
            console->printLine("Error: could not load menu", path);
        }
    }
}

bool Application::testMenuItem(std::shared_ptr<ApplicationContext> applicationContext, const json& item) {
    std::string name = item.count("test") ? "test" : item.count("when") ? "when" : "";
    if (!name.empty()) {
        auto value = item.at(name).get<std::string>();
        return applicationContext->testExpression(value);
    }
    return true;
}

}  // namespace sampleApp

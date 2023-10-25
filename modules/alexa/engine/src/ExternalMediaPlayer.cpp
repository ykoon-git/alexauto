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

/// @file ExternalMediaPlayer.cpp
#include <utility>
#include <vector>

#include "AACE/Engine/Alexa/ExternalMediaPlayer.h"
#include "AACE/Engine/Core/EngineMacros.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "AACE/Engine/Alexa/AdapterUtils.h"
#include <AVSCommon/AVS/SpeakerConstants/SpeakerConstants.h>
#include <AVSCommon/Utils/JSON/JSONUtils.h>
#include <AVSCommon/Utils/Memory/Memory.h>
#include <AVSCommon/Utils/String/StringUtils.h>

namespace aace {
namespace engine {
namespace alexa {

using namespace alexaClientSDK::avsCommon::avs;
// using namespace alexaClientSDK::avsCommon::avs::externalMediaPlayer;
using namespace alexaClientSDK::avsCommon::sdkInterfaces;
// using namespace alexaClientSDK::avsCommon::sdkInterfaces::externalMediaPlayer;
using namespace alexaClientSDK::avsCommon::avs::attachment;
using namespace alexaClientSDK::avsCommon::utils;
using namespace alexaClientSDK::avsCommon::sdkInterfaces;
using namespace alexaClientSDK::avsCommon::utils::json;
using namespace alexaClientSDK::avsCommon::utils::logger;
using namespace alexaClientSDK::avsCommon::utils::string;

/// String to identify log entries originating from this file.
static const std::string TAG("aace.alexa.ExternalMediaPlayer");

/**
 * Create a LogEntry using this file's TAG and the specified event string.
 *
 * @param The event string for this @c LogEntry.
 */
// #define LX(event) alexaClientSDK::alexaClientSDK::avsCommon::utils::logger::LogEntry(TAG, event)

// The namespaces used in the context.
static const std::string EXTERNALMEDIAPLAYER_STATE_NAMESPACE = "ExternalMediaPlayer";
static const std::string PLAYBACKSTATEREPORTER_STATE_NAMESPACE = "Alexa.PlaybackStateReporter";

// The names used in the context.
static const std::string EXTERNALMEDIAPLAYER_NAME = "ExternalMediaPlayerState";
static const std::string PLAYBACKSTATEREPORTER_NAME = "playbackState";

// The namespace for this capability agent.
static const std::string EXTERNALMEDIAPLAYER_NAMESPACE = "ExternalMediaPlayer";
static const std::string PLAYBACKCONTROLLER_NAMESPACE = "Alexa.PlaybackController";
static const std::string PLAYLISTCONTROLLER_NAMESPACE = "Alexa.PlaylistController";
static const std::string SEEKCONTROLLER_NAMESPACE = "Alexa.SeekController";
static const std::string FAVORITESCONTROLLER_NAMESPACE = "Alexa.FavoritesController";

// Capability constants
/// The AlexaInterface constant type.
static const std::string ALEXA_INTERFACE_TYPE = "AlexaInterface";

/// ExternalMediaPlayer capability constants
/// ExternalMediaPlayer interface type
static const std::string EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_TYPE = ALEXA_INTERFACE_TYPE;
/// ExternalMediaPlayer interface name
static const std::string EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_NAME = "ExternalMediaPlayer";
/// ExternalMediaPlayer interface version
static const std::string EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_VERSION = "1.1";

/// Alexa.PlaybackStateReporter name.
static const std::string PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_NAME = PLAYBACKSTATEREPORTER_STATE_NAMESPACE;
/// Alexa.PlaybackStateReporter version.
static const std::string PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.PlaybackController name.
static const std::string PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_NAME = PLAYBACKCONTROLLER_NAMESPACE;
/// Alexa.PlaybackController version.
static const std::string PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.PlaylistController name.
static const std::string PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_NAME = PLAYLISTCONTROLLER_NAMESPACE;
/// Alexa.PlaylistController version.
static const std::string PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.SeekController name.
static const std::string SEEKCONTROLLER_CAPABILITY_INTERFACE_NAME = SEEKCONTROLLER_NAMESPACE;
/// Alexa.SeekController version.
static const std::string SEEKCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.FavoritesController name.
static const std::string FAVORITESCONTROLLER_CAPABILITY_INTERFACE_NAME = FAVORITESCONTROLLER_NAMESPACE;
/// Alexa.FavoritesController version.
static const std::string FAVORITESCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";

//#ifdef EXTERNALMEDIAPLAYER_1_1
/// The name of the @c FocusManager channel used by @c ExternalMediaPlayer and
/// its Adapters.
static const std::string CHANNEL_NAME =
    alexaClientSDK::avsCommon::sdkInterfaces::FocusManagerInterface::CONTENT_CHANNEL_NAME;

/**
 * The activityId string used with @c FocusManager by @c ExternalMediaPlayer.
 * (as per spec for AVS for monitoring channel activity.)
 */
static const std::string FOCUS_MANAGER_ACTIVITY_ID = "ExternalMediaPlayer";

/// The duration to wait for a state change in @c onFocusChanged before failing.
static const std::chrono::milliseconds TIMEOUT{500};
//#endif

// The @c External media player play directive signature.
static const NamespaceAndName PLAY_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE, "Play", AgentId::AGENT_ID_ALL};
static const NamespaceAndName LOGIN_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE, "Login", AgentId::AGENT_ID_ALL};
static const NamespaceAndName LOGOUT_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE, "Logout", AgentId::AGENT_ID_ALL};
static const NamespaceAndName AUTHORIZEDISCOVEREDPLAYERS_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE,
                                                                   "AuthorizeDiscoveredPlayers",
                                                                   AgentId::AGENT_ID_ALL};

// The @c Transport control directive signatures.
static const NamespaceAndName RESUME_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Play", AgentId::AGENT_ID_ALL};
static const NamespaceAndName PAUSE_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Pause", AgentId::AGENT_ID_ALL};
static const NamespaceAndName STOP_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Stop", AgentId::AGENT_ID_ALL};
static const NamespaceAndName NEXT_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Next", AgentId::AGENT_ID_ALL};
static const NamespaceAndName PREVIOUS_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Previous", AgentId::AGENT_ID_ALL};
static const NamespaceAndName STARTOVER_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "StartOver", AgentId::AGENT_ID_ALL};
static const NamespaceAndName REWIND_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Rewind", AgentId::AGENT_ID_ALL};
static const NamespaceAndName FASTFORWARD_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "FastForward", AgentId::AGENT_ID_ALL};

// The @c PlayList control directive signature.
static const NamespaceAndName ENABLEREPEATONE_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE,
                                                        "EnableRepeatOne",
                                                        AgentId::AGENT_ID_ALL};
static const NamespaceAndName ENABLEREPEAT_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE,
                                                     "EnableRepeat",
                                                     AgentId::AGENT_ID_ALL};
static const NamespaceAndName DISABLEREPEAT_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE,
                                                      "DisableRepeat",
                                                      AgentId::AGENT_ID_ALL};
static const NamespaceAndName ENABLESHUFFLE_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE,
                                                      "EnableShuffle",
                                                      AgentId::AGENT_ID_ALL};
static const NamespaceAndName DISABLESHUFFLE_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE,
                                                       "DisableShuffle",
                                                       AgentId::AGENT_ID_ALL};

// The @c Seek control directive signature.
static const NamespaceAndName SEEK_DIRECTIVE{SEEKCONTROLLER_NAMESPACE, "SetSeekPosition", AgentId::AGENT_ID_ALL};
static const NamespaceAndName ADJUSTSEEK_DIRECTIVE{SEEKCONTROLLER_NAMESPACE,
                                                   "AdjustSeekPosition",
                                                   AgentId::AGENT_ID_ALL};

// The @c favorites control directive signature.
static const NamespaceAndName FAVORITE_DIRECTIVE{FAVORITESCONTROLLER_NAMESPACE, "Favorite", AgentId::AGENT_ID_ALL};
static const NamespaceAndName UNFAVORITE_DIRECTIVE{FAVORITESCONTROLLER_NAMESPACE, "Unfavorite", AgentId::AGENT_ID_ALL};

// The @c ExternalMediaPlayer context state signatures.
static const NamespaceAndName SESSION_STATE{EXTERNALMEDIAPLAYER_STATE_NAMESPACE,
                                            EXTERNALMEDIAPLAYER_NAME,
                                            AgentId::AGENT_ID_ALL};
static const NamespaceAndName PLAYBACK_STATE{PLAYBACKSTATEREPORTER_STATE_NAMESPACE,
                                             PLAYBACKSTATEREPORTER_NAME,
                                             AgentId::AGENT_ID_ALL};

/// The const char for the players key field in the context.
static const char PLAYERS[] = "players";

/// The const char for the playerInFocus key field in the context.
static const char PLAYER_IN_FOCUS[] = "playerInFocus";

/// The max relative time in the past that we can  seek to in milliseconds(-12hours in ms).
static const int64_t MAX_PAST_OFFSET = -86400000;

/// The max relative time in the past that we can  seek to in milliseconds(+12 hours in ms).
static const int64_t MAX_FUTURE_OFFSET = 86400000;

/// The agent key.
static const char AGENT_KEY[] = "agent";

/// The authorized key.
static const char AUTHORIZED[] = "authorized";

/// The localPlayerId key.
static const char LOCAL_PLAYER_ID[] = "localPlayerId";

/// The metadata key.
static const char METADATA[] = "metadata";

/// The playerId key.
static const char PLAYER_ID_KEY[] = "playerId";

/// The skillToken key.
static const char SKILL_TOKEN_KEY[] = "skillToken";

/// The spiVersion key.
static const char SPI_VERSION_KEY[] = "spiVersion";

/**
 * Creates the ExternalMediaPlayer capability configuration.
 *
 * @return The ExternalMediaPlayer capability configuration.
 */
static std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityConfiguration>
getExternalMediaPlayerCapabilityConfiguration();

/// The @c m_directiveToHandlerMap Map of the directives to their handlers.
std::unordered_map<NamespaceAndName, std::pair<RequestType, ExternalMediaPlayer::DirectiveHandler>>
    ExternalMediaPlayer::m_directiveToHandlerMap = {
        {AUTHORIZEDISCOVEREDPLAYERS_DIRECTIVE,
         std::make_pair(RequestType::NONE, &ExternalMediaPlayer::handleAuthorizeDiscoveredPlayers)},
        {LOGIN_DIRECTIVE, std::make_pair(RequestType::LOGIN, &ExternalMediaPlayer::handleLogin)},
        {LOGOUT_DIRECTIVE, std::make_pair(RequestType::LOGOUT, &ExternalMediaPlayer::handleLogout)},
        {PLAY_DIRECTIVE, std::make_pair(RequestType::PLAY, &ExternalMediaPlayer::handlePlay)},
        {PAUSE_DIRECTIVE, std::make_pair(RequestType::PAUSE, &ExternalMediaPlayer::handlePlayControl)},
        {STOP_DIRECTIVE, std::make_pair(RequestType::STOP, &ExternalMediaPlayer::handlePlayControl)},
        {RESUME_DIRECTIVE, std::make_pair(RequestType::RESUME, &ExternalMediaPlayer::handlePlayControl)},
        {NEXT_DIRECTIVE, std::make_pair(RequestType::NEXT, &ExternalMediaPlayer::handlePlayControl)},
        {PREVIOUS_DIRECTIVE, std::make_pair(RequestType::PREVIOUS, &ExternalMediaPlayer::handlePlayControl)},
        {STARTOVER_DIRECTIVE, std::make_pair(RequestType::START_OVER, &ExternalMediaPlayer::handlePlayControl)},
        {FASTFORWARD_DIRECTIVE, std::make_pair(RequestType::FAST_FORWARD, &ExternalMediaPlayer::handlePlayControl)},
        {REWIND_DIRECTIVE, std::make_pair(RequestType::REWIND, &ExternalMediaPlayer::handlePlayControl)},
        {ENABLEREPEATONE_DIRECTIVE,
         std::make_pair(RequestType::ENABLE_REPEAT_ONE, &ExternalMediaPlayer::handlePlayControl)},
        {ENABLEREPEAT_DIRECTIVE, std::make_pair(RequestType::ENABLE_REPEAT, &ExternalMediaPlayer::handlePlayControl)},
        {DISABLEREPEAT_DIRECTIVE, std::make_pair(RequestType::DISABLE_REPEAT, &ExternalMediaPlayer::handlePlayControl)},
        {ENABLESHUFFLE_DIRECTIVE, std::make_pair(RequestType::ENABLE_SHUFFLE, &ExternalMediaPlayer::handlePlayControl)},
        {DISABLESHUFFLE_DIRECTIVE,
         std::make_pair(RequestType::DISABLE_SHUFFLE, &ExternalMediaPlayer::handlePlayControl)},
        {FAVORITE_DIRECTIVE, std::make_pair(RequestType::FAVORITE, &ExternalMediaPlayer::handlePlayControl)},
        {UNFAVORITE_DIRECTIVE, std::make_pair(RequestType::UNFAVORITE, &ExternalMediaPlayer::handlePlayControl)},
        {SEEK_DIRECTIVE, std::make_pair(RequestType::SEEK, &ExternalMediaPlayer::handleSeek)},
        {ADJUSTSEEK_DIRECTIVE, std::make_pair(RequestType::ADJUST_SEEK, &ExternalMediaPlayer::handleAdjustSeek)}};
// TODO: ARC-227 Verify default values
auto audioNonBlockingPolicy = BlockingPolicy(BlockingPolicy::MEDIUM_AUDIO, false);
auto neitherNonBlockingPolicy = BlockingPolicy(BlockingPolicy::MEDIUMS_NONE, false);

static DirectiveHandlerConfiguration g_configuration = {{AUTHORIZEDISCOVEREDPLAYERS_DIRECTIVE, audioNonBlockingPolicy},
                                                        {PLAY_DIRECTIVE, audioNonBlockingPolicy},
                                                        {LOGIN_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {LOGOUT_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {RESUME_DIRECTIVE, audioNonBlockingPolicy},
                                                        {PAUSE_DIRECTIVE, audioNonBlockingPolicy},
                                                        {STOP_DIRECTIVE, audioNonBlockingPolicy},
                                                        {NEXT_DIRECTIVE, audioNonBlockingPolicy},
                                                        {PREVIOUS_DIRECTIVE, audioNonBlockingPolicy},
                                                        {STARTOVER_DIRECTIVE, audioNonBlockingPolicy},
                                                        {REWIND_DIRECTIVE, audioNonBlockingPolicy},
                                                        {FASTFORWARD_DIRECTIVE, audioNonBlockingPolicy},
                                                        {ENABLEREPEATONE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {ENABLEREPEAT_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {DISABLEREPEAT_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {ENABLESHUFFLE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {DISABLESHUFFLE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {SEEK_DIRECTIVE, audioNonBlockingPolicy},
                                                        {ADJUSTSEEK_DIRECTIVE, audioNonBlockingPolicy},
                                                        {FAVORITE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {UNFAVORITE_DIRECTIVE, neitherNonBlockingPolicy}};

static std::unordered_map<PlaybackButton, RequestType> g_buttonToRequestType = {
    // adapter handlers
    // Important Note: This changes default AVS Device SDK behavior.
    {PlaybackButton::PLAY, RequestType::RESUME},
    {PlaybackButton::PAUSE, RequestType::PAUSE},
    //     {PlaybackButton::PLAY, RequestType::PAUSE_RESUME_TOGGLE},
    //     {PlaybackButton::PAUSE, RequestType::PAUSE_RESUME_TOGGLE},
    {PlaybackButton::NEXT, RequestType::NEXT},
    {PlaybackButton::PREVIOUS, RequestType::PREVIOUS}};

static std::unordered_map<PlaybackToggle, std::pair<RequestType, RequestType>> g_toggleToRequestType = {
    {PlaybackToggle::SHUFFLE, std::make_pair(RequestType::ENABLE_SHUFFLE, RequestType::DISABLE_SHUFFLE)},
    {PlaybackToggle::LOOP, std::make_pair(RequestType::ENABLE_REPEAT, RequestType::DISABLE_REPEAT)},
    {PlaybackToggle::REPEAT, std::make_pair(RequestType::ENABLE_REPEAT_ONE, RequestType::DISABLE_REPEAT_ONE)},
    {PlaybackToggle::THUMBS_UP, std::make_pair(RequestType::FAVORITE, RequestType::DESELECT_FAVORITE)},
    {PlaybackToggle::THUMBS_DOWN, std::make_pair(RequestType::UNFAVORITE, RequestType::DESELECT_UNFAVORITE)}};

/**
 * Generate a @c CapabilityConfiguration object.
 *
 * @param type The Capability interface type.
 * @param interfaceName The Capability interface name.
 * @param version The Capability interface version.
 */
static std::shared_ptr<CapabilityConfiguration> generateCapabilityConfiguration(
    const std::string& type,
    const std::string& interfaceName,
    const std::string& version) {
    std::unordered_map<std::string, std::string> configMap;
    configMap.insert({CAPABILITY_INTERFACE_TYPE_KEY, type});
    configMap.insert({CAPABILITY_INTERFACE_NAME_KEY, interfaceName});
    configMap.insert({CAPABILITY_INTERFACE_VERSION_KEY, version});

    return std::make_shared<CapabilityConfiguration>(configMap);
}

std::shared_ptr<ExternalMediaPlayer> ExternalMediaPlayer::create(
    const std::string& agentString,
    std::shared_ptr<SpeakerManagerInterface> speakerManager,
    std::shared_ptr<MessageSenderInterface> messageSender,
    std::shared_ptr<alexaClientSDK::certifiedSender::CertifiedSender> certifiedMessageSender,
    std::shared_ptr<FocusManagerInterface> focusManager,
    std::shared_ptr<ContextManagerInterface> contextManager,
    std::shared_ptr<ExceptionEncounteredSenderInterface> exceptionSender,
    std::shared_ptr<PlaybackRouterInterface> playbackRouter,
    std::shared_ptr<aace::engine::alexa::ExternalMediaAdapterRegistrationInterface> externalMediaAdapterRegistration) {
    if (nullptr == speakerManager) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "nullSpeakerManager"));
        return nullptr;
    }

    if (nullptr == messageSender) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "nullMessageSender"));
        return nullptr;
    }
    if (nullptr == certifiedMessageSender) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "nullCertifiedMessageSender"));
        return nullptr;
    }
    if (nullptr == focusManager) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "nullFocusManager"));
        return nullptr;
    }
    if (nullptr == contextManager) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "nullContextManager"));
        return nullptr;
    }
    if (nullptr == exceptionSender) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "nullExceptionSender"));
        return nullptr;
    }
    if (nullptr == playbackRouter) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "nullPlaybackRouter"));
        return nullptr;
    }

    auto externalMediaPlayer = std::shared_ptr<ExternalMediaPlayer>(new ExternalMediaPlayer(
        agentString,
        speakerManager,
        messageSender,
        certifiedMessageSender,
        contextManager,
        exceptionSender,
        playbackRouter,
        externalMediaAdapterRegistration));

    if (!externalMediaPlayer->init(focusManager)) {
        AACE_ERROR(LX(TAG, "createFailed").d("reason", "initFailed"));
        return nullptr;
    }

    // adapter handlers
    externalMediaPlayer->m_focusManager = focusManager;

    return externalMediaPlayer;
}

ExternalMediaPlayer::ExternalMediaPlayer(
    const std::string& agentString,
    std::shared_ptr<SpeakerManagerInterface> speakerManager,
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
    std::shared_ptr<alexaClientSDK::certifiedSender::CertifiedSender> certifiedMessageSender,
    std::shared_ptr<ContextManagerInterface> contextManager,
    std::shared_ptr<ExceptionEncounteredSenderInterface> exceptionSender,
    std::shared_ptr<PlaybackRouterInterface> playbackRouter,
    std::shared_ptr<aace::engine::alexa::ExternalMediaAdapterRegistrationInterface> externalMediaAdapterRegistration) :
        CapabilityAgent{EXTERNALMEDIAPLAYER_NAMESPACE, exceptionSender},
        RequiresShutdown{"ExternalMediaPlayer"},
        m_agentString{agentString},
        m_speakerManager{speakerManager},
        m_messageSender{messageSender},
        m_certifiedMessageSender{certifiedMessageSender},
        m_contextManager{contextManager},
        m_playbackRouter{playbackRouter},
        m_externalMediaAdapterRegistration{externalMediaAdapterRegistration},
        m_focus{FocusState::NONE},
        m_focusAcquireInProgress{false},
        m_haltInitiator{HaltInitiator::NONE},
        m_ignoreExternalPauseCheck{false},
        m_currentActivity{alexaClientSDK::avsCommon::avs::PlayerActivity::IDLE},
        m_mixingBehavior{alexaClientSDK::avsCommon::avs::MixingBehavior::UNDEFINED} {
    // Register all supported capabilities.
    m_capabilityConfigurations.insert(getExternalMediaPlayerCapabilityConfiguration());

    // Register all supported capabilities.
    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_NAME,
        PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_NAME,
        PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_NAME,
        PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE, SEEKCONTROLLER_CAPABILITY_INTERFACE_NAME, SEEKCONTROLLER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        FAVORITESCONTROLLER_CAPABILITY_INTERFACE_NAME,
        FAVORITESCONTROLLER_CAPABILITY_INTERFACE_VERSION));
}

bool ExternalMediaPlayer::init(std::shared_ptr<FocusManagerInterface> focusManager) {
    AACE_VERBOSE(LX(TAG));

    m_authorizedSender = AuthorizedSender::create(m_messageSender);
    if (!m_authorizedSender) {
        AACE_ERROR(LX(TAG, "initFailed").d("reason", "createAuthorizedSenderFailed"));
        return false;
    }

    m_contextManager->setStateProvider(SESSION_STATE, shared_from_this());
    m_contextManager->setStateProvider(PLAYBACK_STATE, shared_from_this());

    return true;
}

std::shared_ptr<CapabilityConfiguration> getExternalMediaPlayerCapabilityConfiguration() {
    return generateCapabilityConfiguration(
        EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_TYPE,
        EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_NAME,
        EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_VERSION);
}

// adapter handlers
void ExternalMediaPlayer::addAdapterHandler(
    std::shared_ptr<aace::engine::alexa::ExternalMediaAdapterHandlerInterface> adapterHandler) {
    AACE_VERBOSE(LX(TAG));
    if (!adapterHandler) {
        AACE_ERROR(LX(TAG, "addAdapterHandler").m("Adapter handler is null."));
        return;
    }
    m_executor.submit([this, adapterHandler]() {
        AACE_VERBOSE(LX(TAG, "addAdapterHandlerInExecutor"));
        if (!m_adapterHandlers.insert(adapterHandler).second) {
            AACE_ERROR(LX(TAG, "addAdapterHandlerInExecutor").m("Duplicate adapter handler."));
        }
    });
}

void ExternalMediaPlayer::removeAdapterHandler(
    std::shared_ptr<aace::engine::alexa::ExternalMediaAdapterHandlerInterface> adapterHandler) {
    AACE_VERBOSE(LX(TAG));
    if (!adapterHandler) {
        AACE_ERROR(LX(TAG, "removeAdapterHandler").m("Adapter handler is null."));
        return;
    }
    m_executor.submit([this, adapterHandler]() {
        AACE_VERBOSE(LX(TAG).m("removeAdapterHandlerInExecutor"));
        if (m_adapterHandlers.erase(adapterHandler) == 0) {
            AACE_WARN(LX(TAG, "removeAdapterHandlerInExecutor").m("Nonexistent adapter handler."));
        }
    });
}

void ExternalMediaPlayer::executeOnFocusChanged(aace::engine::alexa::FocusState newFocus, MixingBehavior behavior) {
    AACE_DEBUG(LX(TAG)
                   .d("from", m_focus)
                   .d("to", newFocus)
                   .d("m_currentActivity", m_currentActivity)
                   .d("m_haltInitiator", m_haltInitiator)
                   .d("m_playerInFocus", m_playerInFocus));
    if (m_focus == newFocus && (m_mixingBehavior == behavior)) {
        m_focusAcquireInProgress = false;
        return;
    }
    m_focus = newFocus;
    m_focusAcquireInProgress = false;
    {
        switch (newFocus) {
            case FocusState::FOREGROUND: {
                /*
                    * If the system is currently in a pause initiated from AVS, on focus change
                    * to FOREGROUND do not try to resume. This happens when a user calls
                    * "Alexa, pause" while Spotify is PLAYING. This moves the adapter to
                    * BACKGROUND focus. AVS then sends a PAUSE request and after calling the
                    * ESDK pause when the adapter switches to FOREGROUND focus we do not want
                    * the adapter to start PLAYING.
                    */
                if (m_haltInitiator == HaltInitiator::EXTERNAL_PAUSE) {
                    return;
                }

                switch (m_currentActivity) {
                    case PlayerActivity::IDLE:
                    case PlayerActivity::STOPPED:
                    case PlayerActivity::FINISHED:
                        return;
                    case PlayerActivity::PAUSED: {
                        // reset flag
                        if (m_ignoreExternalPauseCheck) m_ignoreExternalPauseCheck = false;

                        // At this point a request to play another artist on Spotify may have already
                        // been processed (or is being processed) and we do not want to send resume here.
                        if (m_haltInitiator == HaltInitiator::FOCUS_CHANGE_PAUSE) {
                            for (auto adapterHandler : m_adapterHandlers) {
                                adapterHandler->playControl(m_playerInFocus, RequestType::RESUME);
                                // A focus change to foreground when paused means we should resume the current song.
                                AACE_DEBUG(LX(TAG).d("action", "resumeExternalMediaPlayer"));
                                setCurrentActivity(alexaClientSDK::avsCommon::avs::PlayerActivity::PLAYING);
                            }
                        }
                    }
                        return;
                    case PlayerActivity::PLAYING:
                    case PlayerActivity::BUFFER_UNDERRUN:
                        // We should already have foreground focus in these states; break out to the warning below.
                        break;
                }
                break;
            }
            case FocusState::BACKGROUND:
                switch (m_currentActivity) {
                    case PlayerActivity::STOPPED:
                    // We can also end up here with an empty queue if we've asked MediaPlayer to play, but playback
                    // hasn't started yet, so we fall through to call @c pause() here as well.
                    case PlayerActivity::FINISHED:
                    case PlayerActivity::IDLE:
                    // Note: can be in FINISHED or IDLE while waiting for MediaPlayer to start playing, so we fall
                    // through to call @c pause() here as well.
                    case PlayerActivity::PAUSED:
                    // Note: can be in PAUSED while we're trying to resume, in which case we still want to pause, so we
                    // fall through to call @c pause() here as well.
                    case PlayerActivity::PLAYING:
                    case PlayerActivity::BUFFER_UNDERRUN: {
                        if (behavior == MixingBehavior::MAY_DUCK) {
                            AACE_DEBUG(LX(TAG).d("behavior", "MAY_DUCK"));
                            return;
                        }
                        for (auto adapterHandler : m_adapterHandlers) {
                            // check against currently known playback state, not already paused
                            auto adapterStates = adapterHandler->getAdapterStates();
                            for (auto adapterState : adapterStates) {
                                if (adapterState.sessionState.playerId.compare(m_playerInFocus) == 0 &&
                                    !m_ignoreExternalPauseCheck) {  // match playerId
                                    std::string playbackStateString = adapterState.playbackState.state;
                                    if (playbackStateString.compare(playerActivityToString(PlayerActivity::IDLE)) !=
                                            0 &&
                                        playbackStateString.compare(playerActivityToString(PlayerActivity::PAUSED)) !=
                                            0 &&
                                        playbackStateString.compare(playerActivityToString(PlayerActivity::STOPPED)) !=
                                            0) {
                                        // only send pause if currently playing
                                        adapterHandler->playControl(m_playerInFocus, RequestType::PAUSE);
                                        // If we get pushed into the background while playing or buffering, pause the current song.
                                        AACE_DEBUG(LX(TAG).d("action", "pauseExternalMediaPlayer"));
                                        m_haltInitiator = HaltInitiator::FOCUS_CHANGE_PAUSE;
                                    } else
                                        m_haltInitiator = HaltInitiator::
                                            EXTERNAL_PAUSE;  // Player was not playing, assume external pause
                                }
                            }
                        }
                        //update activity state
                        setCurrentActivity(alexaClientSDK::avsCommon::avs::PlayerActivity::PAUSED);
                    }
                        return;
                }
                break;
            case FocusState::NONE:
                switch (m_currentActivity) {
                    case PlayerActivity::IDLE:
                    case PlayerActivity::STOPPED:
                    case PlayerActivity::FINISHED:
                        // Nothing to more to do if we're already not playing; we got here because the act of stopping
                        // caused the channel to be released, which in turn caused this callback.
                        return;
                    case PlayerActivity::PLAYING:
                    case PlayerActivity::PAUSED:
                    case PlayerActivity::BUFFER_UNDERRUN:
                        // If the focus change came in while we were in a 'playing' state, we need to stop the player because we are
                        // yielding the channel.
                        AACE_DEBUG(LX(TAG).d("action", "stopExternalMediaPlayer"));
                        std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
                        for (auto adapterHandler : m_adapterHandlers) {
                            adapterHandler->playControl(m_playerInFocus, RequestType::STOP);
                        }
                        m_playerInFocus = "";
                        m_haltInitiator = HaltInitiator::FOCUS_CHANGE_STOP;
                        setCurrentActivity(alexaClientSDK::avsCommon::avs::PlayerActivity::STOPPED);
                        return;
                }
                break;
        }
    }
}

void ExternalMediaPlayer::onFocusChanged(FocusState newFocus, MixingBehavior behavior) {
    AACE_DEBUG(LX(TAG).d("newFocus", newFocus).d("MixingBehavior", behavior));
    m_executor.submit([this, newFocus, behavior] { executeOnFocusChanged(newFocus, behavior); });

    switch (newFocus) {
        case FocusState::FOREGROUND:
            // Could wait for playback to actually start, but there's no real benefit to waiting, and long delays in
            // buffering could result in timeouts, so returning immediately for this case.
            return;
        case FocusState::BACKGROUND: {
            //Ideally expecting to see a transition to PAUSED, but in terms of user-observable changes, a move to any
            //of PAUSED/STOPPED/FINISHED will indicate that it's safe for another channel to move to the foreground.

            auto predicate = [this] {
                switch (m_currentActivity) {
                    case PlayerActivity::IDLE:
                    case PlayerActivity::PAUSED:
                    case PlayerActivity::STOPPED:
                    case PlayerActivity::FINISHED:
                        return true;
                    case PlayerActivity::PLAYING:
                    case PlayerActivity::BUFFER_UNDERRUN:
                        return false;
                }
                AACE_ERROR(LX(TAG, "onFocusChangedFailed")
                               .d("reason", "unexpectedActivity")
                               .d("m_currentActivity", m_currentActivity));
                return false;
            };
            std::unique_lock<std::mutex> lock(m_currentActivityMutex);
            if (!m_currentActivityConditionVariable.wait_for(lock, TIMEOUT, predicate)) {
                AACE_ERROR(LX(TAG, "onFocusChangedTimedOut")
                               .d("newFocus", newFocus)
                               .d("m_currentActivity", m_currentActivity));
            }
        }
            return;
        case FocusState::NONE: {
            //Need to wait for STOPPED or FINISHED, indicating that we have completely ended playback.
            auto predicate = [this] {
                switch (m_currentActivity) {
                    case PlayerActivity::IDLE:
                    case PlayerActivity::STOPPED:
                    case PlayerActivity::FINISHED:
                        return true;
                    case PlayerActivity::PLAYING:
                    case PlayerActivity::PAUSED:
                    case PlayerActivity::BUFFER_UNDERRUN:
                        return false;
                }
                AACE_ERROR(LX(TAG, "onFocusChangedFailed")
                               .d("reason", "unexpectedActivity")
                               .d("m_currentActivity", m_currentActivity));
                return false;
            };
            std::unique_lock<std::mutex> lock(m_currentActivityMutex);
            if (!m_currentActivityConditionVariable.wait_for(lock, TIMEOUT, predicate)) {
                AACE_ERROR(LX(TAG, "onFocusChangedFailed")
                               .d("reason", "activityChangeTimedOut")
                               .d("newFocus", newFocus)
                               .d("m_currentActivity", m_currentActivity));
            }

            m_executor.submit([this]() {
                std::unique_lock<std::mutex> lock(m_currentActivityMutex);
                if (m_currentActivity == PlayerActivity::STOPPED) {
                    std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
                    m_playerInFocus = "";
                }
            });
        }
            return;
    }
    AACE_ERROR(LX(TAG, "onFocusChangedFailed").d("reason", "unexpectedFocusState").d("newFocus", newFocus));
}
// end adapter handler code

void ExternalMediaPlayer::onContextAvailable(const std::string& jsonContext) {
    // Send Message happens on the calling thread. Do not block the ContextManager thread.
    m_executor.submit([this, jsonContext] {
        AACE_VERBOSE(LX(TAG, "onContextAvailableInExecutor"));

        while (!m_eventQueue.empty()) {
            std::pair<std::string, std::string> nameAndPayload = m_eventQueue.front();
            m_eventQueue.pop();
            auto event = buildJsonEventString(nameAndPayload.first, "", nameAndPayload.second, jsonContext);

            AACE_VERBOSE(LX(TAG).d("event", event.second));
            auto request = std::make_shared<MessageRequest>(AgentId::AGENT_ID_ALL, event.second);
            m_messageSender->sendMessage(request);
        }
    });
}

void ExternalMediaPlayer::onContextFailure(const alexaClientSDK::avsCommon::sdkInterfaces::ContextRequestError error) {
    std::pair<std::string, std::string> nameAndPayload = m_eventQueue.front();
    m_eventQueue.pop();
    AACE_ERROR(LX(TAG, __func__)
                   .d("error", error)
                   .d("eventName", nameAndPayload.first)
                   .sensitive("payload", nameAndPayload.second));
}

void ExternalMediaPlayer::provideState(
    const alexaClientSDK::avsCommon::avs::NamespaceAndName& stateProviderName,
    unsigned int stateRequestToken) {
    m_executor.submit([this, stateProviderName, stateRequestToken] {
        executeProvideState(stateProviderName, true, stateRequestToken);
    });
}

void ExternalMediaPlayer::handleDirectiveImmediately(std::shared_ptr<AVSDirective> directive) {
    handleDirective(std::make_shared<DirectiveInfo>(directive, nullptr));
}

void ExternalMediaPlayer::preHandleDirective(std::shared_ptr<DirectiveInfo> info) {
}

bool ExternalMediaPlayer::parseDirectivePayload(std::shared_ptr<DirectiveInfo> info, rapidjson::Document* document) {
    rapidjson::ParseResult result = document->Parse(info->directive->getPayload().c_str());

    if (result) {
        return true;
    }

    AACE_ERROR(LX(TAG, "parseDirectivePayloadFailed")
                   .d("reason", rapidjson::GetParseError_En(result.Code()))
                   .d("offset", result.Offset())
                   .d("messageId", info->directive->getMessageId()));

    sendExceptionEncounteredAndReportFailed(
        info, "Unable to parse payload", ExceptionErrorType::UNEXPECTED_INFORMATION_RECEIVED);

    return false;
}

void ExternalMediaPlayer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
    if (!info) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullDirectiveInfo"));
        return;
    }

    NamespaceAndName directiveNamespaceAndName(
        info->directive->getNamespace(), info->directive->getName(), AgentId::AGENT_ID_ALL);
    auto handlerIt = m_directiveToHandlerMap.find(directiveNamespaceAndName);
    if (handlerIt == m_directiveToHandlerMap.end()) {
        AACE_ERROR(LX(TAG, "handleDirectivesFailed")
                       .d("reason", "noDirectiveHandlerForDirective")
                       .d("nameSpace", info->directive->getNamespace())
                       .d("name", info->directive->getName()));
        sendExceptionEncounteredAndReportFailed(
            info, "Unhandled directive", ExceptionErrorType::UNEXPECTED_INFORMATION_RECEIVED);
        return;
    }

    AACE_VERBOSE(LX(TAG)
                     .d("nameSpace", info->directive->getNamespace())
                     .d("name", info->directive->getName())
                     .d("payload", info->directive->getPayload()));

    auto handler = (handlerIt->second.second);
    (this->*handler)(info, handlerIt->second.first);
}

std::string ExternalMediaPlayer::preprocessDirective(
    std::shared_ptr<DirectiveInfo> info,
    rapidjson::Document* document) {
    AACE_VERBOSE(LX(TAG));
    std::string playerId;

    if (!parseDirectivePayload(info, document)) {
        return playerId;
    }

    if (!jsonUtils::retrieveValue(*document, PLAYER_ID, &playerId)) {
        AACE_DEBUG(LX(TAG, "directiveHasNoPlayerId"));
    }
    return playerId;
}

void ExternalMediaPlayer::handleAuthorizeDiscoveredPlayers(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    AACE_VERBOSE(LX(TAG));

    rapidjson::Document payload;

    // A map of playerId to skillToken
    std::unordered_map<std::string, std::string> authorizedForJson;
    // The new map of m_authorizedAdapters.
    std::unordered_map<std::string, std::string> newAuthorizedAdapters;
    // The keys of the newAuthorizedAdapters map.
    std::unordered_set<std::string> newAuthorizedAdaptersKeys;
    // Deauthroized localId
    std::unordered_set<std::string> deauthorizedLocal;

    if (!parseDirectivePayload(info, &payload)) {
        return;
    }

    std::vector<aace::engine::alexa::PlayerInfo> playerInfoList;

    rapidjson::Value::ConstMemberIterator playersIt;
    if (json::jsonUtils::findNode(payload, PLAYERS, &playersIt)) {
        for (rapidjson::Value::ConstValueIterator playerIt = playersIt->value.Begin();
             playerIt != playersIt->value.End();
             playerIt++) {
            bool authorized = false;
            aace::engine::alexa::PlayerInfo playerInfo;
            std::string localPlayerId, playerId, defaultSkillToken;

            if (!(*playerIt).IsObject()) {
                AACE_ERROR(LX(TAG, "handleAuthorizeDiscoveredPlayersFailed").d("reason", "unexpectedFormat"));
                continue;
            }

            if (!json::jsonUtils::retrieveValue(*playerIt, LOCAL_PLAYER_ID, &localPlayerId)) {
                AACE_ERROR(LX(TAG, "handleAuthorizeDiscoveredPlayersFailed")
                               .d("reason", "missingAttribute")
                               .d("attribute", LOCAL_PLAYER_ID));
                continue;
            } else
                playerInfo.localPlayerId = localPlayerId;

            if (!json::jsonUtils::retrieveValue(*playerIt, AUTHORIZED, &authorized)) {
                AACE_ERROR(LX(TAG, "handleAuthorizeDiscoveredPlayersFailed")
                               .d("reason", "missingAttribute")
                               .d("attribute", AUTHORIZED));
                continue;
            } else
                playerInfo.authorized = authorized;

            if (authorized) {
                rapidjson::Value::ConstMemberIterator metadataIt;

                if (!json::jsonUtils::findNode(*playerIt, METADATA, &metadataIt)) {
                    AACE_ERROR(LX(TAG, "handleAuthorizeDiscoveredPlayersFailed")
                                   .d("reason", "missingAttribute")
                                   .d("attribute", METADATA));
                    continue;
                }

                if (!json::jsonUtils::retrieveValue(metadataIt->value, PLAYER_ID_KEY, &playerId)) {
                    AACE_ERROR(LX(TAG, "handleAuthorizeDiscoveredPlayersFailed")
                                   .d("reason", "missingAttribute")
                                   .d("attribute", PLAYER_ID_KEY));
                    continue;
                } else
                    playerInfo.playerId = playerId;
                if (!json::jsonUtils::retrieveValue(metadataIt->value, SKILL_TOKEN_KEY, &defaultSkillToken)) {
                    AACE_ERROR(LX(TAG, "handleAuthorizeDiscoveredPlayersFailed")
                                   .d("reason", "missingAttribute")
                                   .d("attribute", SKILL_TOKEN_KEY));
                    continue;
                } else
                    playerInfo.skillToken = defaultSkillToken;
            }

            AACE_DEBUG(LX(TAG)
                           .d("localPlayerId", localPlayerId)
                           .d("authorized", authorized)
                           .d("playerId", playerId)
                           .d("defaultSkillToken", defaultSkillToken));

            playerInfoList.push_back(playerInfo);
        }
    }

    m_executor.submit([this, info, playerInfoList]() {
        for (auto adapterHandler : m_adapterHandlers) {
            // adapterHandler->authorizeDiscoveredPlayer(localPlayerId, authorized, playerId, defaultSkillToken);
            adapterHandler->authorizeDiscoveredPlayers(playerInfoList);
        }
        setHandlingCompleted(info);
    });

    return;
}

std::shared_ptr<ExternalMediaAdapterHandlerInterface> ExternalMediaPlayer::getAdapterHandlerByPlayerId(
    const std::string& playerId) {
    AACE_VERBOSE(LX(TAG));
    for (auto adapterHandler : m_adapterHandlers) {
        auto adapterStates = adapterHandler->getAdapterStates(false);
        for (auto adapterState : adapterStates) {
            if (adapterState.sessionState.playerId == playerId) {
                return adapterHandler;
            }
        }
    }
    return nullptr;
}

bool ExternalMediaPlayer::isRegisteredPlayerId(const std::string& playerId) {
    return (m_externalMediaAdapterRegistration != nullptr) &&
           (m_externalMediaAdapterRegistration->getPlayerId() == playerId);
}

void ExternalMediaPlayer::handleLogin(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;
    std::string playerId = preprocessDirective(info, &payload);
    if (playerId.empty()) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullPlayerId"));
        sendExceptionEncounteredAndReportFailed(info, "missing playerId in Login directive");
        return;
    }

    std::string accessToken;
    if (!jsonUtils::retrieveValue(payload, "accessToken", &accessToken)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullAccessToken"));
        sendExceptionEncounteredAndReportFailed(info, "missing accessToken in Login directive");
        return;
    }

    std::string userName;
    if (!jsonUtils::retrieveValue(payload, USERNAME, &userName)) {
        userName = "";
    }

    int64_t refreshInterval;
    if (!jsonUtils::retrieveValue(payload, "tokenRefreshIntervalInMilliseconds", &refreshInterval)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullRefreshInterval"));
        sendExceptionEncounteredAndReportFailed(info, "missing tokenRefreshIntervalInMilliseconds in Login directive");
        return;
    }

    bool forceLogin;
    if (!jsonUtils::retrieveValue(payload, "forceLogin", &forceLogin)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullForceLogin"));
        sendExceptionEncounteredAndReportFailed(info, "missing forceLogin in Login directive");
        return;
    }

    m_executor.submit([this, info, playerId, accessToken, userName, refreshInterval, forceLogin]() {
        for (auto adapterHandler : m_adapterHandlers) {
            adapterHandler->login(
                playerId, accessToken, userName, forceLogin, std::chrono::milliseconds(refreshInterval));
        }
        setHandlingCompleted(info);
    });
    return;
}

void ExternalMediaPlayer::handleLogout(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;
    std::string playerId = preprocessDirective(info, &payload);
    if (playerId.empty()) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullPlayerId"));
        sendExceptionEncounteredAndReportFailed(info, "missing playerId in Logout directive");
        return;
    }

    m_executor.submit([this, info, playerId]() {
        for (auto adapterHandler : m_adapterHandlers) {
            adapterHandler->logout(playerId);
        }
        setHandlingCompleted(info);
    });
    return;
}

void ExternalMediaPlayer::handlePlay(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;
    std::string playerId = preprocessDirective(info, &payload);

    std::string playbackContextToken;
    if (!jsonUtils::retrieveValue(payload, "playbackContextToken", &playbackContextToken)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullPlaybackContextToken"));
        sendExceptionEncounteredAndReportFailed(info, "missing playbackContextToken in Play directive");
        return;
    }

    int64_t offset;
    if (!jsonUtils::retrieveValue(payload, "offsetInMilliseconds", &offset)) {
        offset = 0;
    }

    int64_t index;
    if (!jsonUtils::retrieveValue(payload, "index", &index)) {
        index = 0;
    }

    std::string skillToken;
    if (!jsonUtils::retrieveValue(payload, "skillToken", &skillToken)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullSkillToken"));
        sendExceptionEncounteredAndReportFailed(info, "missing skillToken in Play directive");
        return;
    }

    std::string playbackSessionId;
    if (!jsonUtils::retrieveValue(payload, "playbackSessionId", &playbackSessionId)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullPlaybackSessionId"));
        sendExceptionEncounteredAndReportFailed(info, "missing playbackSessionId in Play directive");
        return;
    }

    std::string navigation;
    if (!jsonUtils::retrieveValue(payload, "navigation", &navigation)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullNavigation"));
        sendExceptionEncounteredAndReportFailed(info, "missing navigation in Play directive");
        return;
    }

    bool preload;
    if (!jsonUtils::retrieveValue(payload, "preload", &preload)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullPreload"));
        sendExceptionEncounteredAndReportFailed(info, "missing preload in Play directive");
        return;
    }

    rapidjson::Value::ConstMemberIterator playRequestorJson;
    alexaClientSDK::avsCommon::avs::PlayRequestor playRequestor;
    if (jsonUtils::findNode(payload, "playRequestor", &playRequestorJson)) {
        if (!jsonUtils::retrieveValue(playRequestorJson->value, "type", &playRequestor.type)) {
            AACE_ERROR(LX(TAG, "handlePlayDirectiveFailed")
                           .d("reason", "missingPlayRequestorType")
                           .d("messageId", info->directive->getMessageId()));
            sendExceptionEncounteredAndReportFailed(info, "missing playRequestor type in Play directive");
            return;
        }
        if (!jsonUtils::retrieveValue(playRequestorJson->value, "id", &playRequestor.id)) {
            AACE_ERROR(LX(TAG, "handlePlayDirectiveFailed")
                           .d("reason", "missingPlayRequestorId")
                           .d("messageId", info->directive->getMessageId()));
            sendExceptionEncounteredAndReportFailed(info, "missing playRequestor id in Play directive");
            return;
        }
    }

    m_executor.submit([this,
                       request,
                       playerId,
                       playbackContextToken,
                       index,
                       offset,
                       skillToken,
                       playbackSessionId,
                       navigation,
                       preload,
                       playRequestor]() {
        for (auto adapterHandler : m_adapterHandlers) {
            setHaltInitiatorRequestHelper(request);
            if (adapterHandler->play(
                    playerId,
                    playbackContextToken,
                    index,
                    std::chrono::milliseconds(offset),
                    skillToken,
                    playbackSessionId,
                    navigation,
                    preload,
                    playRequestor)) {
                // special case where player has not started playing, but should start on next focus change
                m_ignoreExternalPauseCheck = true;
                m_haltInitiator = HaltInitiator::FOCUS_CHANGE_PAUSE;
            }
        }
    });
    setHandlingCompleted(info);
}

void ExternalMediaPlayer::handleSeek(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;
    std::string playerId = preprocessDirective(info, &payload);
    if (playerId.empty()) {
        AACE_DEBUG(LX(TAG, "directiveHasNoPlayerId"));
    }

    int64_t position;
    if (!jsonUtils::retrieveValue(payload, POSITIONINMS, &position)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullPosition"));
        sendExceptionEncounteredAndReportFailed(info, "missing positionMilliseconds in SetSeekPosition directive");
        return;
    }

    m_executor.submit([this, info, playerId, position]() {
        for (auto adapterHandler : m_adapterHandlers) {
            adapterHandler->seek(playerId, std::chrono::milliseconds(position));
        }
        setHandlingCompleted(info);
    });
    return;
}

void ExternalMediaPlayer::handleAdjustSeek(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;
    std::string playerId = preprocessDirective(info, &payload);
    if (playerId.empty()) {
        AACE_DEBUG(LX(TAG, "directiveHasNoPlayerId"));
    }

    int64_t deltaPosition;
    if (!jsonUtils::retrieveValue(payload, "deltaPositionMilliseconds", &deltaPosition)) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "nullDeltaPositionMilliseconds"));
        sendExceptionEncounteredAndReportFailed(
            info, "missing deltaPositionMilliseconds in AdjustSeekPosition directive");
        return;
    }

    if (deltaPosition < MAX_PAST_OFFSET || deltaPosition > MAX_FUTURE_OFFSET) {
        AACE_ERROR(LX(TAG, "handleDirectiveFailed").d("reason", "deltaPositionMillisecondsOutOfRange."));
        sendExceptionEncounteredAndReportFailed(
            info, "missing deltaPositionMilliseconds in AdjustSeekPosition directive");
        return;
    }

    m_executor.submit([this, info, playerId, deltaPosition]() {
        for (auto adapterHandler : m_adapterHandlers) {
            adapterHandler->adjustSeek(playerId, std::chrono::milliseconds(deltaPosition));
        }
        setHandlingCompleted(info);
    });
    return;
}

void ExternalMediaPlayer::handlePlayControl(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    std::string playerId = preprocessDirective(info, &payload);
    if (playerId.empty()) {
        AACE_DEBUG(LX(TAG, "directiveHasNoPlayerId"));
    }

    if (isRegisteredPlayerId(playerId)) {
        m_executor.submit([this, info, playerId, request]() {
            for (auto adapterHandler : m_adapterHandlers) {
                adapterHandler->playControl(playerId, request);
            }
            setHandlingCompleted(info);
        });
        return;
    }
    m_executor.submit([this, info, playerId, request]() {
        for (auto adapterHandler : m_adapterHandlers) {
            // if in focus play control, other-wise use focus change pause mechanism to initiate resume
            if (m_playerInFocus.compare(playerId) == 0) {
                setHaltInitiatorRequestHelper(request);
                adapterHandler->playControl(playerId, request);
            } else if (request == RequestType::RESUME) {
                // special case where player has not started playing, but should start on next focus change
                m_ignoreExternalPauseCheck = true;
                m_haltInitiator = HaltInitiator::FOCUS_CHANGE_PAUSE;
                setPlayerInFocus(playerId, true);
            } else
                AACE_WARN(LX(TAG)
                              .d("reason", "cannot playControl non-RESUME request for non-in-focus player")
                              .d("playerId", playerId)
                              .d("RequestType", request));
        }
        setHandlingCompleted(info);
    });
    return;
}

void ExternalMediaPlayer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
    removeDirective(info);
}

void ExternalMediaPlayer::onDeregistered() {
}

DirectiveHandlerConfiguration ExternalMediaPlayer::getConfiguration() const {
    return g_configuration;
}

void ExternalMediaPlayer::setCurrentActivity(const alexaClientSDK::avsCommon::avs::PlayerActivity currentActivity) {
    {
        std::lock_guard<std::mutex> lock(m_currentActivityMutex);
        AACE_VERBOSE(LX(TAG).d("from", m_currentActivity).d("to", currentActivity));
        m_currentActivity = currentActivity;
    }
    m_currentActivityConditionVariable.notify_all();
}

void ExternalMediaPlayer::setPlayerInFocus(const std::string& playerInFocus, bool focusAcquire) {
    m_executor.submit([this, playerInFocus, focusAcquire] { executeSetPlayerInFocus(playerInFocus, focusAcquire); });
}

void ExternalMediaPlayer::executeSetPlayerInFocus(const std::string& playerInFocus, bool acquireFocus) {
    AACE_VERBOSE(LX(TAG).d("playerId", playerInFocus).d("acquireFocus", acquireFocus ? "true" : "false"));

    // registered EMP adapter will not use this focus manger
    auto registered = isRegisteredPlayerId(playerInFocus);

    std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
    if (m_playerInFocus == playerInFocus) {
        AACE_WARN(LX(TAG).m("Player is already in focus").d("playerId", m_playerInFocus));
    }
    m_playerInFocus = playerInFocus;

    if (acquireFocus) {
        m_adapterHandlerInFocus = nullptr;
        if (registered) return;
        if (m_focus == FocusState::NONE && m_focusAcquireInProgress == false) {
            m_focusAcquireInProgress = true;
            m_focusManager->acquireChannel(CHANNEL_NAME, shared_from_this(), FOCUS_MANAGER_ACTIVITY_ID);
        }
    } else {
        if (registered) return;
        if (m_focus != FocusState::NONE) {
            m_focusManager->releaseChannel(CHANNEL_NAME, shared_from_this());
            m_haltInitiator = HaltInitiator::NONE;
            setCurrentActivity(alexaClientSDK::avsCommon::avs::PlayerActivity::IDLE);
        }
    }
}

std::string ExternalMediaPlayer::getPlayerInFocus() {
    AACE_VERBOSE(LX(TAG).d("playerInFocus", m_playerInFocus));
    std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
    return m_playerInFocus;
}

void ExternalMediaPlayer::onPlayerActivityChanged(
    alexaClientSDK::avsCommon::avs::PlayerActivity state,
    const alexaClientSDK::acsdkAudioPlayerInterfaces::AudioPlayerObserverInterface::Context& context) {
    AACE_VERBOSE(LX(TAG, "onPlayerActivityChanged").d("state", state));
    std::string playerInFocus;
    {
        std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
        playerInFocus = m_playerInFocus;
    }
    // registered EMP adapter will not use this focus manger
    auto registered = isRegisteredPlayerId(playerInFocus);
    switch (state) {
        case alexaClientSDK::avsCommon::avs::PlayerActivity::PLAYING:
            if (!registered) {
                // Release channel when audioplayer is taking foreground
                m_focusManager->releaseChannel(CHANNEL_NAME, shared_from_this());
                AACE_VERBOSE(LX(TAG, "releasing EMP channel due to audioplayer PLAYING"));
            }
            break;
        default:
            break;
    }
}

void ExternalMediaPlayer::setObserver(
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::RenderPlayerInfoCardsObserverInterface> observer) {
    AACE_VERBOSE(LX(TAG));
    std::lock_guard<std::mutex> lock{m_observersMutex};
    m_renderPlayerObserver = observer;
}

std::chrono::milliseconds ExternalMediaPlayer::getAudioItemOffset() {
    AACE_VERBOSE(LX(TAG));
    std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
    if (!m_adapterHandlerInFocus) {
        AACE_ERROR(LX(TAG, "getAudioItemOffsetFailed").d("reason", "NoActiveAdapter").d("player", m_playerInFocus));
        return std::chrono::milliseconds::zero();
    }
    return m_adapterHandlerInFocus->getOffset(m_playerInFocus);
}

void ExternalMediaPlayer::setPlayerInFocus(const std::string& playerInFocus) {
    AACE_VERBOSE(LX(TAG).d("playerId", playerInFocus));
    // registered EMP adapter will not use this focus manger
    auto registered = isRegisteredPlayerId(playerInFocus);
    if (!registered) {
        auto lock = std::unique_lock<std::mutex>(m_authorizedMutex);
        if (m_authorizedAdapters.find(playerInFocus) == m_authorizedAdapters.end()) {
            AACE_ERROR(
                LX(TAG, "setPlayerInFocusFailed").d("reason", "unauthorizedPlayer").d("playerId", playerInFocus));
            return;
        }
    }
    std::shared_ptr<ExternalMediaAdapterHandlerInterface> adapterHandlerInFocus =
        getAdapterHandlerByPlayerId(playerInFocus);

    {
        std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
        if (m_playerInFocus == playerInFocus) {
            AACE_VERBOSE(LX(TAG).m("Aborting - no change"));
            return;
        }
        m_playerInFocus = playerInFocus;
        m_playbackRouter->setHandler(shared_from_this());
        m_adapterHandlerInFocus = adapterHandlerInFocus;
    }
}

void ExternalMediaPlayer::onButtonPressed(PlaybackButton button) {
    std::string playerInFocus;
    {
        std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
        playerInFocus = m_playerInFocus;
    }
    AACE_INFO(LX(TAG).d("button", button).d("playerInFocus", playerInFocus));

    auto buttonIt = g_buttonToRequestType.find(button);
    if (g_buttonToRequestType.end() == buttonIt) {
        AACE_ERROR(LX("onButtonPressedFailed").d("reason", "buttonToRequestTypeNotFound").d("button", button));
        return;
    }

    auto requestType = buttonIt->second;

    // Route all button presses to the registered player, if one exists, regardless of whether it is the currently
    // focused player. Only registered players manage their play queue through the SDK and share the Alexa UI (i.e.,
    // other player IDs are separate apps with their own play queue and own UI), and hence only registered players
    // are expected to be targeted through the PlaybackRouter component (i.e., other player IDs manage their own button
    // presses without SDK involvement). We therefore safely assume the PlaybackRouter invocation happened due to the
    // user interacting with the UI of the Alexa media app used by only AudioPlayer and EMP registered players.
    // The button press would have routed to AudioPlayer capability if AudioPlayer had been playing more
    // recently than the registered EMP player.

    if (m_externalMediaAdapterRegistration == nullptr) {
        AACE_WARN(LX(TAG).m("ignoring button press; no registered player ID for routing found"));
        return;
    }
    auto registeredPlayerId = m_externalMediaAdapterRegistration->getPlayerId();
    if (playerInFocus.compare(registeredPlayerId) != 0) {
        AACE_INFO(LX(TAG)
                      .m("registered player is not the player in focus")
                      .d("playerInFocus", playerInFocus)
                      .d("registeredPlayer", registeredPlayerId));
    }
    auto registeredHandler = getAdapterHandlerByPlayerId(registeredPlayerId);
    if (registeredHandler == nullptr) {
        AACE_ERROR(LX("onButtonPressedFailed")
                       .d("reason", "no adapter handler for registered player found")
                       .d("registered playerId", registeredPlayerId));
        return;
    }

    if (requestType == RequestType::PAUSE || requestType == RequestType::RESUME) {
        requestType = RequestType::PAUSE_RESUME_TOGGLE;  // registered adapter handler expects this
    }
    registeredHandler->playControl(registeredPlayerId, requestType);
}

void ExternalMediaPlayer::onTogglePressed(PlaybackToggle toggle, bool action) {
    std::string playerInFocus;
    {
        std::lock_guard<std::mutex> lock{m_inFocusAdapterMutex};
        playerInFocus = m_playerInFocus;
    }

    auto toggleIt = g_toggleToRequestType.find(toggle);
    if (g_toggleToRequestType.end() == toggleIt) {
        AACE_ERROR(LX("onTogglePressedFailed").d("reason", "toggleToRequestTypeNotFound").d("toggle", toggle));
        return;
    }

    // toggleStates map is <SELECTED,DESELECTED>
    auto toggleStates = toggleIt->second;

    if (isRegisteredPlayerId(playerInFocus)) {
        if (auto adapterHandlerInFocus = getAdapterHandlerByPlayerId(playerInFocus)) {
            if (action) {
                adapterHandlerInFocus->playControl(playerInFocus, toggleStates.first);
            } else {
                adapterHandlerInFocus->playControl(playerInFocus, toggleStates.second);
            }
        } else {
            AACE_ERROR(LX("onTogglePressedFailed")
                           .d("reason", "adapterHandlerInFocusNotFound")
                           .d("playerInFocus", playerInFocus));
        }
    } else {
        m_executor.submit([this, playerInFocus, action, toggleStates]() {
            for (auto adapterHandler : m_adapterHandlers) {
                if (action) {
                    adapterHandler->playControl(playerInFocus, toggleStates.first);
                } else {
                    adapterHandler->playControl(playerInFocus, toggleStates.second);
                }
            }
        });
    }
}

void ExternalMediaPlayer::doShutdown() {
    AACE_INFO(LX(TAG));

    m_executor.shutdown();

    m_adapterHandlers.clear();
    m_externalMediaAdapterRegistration.reset();
    m_focusManager.reset();

    // Reset the EMP from being a state provider. If not there would be calls from the adapter to provide context
    // which will try to add tasks to the executor thread.
    m_contextManager->setStateProvider(SESSION_STATE, nullptr);
    m_contextManager->setStateProvider(PLAYBACK_STATE, nullptr);

    {
        std::unique_lock<std::mutex> lock{m_inFocusAdapterMutex};
        m_adapterHandlerInFocus.reset();
    }

    m_authorizedSender.reset();
    m_messageSender.reset();
    m_certifiedMessageSender.reset();
    m_exceptionEncounteredSender.reset();
    m_contextManager.reset();
    m_playbackRouter.reset();
    m_speakerManager.reset();
    m_renderPlayerObserver.reset();
}

void ExternalMediaPlayer::removeDirective(std::shared_ptr<DirectiveInfo> info) {
    // Check result too, to catch cases where DirectiveInfo was created locally, without a nullptr result.
    // In those cases there is no messageId to remove because no result was expected.
    if (info->directive && info->result) {
        CapabilityAgent::removeDirective(info->directive->getMessageId());
    }
}

void ExternalMediaPlayer::setHaltInitiatorRequestHelper(RequestType request) {
    switch (request) {
        case RequestType::PAUSE:
            m_haltInitiator = HaltInitiator::EXTERNAL_PAUSE;
            break;
        case RequestType::PAUSE_RESUME_TOGGLE:
            if (m_currentActivity == alexaClientSDK::avsCommon::avs::PlayerActivity::PLAYING ||
                (m_currentActivity == alexaClientSDK::avsCommon::avs::PlayerActivity::PAUSED &&
                 m_haltInitiator == HaltInitiator::FOCUS_CHANGE_PAUSE)) {
                m_haltInitiator = HaltInitiator::EXTERNAL_PAUSE;
            }
            break;
        case RequestType::PLAY:
        case RequestType::RESUME:
            m_haltInitiator = HaltInitiator::NONE;
            break;
        default:
            break;
    }
}

void ExternalMediaPlayer::setHandlingCompleted(std::shared_ptr<DirectiveInfo> info) {
    if (info && info->result) {
        info->result->setCompleted();
    }

    removeDirective(info);
}

void ExternalMediaPlayer::sendExceptionEncounteredAndReportFailed(
    std::shared_ptr<DirectiveInfo> info,
    const std::string& message,
    alexaClientSDK::avsCommon::avs::ExceptionErrorType type) {
    if (info && info->directive) {
        m_exceptionEncounteredSender->sendExceptionEncountered(info->directive->getUnparsedDirective(), type, message);
    }

    if (info && info->result) {
        info->result->setFailed(message);
    }

    removeDirective(info);
}

void ExternalMediaPlayer::executeProvideState(
    const alexaClientSDK::avsCommon::avs::NamespaceAndName& stateProviderName,
    bool sendToken,
    unsigned int stateRequestToken) {
    AACE_DEBUG(LX(TAG).d("sendToken", sendToken).d("stateRequestToken", stateRequestToken));
    std::string state;

    std::vector<aace::engine::alexa::AdapterState> adapterStates;
    for (auto adapterHandler : m_adapterHandlers) {
        auto handlerAdapterStates = adapterHandler->getAdapterStates();
        adapterStates.insert(adapterStates.end(), handlerAdapterStates.begin(), handlerAdapterStates.end());
    }

    if (stateProviderName == SESSION_STATE) {
        state = provideSessionState(adapterStates);
    } else if (stateProviderName == PLAYBACK_STATE) {
        state = providePlaybackState(adapterStates);
    } else {
        AACE_ERROR(LX(TAG, "executeProvideState").d("reason", "unknownStateProviderName"));
        return;
    }
    SetStateResult result;
    if (sendToken) {
        result = m_contextManager->setState(stateProviderName, state, StateRefreshPolicy::ALWAYS, stateRequestToken);
    } else {
        result = m_contextManager->setState(stateProviderName, state, StateRefreshPolicy::ALWAYS);
    }

    if (result != SetStateResult::SUCCESS) {
        AACE_ERROR(LX(TAG, "executeProvideState").d("reason", "contextManagerSetStateFailedForEMPState"));
    }
}

std::string ExternalMediaPlayer::provideSessionState(std::vector<aace::engine::alexa::AdapterState> adapterStates) {
    rapidjson::Document state(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& stateAlloc = state.GetAllocator();

    state.AddMember(rapidjson::StringRef(AGENT_KEY), std::string(m_agentString), stateAlloc);
    state.AddMember(rapidjson::StringRef(SPI_VERSION_KEY), std::string(ExternalMediaPlayer::SPI_VERSION), stateAlloc);
    //Set playerInFocus null when default player is in focus, else set a named player's playerId.
    if (m_playerInFocus.empty()) {
        state.AddMember(rapidjson::StringRef(PLAYER_IN_FOCUS), rapidjson::Value(), stateAlloc);
    } else {
        state.AddMember(rapidjson::StringRef(PLAYER_IN_FOCUS), m_playerInFocus, stateAlloc);
    }

    rapidjson::Value players(rapidjson::kArrayType);

    std::unordered_map<std::string, std::string> authorizedAdaptersCopy;
    {
        std::lock_guard<std::mutex> lock(m_authorizedMutex);
        authorizedAdaptersCopy = m_authorizedAdapters;
    }

    for (auto adapterState : adapterStates) {
        if (!adapterState.sessionState.playerId.empty()) {
            rapidjson::Value playerJson = buildSessionState(adapterState.sessionState, stateAlloc);
            players.PushBack(playerJson, stateAlloc);
        }
    }

    state.AddMember(rapidjson::StringRef(PLAYERS), players, stateAlloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (!state.Accept(writer)) {
        AACE_ERROR(LX(TAG, "provideSessionStateFailed").d("reason", "writerRefusedJsonObject"));
        return "";
    }

    return buffer.GetString();
}

// adapter handler playback states
std::string ExternalMediaPlayer::providePlaybackState(std::vector<aace::engine::alexa::AdapterState> adapterStates) {
    rapidjson::Document state(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& stateAlloc = state.GetAllocator();

    // Fetch actual PlaybackState from every player supported by the ExternalMediaPlayer.
    rapidjson::Value players(rapidjson::kArrayType);

    std::unordered_map<std::string, std::string> authorizedAdaptersCopy;
    {
        std::lock_guard<std::mutex> lock(m_authorizedMutex);
        authorizedAdaptersCopy = m_authorizedAdapters;
    }

    notifyRenderPlayerInfoCardsObservers();

    // Fill the default player state.
    bool defaultPlayerExists = false;
    rapidjson::Value playerJson;
    // adapter handlers
    for (auto adapterState : adapterStates) {
        if (!adapterState.sessionState.playerId.empty()) {
            rapidjson::Value playerJson =
                buildPlaybackState(adapterState.sessionState.playerId, adapterState.playbackState, stateAlloc);
            players.PushBack(playerJson, stateAlloc);
        } else {
            defaultPlayerExists = true;
            playerJson = buildPlaybackState("", adapterState.playbackState, stateAlloc);
        }
    }

    if (defaultPlayerExists) {
        state.CopyFrom(playerJson, stateAlloc);
    } else if (!defaultPlayerExists && !buildDefaultPlayerState(&state, stateAlloc)) {
        AACE_ERROR(LX(TAG, "PlaybackStateCreationFailed for defaultPlayer"));
        return "";
    }

    state.AddMember(PLAYERS, players, stateAlloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    if (!state.Accept(writer)) {
        AACE_ERROR(LX(TAG, "providePlaybackState").d("reason", "writerRefusedJsonObject"));
        return "";
    }

    return buffer.GetString();
}

std::unordered_set<std::shared_ptr<alexaClientSDK::avsCommon::avs::CapabilityConfiguration>> ExternalMediaPlayer::
    getCapabilityConfigurations() {
    return m_capabilityConfigurations;
}

void ExternalMediaPlayer::addObserver(std::shared_ptr<ExternalMediaPlayerObserverInterface> observer) {
    if (!observer) {
        AACE_ERROR(LX(TAG, "addObserverFailed").d("reason", "nullObserver"));
        return;
    }
    std::lock_guard<std::mutex> lock{m_observersMutex};
    m_observers.insert(observer);
}

void ExternalMediaPlayer::removeObserver(std::shared_ptr<ExternalMediaPlayerObserverInterface> observer) {
    if (!observer) {
        AACE_ERROR(LX(TAG, "removeObserverFailed").d("reason", "nullObserver"));
        return;
    }
    std::lock_guard<std::mutex> lock{m_observersMutex};
    m_observers.erase(observer);
}

void ExternalMediaPlayer::notifyObservers(
    const std::string& playerId,
    const ObservableSessionProperties* sessionProperties) {
    notifyObservers(playerId, sessionProperties, nullptr);
}

void ExternalMediaPlayer::notifyObservers(
    const std::string& playerId,
    const ObservablePlaybackStateProperties* playbackProperties) {
    notifyObservers(playerId, nullptr, playbackProperties);
}

void ExternalMediaPlayer::notifyObservers(
    const std::string& playerId,
    const ObservableSessionProperties* sessionProperties,
    const ObservablePlaybackStateProperties* playbackProperties) {
    std::unique_lock<std::mutex> lock{m_observersMutex};
    auto observers = m_observers;
    lock.unlock();

    for (const auto& observer : observers) {
        if (sessionProperties) {
            observer->onLoginStateProvided(playerId, *sessionProperties);
        }

        if (playbackProperties) {
            observer->onPlaybackStateProvided(playerId, *playbackProperties);
        }
    }
}

void ExternalMediaPlayer::notifyRenderPlayerInfoCardsObservers() {
    AACE_VERBOSE(LX(TAG));

    std::unique_lock<std::mutex> lock{m_inFocusAdapterMutex};
    if (m_adapterHandlerInFocus) {
        bool found = false;
        aace::engine::alexa::AdapterState adapterState;
        auto adapterStates = m_adapterHandlerInFocus->getAdapterStates();
        for (auto state : adapterStates) {
            if (state.sessionState.playerId == m_playerInFocus) {
                adapterState = state;
                found = true;
                break;
            }
        }
        if (!found) {
            AACE_ERROR(LX(TAG, "notifyRenderPlayerInfoCardsFailed").d("reason", "stateNotFound"));
            return;
        }

        lock.unlock();
        alexaClientSDK::avsCommon::avs::PlayerActivity playerActivity =
            alexaClientSDK::avsCommon::avs::PlayerActivity::IDLE;
        std::string str = adapterState.playbackState.state;
        if ("IDLE" == str) {
            playerActivity = PlayerActivity::IDLE;
        } else if ("START_PLAYING" == str || "PLAYING" == str) {
            playerActivity = PlayerActivity::PLAYING;
        } else if ("IS_STOPPING" == str || "STOPPED" == str) {
            playerActivity = PlayerActivity::STOPPED;
        } else if ("IS_PAUSING" == str || "PAUSED" == str) {
            playerActivity = PlayerActivity::PAUSED;
        } else if ("BUFFER_UNDERRUN" == str) {
            playerActivity = PlayerActivity::BUFFER_UNDERRUN;
        } else if ("FINISHED" == str) {
            playerActivity = PlayerActivity::FINISHED;
        } else {
            AACE_ERROR(LX(TAG, "notifyRenderPlayerInfoCardsFailed")
                           .d("reason", "invalidState")
                           .d("state", adapterState.playbackState.state));
            return;
        }
        RenderPlayerInfoCardsObserverInterface::Context context;
        context.audioItemId = adapterState.playbackState.trackId;
        context.offset = getAudioItemOffset();
        context.mediaProperties = shared_from_this();
        {
            std::lock_guard<std::mutex> lock{m_observersMutex};
            if (m_renderPlayerObserver) {
                m_renderPlayerObserver->onRenderPlayerCardsInfoChanged(playerActivity, context);
            }
        }
    }
}

}  // namespace alexa
}  // namespace engine
}  // namespace aace

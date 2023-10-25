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

#include <AASB/Engine/Alexa/AASBExternalMediaAdapter.h>
#include <AACE/Engine/Core/EngineMacros.h>

#include <AASB/Message/Alexa/ExternalMediaAdapter/AdjustSeekMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/AuthorizedPlayerInfo.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/AuthorizeMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/DiscoveredPlayerInfo.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/ExternalMediaAdapterState.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/Favorites.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/GetStateMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/LoginCompleteMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/LoginMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/LogoutCompleteMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/LogoutMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/MediaType.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/MutedState.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/MutedStateChangedMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/Navigation.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/PlaybackStateExternal.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/PlayControlMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/PlayControlType.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/PlayerErrorMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/PlayerEventMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/PlayMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/RemoveDiscoveredPlayerMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/ReportDiscoveredPlayersMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/RequestTokenMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/SeekMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/SessionStateExternal.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/SetFocusMessage.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/SupportedPlaybackOperation.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/ValidationData.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/ValidationMethod.h>
#include <AASB/Message/Alexa/ExternalMediaAdapter/VolumeChangedMessage.h>

#include <nlohmann/json.hpp>
#include <sstream>

namespace aace {
namespace alexa {

// map TaskState values to JSON as strings
NLOHMANN_JSON_SERIALIZE_ENUM(
    ExternalMediaAdapter::SupportedPlaybackOperation,
    {
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::PLAY, "PLAY"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::PAUSE, "PAUSE"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::STOP, "STOP"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::NEXT, "NEXT"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::PREVIOUS, "PREVIOUS"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::START_OVER, "START_OVER"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::FAST_FORWARD, "FAST_FORWARD"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::REWIND, "REWIND"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::ENABLE_REPEAT, "ENABLE_REPEAT"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::ENABLE_REPEAT_ONE, "ENABLE_REPEAT_ONE"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::DISABLE_REPEAT, "DISABLE_REPEAT"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::ENABLE_SHUFFLE, "ENABLE_SHUFFLE"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::DISABLE_SHUFFLE, "DISABLE_SHUFFLE"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::FAVORITE, "FAVORITE"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::UNFAVORITE, "UNFAVORITE"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::SEEK, "SEEK"},
        {aace::alexa::ExternalMediaAdapter::SupportedPlaybackOperation::ADJUST_SEEK, "ADJUST_SEEK"},
    })

//json serialization
void to_json(nlohmann::json& j, const ExternalMediaAdapter::AuthorizedPlayerInfo& s) {
    j["localPlayerId"] = s.localPlayerId;
    j["authorized"] = s.authorized;
}

void to_json(nlohmann::json& j, const ExternalMediaAdapter::SessionState& s) {
    j["endpointId"] = s.endpointId;
    j["loggedIn"] = s.loggedIn;
    j["userName"] = s.userName;
    j["isGuest"] = s.isGuest;
    j["launched"] = s.launched;
    j["active"] = s.active;
    j["accessToken"] = s.accessToken;
    j["tokenRefreshInterval"] = s.tokenRefreshInterval.count();
    j["playerCookie"] = s.playerCookie;
    j["spiVersion"] = s.spiVersion;
}

void to_json(nlohmann::json& j, const ExternalMediaAdapter::PlaybackState& s) {
    j["state"] = s.state;
    j["supportedOperations"] = s.supportedOperations;
    j["trackOffset"] = s.trackOffset.count();
    j["shuffleEnabled"] = s.shuffleEnabled;
    j["repeatEnabled"] = s.repeatEnabled;
    j["favorites"] = s.favorites;
    j["type"] = s.type;
    j["playbackSource"] = s.playbackSource;
    j["playbackSourceId"] = s.playbackSourceId;
    j["trackName"] = s.trackName;
    j["trackId"] = s.trackId;
    j["trackNumber"] = s.trackNumber;
    j["artistName"] = s.artistName;
    j["artistId"] = s.artistId;
    j["albumName"] = s.albumName;
    j["albumId"] = s.albumId;
    j["tinyURL"] = s.tinyURL;
    j["smallURL"] = s.smallURL;
    j["mediumURL"] = s.mediumURL;
    j["largeURL"] = s.largeURL;
    j["coverId"] = s.coverId;
    j["mediaProvider"] = s.mediaProvider;
    j["mediaType"] = s.mediaType;
    j["duration"] = s.duration.count();
}

void to_json(nlohmann::json& j, const ExternalMediaAdapter::ExternalMediaAdapterState& s) {
    j["sessionState"] = s.sessionState;
    j["playbackState"] = s.playbackState;
}

}  // namespace alexa
}  // namespace aace

namespace aasb {
namespace engine {
namespace alexa {

// String to identify log entries originating from this file.
static const std::string TAG("aasb.alexa.AASBExternalMediaAdapter");

using Message = aace::engine::messageBroker::Message;

std::shared_ptr<AASBExternalMediaAdapter> AASBExternalMediaAdapter::create(
    std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker) {
    try {
        ThrowIfNull(messageBroker, "invalidMessageBrokerInterface");

        auto externalMediaAdapter = std::shared_ptr<AASBExternalMediaAdapter>(new AASBExternalMediaAdapter());

        ThrowIfNot(externalMediaAdapter->initialize(messageBroker), "initializeFailed");

        return externalMediaAdapter;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return nullptr;
    }
}

bool AASBExternalMediaAdapter::initialize(
    std::shared_ptr<aace::engine::messageBroker::MessageBrokerInterface> messageBroker) {
    try {
        ThrowIfNull(messageBroker, "invalidMessageBrokerInterface");
        m_messageBroker = messageBroker;

        std::weak_ptr<AASBExternalMediaAdapter> wp = shared_from_this();
        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::ReportDiscoveredPlayersMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::ReportDiscoveredPlayersMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::ReportDiscoveredPlayersMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    std::vector<aace::alexa::ExternalMediaAdapter::DiscoveredPlayerInfo> discoveredPlayers;
                    for (auto player : payload.discoveredPlayers) {
                        aace::alexa::ExternalMediaAdapter::DiscoveredPlayerInfo discoveredPlayer;
                        discoveredPlayer.localPlayerId = player.localPlayerId;
                        discoveredPlayer.spiVersion = player.spiVersion;
                        discoveredPlayer.validationMethod =
                            aasb::message::alexa::externalMediaAdapter::toString(player.validationMethod);

                        std::vector<std::string> validationDataVector;
                        for (auto data : player.validationData) {
                            validationDataVector.push_back(data.certificate);
                        }
                        discoveredPlayer.validationData = validationDataVector;

                        discoveredPlayers.push_back(discoveredPlayer);
                    }

                    sp->reportDiscoveredPlayers(discoveredPlayers);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "ReportDiscoveredPlayersMessage").d("reason", ex.what()));
                }
            });

        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::RequestTokenMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::RequestTokenMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::RequestTokenMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    sp->requestToken(payload.localPlayerId);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "RequestTokenMessage").d("reason", ex.what()));
                }
            });

        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::LoginCompleteMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::LoginCompleteMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::LoginCompleteMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    sp->loginComplete(payload.localPlayerId);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "LoginCompleteMessage").d("reason", ex.what()));
                }
            });

        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::LogoutCompleteMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::LogoutCompleteMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::LogoutCompleteMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    sp->logoutComplete(payload.localPlayerId);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "LogoutCompleteMessage").d("reason", ex.what()));
                }
            });

        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::PlayerEventMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::PlayerEventMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::PlayerEventMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    sp->playerEvent(payload.localPlayerId, payload.eventName);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "PlayerEventMessage").d("reason", ex.what()));
                }
            });

        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::PlayerErrorMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::PlayerErrorMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::PlayerErrorMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    sp->playerError(
                        payload.localPlayerId, payload.errorName, payload.code, payload.description, payload.fatal);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "PlayerErrorMessage").d("reason", ex.what()));
                }
            });

        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::SetFocusMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::SetFocusMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::SetFocusMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    sp->setFocus(payload.localPlayerId);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "SetFocusMessage").d("reason", ex.what()));
                }
            });

        messageBroker->subscribe(
            aasb::message::alexa::externalMediaAdapter::RemoveDiscoveredPlayerMessage::topic(),
            aasb::message::alexa::externalMediaAdapter::RemoveDiscoveredPlayerMessage::action(),
            [wp](const Message& message) {
                try {
                    auto sp = wp.lock();
                    ThrowIfNull(sp, "invalidWeakPtrReference");

                    aasb::message::alexa::externalMediaAdapter::RemoveDiscoveredPlayerMessage::Payload payload =
                        nlohmann::json::parse(message.payload());

                    sp->removeDiscoveredPlayer(payload.localPlayerId);
                } catch (std::exception& ex) {
                    AACE_ERROR(LX(TAG, "RemoveDiscoveredPlayerMessage").d("reason", ex.what()));
                }
            });

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

//aace::alexa::ExternalMediaAdapter
bool AASBExternalMediaAdapter::login(
    const std::string& localPlayerId,
    const std::string& accessToken,
    const std::string& userName,
    bool forceLogin,
    std::chrono::milliseconds tokenRefreshInterval) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");

        aasb::message::alexa::externalMediaAdapter::LoginMessage message;

        message.payload.localPlayerId = localPlayerId;
        message.payload.accessToken = accessToken;
        message.payload.userName = userName;
        message.payload.forceLogin = forceLogin;
        message.payload.tokenRefreshInterval = tokenRefreshInterval.count();

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::logout(const std::string& localPlayerId) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");

        aasb::message::alexa::externalMediaAdapter::LogoutMessage message;

        message.payload.localPlayerId = localPlayerId;

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::play(
    const std::string& localPlayerId,
    const std::string& playContextToken,
    int64_t index,
    std::chrono::milliseconds offset,
    bool preload,
    aace::alexa::ExternalMediaAdapter::Navigation navigation) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");

        aasb::message::alexa::externalMediaAdapter::PlayMessage message;

        message.payload.localPlayerId = localPlayerId;
        message.payload.playContextToken = playContextToken;
        message.payload.index = index;
        message.payload.offset = offset.count();
        message.payload.preload = preload;
        message.payload.navigation = static_cast<aasb::message::alexa::externalMediaAdapter::Navigation>(navigation);

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::playControl(const std::string& localPlayerId, PlayControlType controlType) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");

        aasb::message::alexa::externalMediaAdapter::PlayControlMessage message;

        message.payload.localPlayerId = localPlayerId;
        message.payload.controlType =
            static_cast<aasb::message::alexa::externalMediaAdapter::PlayControlType>(controlType);

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::seek(const std::string& localPlayerId, std::chrono::milliseconds offset) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");
        aasb::message::alexa::externalMediaAdapter::SeekMessage message;

        message.payload.localPlayerId = localPlayerId;
        message.payload.offset = offset.count();

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::adjustSeek(const std::string& localPlayerId, std::chrono::milliseconds deltaOffset) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");
        aasb::message::alexa::externalMediaAdapter::AdjustSeekMessage message;

        message.payload.localPlayerId = localPlayerId;
        message.payload.deltaOffset = deltaOffset.count();

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::authorize(
    const std::vector<aace::alexa::ExternalMediaAdapter::AuthorizedPlayerInfo>& authorizedPlayers) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");
        aasb::message::alexa::externalMediaAdapter::AuthorizeMessage message;

        std::vector<aasb::message::alexa::externalMediaAdapter::AuthorizedPlayerInfo> aasbAuthorizedPlayers;
        for (auto player : authorizedPlayers) {
            aasb::message::alexa::externalMediaAdapter::AuthorizedPlayerInfo aasbPlayer;
            aasbPlayer.localPlayerId = player.localPlayerId;
            aasbPlayer.authorized = player.authorized;
            aasbAuthorizedPlayers.push_back(aasbPlayer);
        }
        message.payload.authorizedPlayers = aasbAuthorizedPlayers;

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::getState(
    const std::string& localPlayerId,
    aace::alexa::ExternalMediaAdapter::ExternalMediaAdapterState& state) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto messageBroker = m_messageBroker.lock();
        ThrowIfNull(messageBroker, "invalidMessageBrokerReference");
        aasb::message::alexa::externalMediaAdapter::GetStateMessage message;

        message.payload.localPlayerId = localPlayerId;

        auto result = messageBroker->publish(message.toString()).get();
        ThrowIfNot(result.valid(), "waitForReplyTimeout");

        aasb::message::alexa::externalMediaAdapter::GetStateMessageReply::Payload payload =
            nlohmann::json::parse(result.payload());

        // AASB SessionState
        auto& sessionState = payload.state.sessionState;
        state.sessionState.endpointId = sessionState.endpointId;
        state.sessionState.loggedIn = sessionState.loggedIn;
        state.sessionState.isGuest = sessionState.isGuest;
        state.sessionState.launched = sessionState.launched;
        state.sessionState.active = sessionState.active;
        state.sessionState.accessToken = sessionState.accessToken;
        state.sessionState.tokenRefreshInterval = std::chrono::milliseconds{sessionState.tokenRefreshInterval};
        state.sessionState.playerCookie = sessionState.playerCookie;
        state.sessionState.spiVersion = sessionState.spiVersion;

        // AASB PlaybackState
        auto& playbackState = payload.state.playbackState;
        state.playbackState.state = playbackState.state;

        std::vector<ExternalMediaAdapter::SupportedPlaybackOperation> supportedOperations;
        for (auto operation : playbackState.supportedOperations) {
            supportedOperations.push_back(
                static_cast<ExternalMediaAdapter::SupportedPlaybackOperation>(operation));
        }
        state.playbackState.supportedOperations = supportedOperations;

        state.playbackState.trackOffset = std::chrono::milliseconds{playbackState.trackOffset};
        state.playbackState.shuffleEnabled = playbackState.shuffleEnabled;
        state.playbackState.repeatEnabled = playbackState.repeatEnabled;
        state.playbackState.favorites = static_cast<ExternalMediaAdapter::Favorites>(playbackState.favorites);
        state.playbackState.type = playbackState.type;
        state.playbackState.playbackSource = playbackState.playbackSource;
        state.playbackState.playbackSourceId = playbackState.playbackSourceId;
        state.playbackState.trackName = playbackState.trackName;
        state.playbackState.trackId = playbackState.trackId;
        state.playbackState.trackNumber = playbackState.trackNumber;
        state.playbackState.artistName = playbackState.artistName;
        state.playbackState.artistId = playbackState.artistId;
        state.playbackState.albumName = playbackState.albumName;
        state.playbackState.albumId = playbackState.albumId;
        state.playbackState.tinyURL = playbackState.tinyURL;
        state.playbackState.smallURL = playbackState.smallURL;
        state.playbackState.mediumURL = playbackState.mediumURL;
        state.playbackState.largeURL = playbackState.largeURL;
        state.playbackState.coverId = playbackState.coverId;
        state.playbackState.mediaProvider = playbackState.mediaProvider;
        state.playbackState.mediaType = static_cast<ExternalMediaAdapter::MediaType>(playbackState.mediaType);
        state.playbackState.duration = std::chrono::milliseconds{playbackState.duration};

        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::volumeChanged(float volume) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");
        aasb::message::alexa::externalMediaAdapter::VolumeChangedMessage message;

        message.payload.volume = volume;

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

bool AASBExternalMediaAdapter::mutedStateChanged(aace::alexa::ExternalMediaAdapter::MutedState state) {
    try {
        AACE_VERBOSE(LX(TAG));

        auto m_messageBroker_lock = m_messageBroker.lock();
        ThrowIfNull(m_messageBroker_lock, "invalidMessageBrokerReference");

        aasb::message::alexa::externalMediaAdapter::MutedStateChangedMessage message;

        message.payload.state = static_cast<aasb::message::alexa::externalMediaAdapter::MutedState>(state);

        m_messageBroker_lock->publish(message.toString()).send();
        return true;
    } catch (std::exception& ex) {
        AACE_ERROR(LX(TAG).d("reason", ex.what()));
        return false;
    }
}

}  // namespace alexa
}  // namespace engine
}  // namespace aasb

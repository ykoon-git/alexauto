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

#ifndef AACE_AUDIO_AUDIO_OUTPUT_H
#define AACE_AUDIO_AUDIO_OUTPUT_H

#include <memory>
#include <map>

#include "AudioEngineInterfaces.h"
#include "AudioStream.h"

/** @file */

namespace aace {
namespace audio {

/**
 * AudioOutput should be extended to play audio data provided by the Engine.
 *
 * After returning @c true from a playback-controlling method invocation from the Engine (i.e. @c play(), @c pause(),
 * @c stop(), @c resume()), it is required that platform implementation notify the Engine of a playback state change by
 * calling one of @c mediaStateChanged() with the new @c MediaState or @c mediaError() with the @c MediaError. The
 * Engine expects no call to @c mediaStateChanged() in response to an invocation for which the platform returned
 * @c false.
 *
 * The platform implementation may call @c mediaError() or @c mediaStateChanged() with @c MediaState::BUFFERING
 * at any time during a playback operation to notify the Engine of an error or buffer underrun, respectively.
 * When the media player resumes playback after a buffer underrun, the platform implementation should call
 * @c mediaStateChanged() with @c MediaState::PLAYING.
 *
 * @note The @c AudioOutput platform implementation should be able to support the
 * audio formats recommended by AVS for a familiar Alexa experience:
 * https://developer.amazon.com/docs/alexa-voice-service/recommended-media-support.html
 */
class AudioOutput {
protected:
    AudioOutput() = default;

public:
    /**
     * Describes the playback state of the platform media player
     * @sa @c aace::alexa::MediaPlayerEngineInterface::MediaState
     */
    using MediaState = aace::audio::AudioOutputEngineInterface::MediaState;

    /**
     * Describes an error during a media playback operation
     * @sa @c aace::alexa::MediaPlayerEngineInterface::MediaError
     */
    using MediaError = aace::audio::AudioOutputEngineInterface::MediaError;

    /**
     * Describes a focus action platform interface reports or requests
     * @sa @c aace::audio::AudioOutputEngineInterface::FocusAction
     */
    using FocusAction = aace::audio::AudioOutputEngineInterface::FocusAction;

    /**
     * Used when audio time is unknown or indeterminate.
     */
    static const int64_t TIME_UNKNOWN = -1;

    enum class MutedState {
        /**
         * The audio channel state id muted.
         */
        MUTED,

        /**
         * The audio channel state id unmuted.
         */
        UNMUTED
    };

    virtual ~AudioOutput();

    /**
     * Notifies the platform implementation to prepare for playback of an
     * @c AudioStream audio source. After returning @c true, the Engine will call @c play()
     * to initiate audio playback.
     *
     * @param [in] stream The @c AudioStream object that provides the platform implementation
     * audio data to play.
     * @param [in] repeating @c true if the platform should loop the audio when playing.
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool prepare(std::shared_ptr<AudioStream> stream, bool repeating) = 0;

    /**
     * Context related to playback of an audio item.
     */
    struct PlaybackContext {
        typedef std::map<std::string, std::string> HeaderConfig;

        /**
         * Headers to use when fetching encryption keys. The map contains up to
         * 20 pairs of header name and value. Header names may be
         * "Authorization" or strings prefixed with "x-", containing up to 256
         * characters. Values may contain up to 4096 characters.
         */
        HeaderConfig keyConfig;

        /**
         * Headers to use when fetching manifests. The map contains up to 20
         * pairs of header name and value. Header names may be "Authorization"
         * or strings prefixed with "x-", containing up to 256 characters.
         * Values may contain up to 4096 characters.
         */
        HeaderConfig manifestConfig;

        /**
         * Headers to use when fetching audio chunks described in the manifest.
         * The map contains up to 20 pairs of header name and value. Header
         * names may be "Authorization" or strings prefixed with "x-",
         * containing up to 256 characters. Values may contain up to 4096
         * characters.
         */
        HeaderConfig audioSegmentConfig;

        /**
         * A catch-all list of headers to use in all URL requests. The map
         * contains up to 20 pairs of header name and value. The headers in
         * @c keyConfig, @c manifestConfig, and @c audioSegmentConfig take
         * priority over the "all" headers, and hence any name-value pairs in
         * the higher priority lists must overwrite any pair with the same
         * name from @c allConfig.
         *
         * Header names may be "Authorization" or strings prefixed with "x-",
         * containing up to 256 characters. Values may contain up to 4096
         * characters.
         */
        HeaderConfig allConfig;
    };

    /**
     * Notifies the platform implementation to prepare for playback of a
     * URL audio source. After returning @c true, the Engine will call @c play()
     * to initiate audio playback.
     *
     * @param [in] url The URL audio source to set in the platform media player
     * @param [in] repeating @c true if the platform should loop the audio when playing.
     * @param [in] playbackContext The context related to playback of an audio item.
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool prepare(const std::string& url, bool repeating, const PlaybackContext& playbackContext) = 0;

    /**
     * Notifies the platform implementation only if prepared media allows platform interface to duck the volume
     * if any high priority audio stream is in the focus. If platform interface ducks the volume, report the
     * state using @c audioFocusEvent always. If @c mayDuck is not called, platform interface can assume that media
     * is not allowed to duck.
     */
    virtual void mayDuck() = 0;

    /**
     * Notifies the platform implementation to start playback of the current audio source. After returning @c true,
     * the platform implementation must call @c mediaStateChanged() with @c MediaState.PLAYING
     * when the media player begins playing the audio or @c mediaError() if an error occurs.
     *
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool play() = 0;

    /**
     * Notifies the platform implementation to stop playback of the current audio source. After returning @c true,
     * the platform implementation must call @c mediaStateChanged() with @c MediaState.STOPPED
     * when the media player stops playing the audio or immediately if it is already stopped, or @c mediaError() if an error occurs.
     * A subsequent call to @c play() will be preceded by calls to @c prepare()
     * and @c setPosition().
     *
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool stop() = 0;

    /**
     * Notifies the platform implementation to pause playback of the current audio source. After returning @c true,
     * the platform implementation must call @c mediaStateChanged() with @c MediaState.STOPPED
     * when the media player pauses the audio or @c mediaError() if an error occurs.
     * A subsequent call to @c resume() will not be preceded by calls to @c prepare()
     * and @c setPosition().
     *
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool pause() = 0;

    /**
     * Notifies the platform implementation to resume playback of the current audio source. After returning @c true,
     * the platform implementation must call @c mediaStateChanged() with @c MediaState.PLAYING
     * when the media player resumes the audio or @c mediaError() if an error occurs.
     *
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool resume() = 0;

    /**
     * Notifies the platform implementation to move the playback in background.
     * If platform implementation supports audio ducking, reduce the media player volume according to platform guidelines.
     * @return @c true if the platform implementation successfully attenuated the volume, else @c false.
     * If @c false is returned, @c stopDucking call will not be received.
     */
    virtual bool startDucking() = 0;

    /**
     * Notifies the platform implementation to move the playback in foreground.
     * If platform implementation supports audio ducking, restore the media player volume to original value.
     * @return @c true if the platform implementation successfully restored the volume, else @c false.
     * If @c false is returned, internal state considers that platform implementation is still in the ducked state which may result into unexpected behavior.
     */
    virtual bool stopDucking() = 0;

    /**
     * Returns the current playback position of the platform media player.
     * If the audio source is not playing, the most recent position played
     * should be returned.
     *
     * @return The platform media player's playback position in milliseconds,
     * or @c TIME_UNKNOWN if the current media position is unknown or invalid.
     */
    virtual int64_t getPosition() = 0;

    /**
     * Notifies the platform implementation to set the playback position of the current audio source
     * in the platform media player.
     *
     * @param [in] position The playback position in milliseconds to set in the platform media player
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool setPosition(int64_t position) = 0;

    /**
     * Returns the duration of the current audio source. If the duration is unknown, then
     * @c TIME_UNKNOWN should be returned.
     *
     * @return The duration of the current audio source in milliseconds, or @c TIME_UNKNOWN.
     */
    virtual int64_t getDuration() = 0;

    /**
     * Returns the amount of audio data buffered.
     *
     * @return the number of bytes of the audio data buffered, or 0 if it's unknown.
     */
    virtual int64_t getNumBytesBuffered();

    /**
     * Notifies the platform implementation to set the volume of the output channel. The
     * @c volume value should be scaled to fit the needs of the platform.
     *
     * @param [in] volume The volume to set on the output channel. @c volume
     * is in the range [0,1].
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool volumeChanged(float volume) = 0;

    /**
     * Notifies the platform implementation to apply a muted state has changed for
     * the output channel
     *
     * @param [in] state The muted state to apply to the output channel. @c MutedState::MUTED when
     * the output channel be muted, @c MutedState::UNMUTED when unmuted
     * @return @c true if the platform implementation successfully handled the call,
     * else @c false
     */
    virtual bool mutedStateChanged(MutedState state) = 0;

    /**
     * Notifies the Engine of an audio playback state change in the platform implementation.
     * Must be called when the platform media player transitions between stopped and playing states.
     *
     * @param [in] state The new playback state of the platform media player
     * @sa MediaState
     */
    void mediaStateChanged(MediaState state);

    /**
     * Notifies the Engine of an error during audio playback
     *
     * @param [in] error The error encountered by the platform media player during playback
     * @param [in] description A description of the error
     * @sa MediaError
     */
    void mediaError(MediaError error, const std::string& description = "");

    /**
     * Request engine to perform the action mentioned in the parameter.
     *
     * @param [in] action An @c FocusAction platform interface wishes to request.
     */
    void audioFocusEvent(FocusAction action);

    /**
     * @internal
     * Sets the Engine interface delegate.
     *
     * Should *never* be called by the platform implementation.
     */
    void setEngineInterface(std::shared_ptr<aace::audio::AudioOutputEngineInterface> audioOutputEngineInterface);

private:
    std::weak_ptr<aace::audio::AudioOutputEngineInterface> m_audioOutputEngineInterface;
};

inline std::ostream& operator<<(std::ostream& stream, const AudioOutput::MutedState& state) {
    switch (state) {
        case AudioOutput::MutedState::MUTED:
            stream << "MUTED";
            break;
        case AudioOutput::MutedState::UNMUTED:
            stream << "UNMUTED";
            break;
    }
    return stream;
}

inline std::ostream& operator<<(std::ostream& stream, const AudioOutput::FocusAction& action) {
    switch (action) {
        case AudioOutput::FocusAction::REPORT_DUCKING_STARTED:
            stream << "REPORT_DUCKING_STARTED";
            break;
        case AudioOutput::FocusAction::REPORT_DUCKING_STOPPED:
            stream << "REPORT_DUCKING_STOPPED";
            break;
    }
    return stream;
}

}  // namespace audio
}  // namespace aace

#endif  // AACE_AUDIO_AUDIO_OUTPUT_H

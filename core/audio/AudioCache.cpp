/****************************************************************************
 Copyright (c) 2014-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2018-2020 HALX99.
 Copyright (c) 2020 C4games Ltd.
 Copyright (c) 2021-2022 Bytedance Inc.

 https://adxeproject.github.io/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#define LOG_TAG "AudioCache"

#include "platform/CCPlatformConfig.h"

#include "audio/AudioCache.h"
#include <thread>
#include "base/CCDirector.h"
#include "base/CCScheduler.h"

#include "audio/AudioDecoderManager.h"
#include "audio/AudioDecoder.h"

#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#    define ALOGVV ALOGV
#else
#    define ALOGVV(...) \
        do              \
        {               \
        } while (false)
#endif

namespace
{
unsigned int __idIndex = 0;
}

#define INVALID_AL_BUFFER_ID 0xFFFFFFFF
#define PCMDATA_CACHEMAXSIZE 1048576

using namespace cocos2d;

AudioCache::AudioCache()
    : _totalFrames(0)
    , _framesRead(0)
    , _format(-1)
    , _duration(0.0f)
    , _alBufferId(INVALID_AL_BUFFER_ID)
    , _queBufferFrames(0)
    , _state(State::INITIAL)
    , _isDestroyed(std::make_shared<bool>(false))
    , _id(++__idIndex)
    , _isLoadingFinished(false)
    , _isSkipReadDataTask(false)
{
    ALOGVV("AudioCache() %p, id=%u", this, _id);
    for (int i = 0; i < QUEUEBUFFER_NUM; ++i)
    {
        _queBuffers[i]    = nullptr;
        _queBufferSize[i] = 0;
    }
}

AudioCache::~AudioCache()
{
    ALOGVV("~AudioCache() %p, id=%u, begin", this, _id);
    *_isDestroyed = true;
    while (!_isLoadingFinished)
    {
        if (_isSkipReadDataTask)
        {
            ALOGV("id=%u, Skip read data task, don't continue to wait!", _id);
            break;
        }
        ALOGVV("id=%u, waiting readData thread to finish ...", _id);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // wait for the 'readDataTask' task to exit
    _readDataTaskMutex.lock();

    if (_state == State::READY)
    {
        if (_alBufferId != INVALID_AL_BUFFER_ID && alIsBuffer(_alBufferId))
        {
            ALOGV("~AudioCache(id=%u), delete buffer: %u", _id, _alBufferId);
            alDeleteBuffers(1, &_alBufferId);
            _alBufferId = INVALID_AL_BUFFER_ID;
        }
    }
    else
    {
        ALOGW("AudioCache (%p), id=%u, buffer isn't ready, state=%d", this, _id, (int)_state);
    }

    if (_queBufferFrames > 0)
    {
        for (int index = 0; index < QUEUEBUFFER_NUM; ++index)
        {
            free(_queBuffers[index]);
        }
    }
    ALOGVV("~AudioCache() %p, id=%u, end", this, _id);
    _readDataTaskMutex.unlock();
}

void AudioCache::readDataTask(unsigned int selfId)
{
    // Note: It's in sub thread
    ALOGVV("readDataTask begin, cache id=%u", selfId);

    _readDataTaskMutex.lock();
    _state = State::LOADING;

    AudioDecoder* decoder = AudioDecoderManager::createDecoder(_fileFullPath);
    do
    {
        if (decoder == nullptr || !decoder->open(_fileFullPath))
            break;

        const uint32_t originalTotalFrames = decoder->getTotalFrames();
        const uint32_t sampleRate          = decoder->getSampleRate();
        const uint32_t channelCount        = decoder->getChannelCount();
        const auto sourceFormat            = decoder->getSourceFormat();

        uint32_t totalFrames     = originalTotalFrames;
        uint32_t dataSize        = decoder->framesToBytes(totalFrames);
        uint32_t remainingFrames = totalFrames;

        switch (sourceFormat)
        {
        case AUDIO_SOURCE_FORMAT::PCM_16:
            _format = channelCount > 1 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;  // bits depth: 16bits
            break;
        case AUDIO_SOURCE_FORMAT::PCM_U8:
            _format = channelCount > 1 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;  // bits depth: 8bits
            break;
#if AX_USE_ALSOFT
        case AUDIO_SOURCE_FORMAT::PCM_FLT32:
            _format = channelCount > 1 ? AL_FORMAT_STEREO_FLOAT32 : AL_FORMAT_MONO_FLOAT32;
            break;
        case AUDIO_SOURCE_FORMAT::PCM_FLT64:
            _format = channelCount > 1 ? AL_FORMAT_STEREO_DOUBLE_EXT : AL_FORMAT_MONO_DOUBLE_EXT;
            break;
        case AUDIO_SOURCE_FORMAT::MULAW:
            _format = channelCount > 1 ? AL_FORMAT_STEREO_MULAW : AL_FORMAT_MONO_MULAW;
            break;
        case AUDIO_SOURCE_FORMAT::ALAW:
            _format = channelCount > 1 ? AL_FORMAT_STEREO_ALAW_EXT : AL_FORMAT_MONO_ALAW_EXT;
            break;
        case AUDIO_SOURCE_FORMAT::ADPCM:
            _format = channelCount > 1 ? AL_FORMAT_STEREO_MSADPCM_SOFT : AL_FORMAT_MONO_MSADPCM_SOFT;
            break;
        case AUDIO_SOURCE_FORMAT::IMA_ADPCM:
            _format = channelCount > 1 ? AL_FORMAT_STEREO_IMA4 : AL_FORMAT_MONO_IMA4;
            break;
#endif
        default:
            assert(false);
        }

        _sampleRate  = (ALsizei)sampleRate;
        _duration    = 1.0f * totalFrames / sampleRate;
        _totalFrames = totalFrames;

        if (dataSize <= PCMDATA_CACHEMAXSIZE)
        {
            uint32_t framesRead = 0;
            const uint32_t framesToReadOnce =
                std::min(totalFrames, static_cast<uint32_t>(sampleRate * QUEUEBUFFER_TIME_STEP * QUEUEBUFFER_NUM));

            alGenBuffers(1, &_alBufferId);
            auto alError = alGetError();
            if (alError != AL_NO_ERROR)
            {
                ALOGE("%s: attaching audio to buffer fail: %x", __FUNCTION__, alError);
                break;
            }

            std::vector<char> pcmBuffer(dataSize, 0);
            auto pcmData = pcmBuffer.data();

            if (*_isDestroyed)
                break;

            framesRead = decoder->readFixedFrames((std::min)(framesToReadOnce, remainingFrames),
                                                  pcmData + decoder->framesToBytes(_framesRead));
            _framesRead += framesRead;
            remainingFrames -= framesRead;

            if (*_isDestroyed)
                break;

            uint32_t frames = 0;
            while (!*_isDestroyed && _framesRead < originalTotalFrames)
            {
                frames = (std::min)(framesToReadOnce, remainingFrames);
                if (_framesRead + frames > originalTotalFrames)
                {
                    frames = originalTotalFrames - _framesRead;
                }
                framesRead = decoder->read(frames, pcmData + decoder->framesToBytes(_framesRead));
                if (framesRead == 0)
                    break;
                _framesRead += framesRead;
                remainingFrames -= framesRead;
            }

            if (*_isDestroyed)
                break;

            if (_framesRead < originalTotalFrames)
            {
                memset(pcmData + decoder->framesToBytes(_framesRead), 0x00,
                       decoder->framesToBytes(totalFrames - _framesRead));
            }

#if AX_USE_ALSOFT
            ALOGV("pcm buffer was loaded successfully, total frames: %u, total read frames: %u, remainingFrames: %u",
                  totalFrames, _framesRead, remainingFrames);
            if (sourceFormat == AUDIO_SOURCE_FORMAT::ADPCM || sourceFormat == AUDIO_SOURCE_FORMAT::IMA_ADPCM)
                alBufferi(_alBufferId, AL_UNPACK_BLOCK_ALIGNMENT_SOFT, decoder->getSamplesPerBlock());
            alBufferData(_alBufferId, _format, pcmData, (ALsizei)dataSize, (ALsizei)sampleRate);
#else
#    if !AX_USE_ALSOFT
            /// Apple OpenAL framework, try adjust frames
            /// May don't need, xcode11 sdk works well
            uint32_t adjustFrames = 0;
            BREAK_IF_ERR_LOG(!decoder->seek(totalFrames), "AudioDecoder::seek(%u) error", totalFrames);

            char* tmpBuf = (char*)malloc(decoder->framesToBytes(framesToReadOnce));
            std::vector<char> adjustFrameBuf;
            adjustFrameBuf.reserve(decoder->framesToBytes(framesToReadOnce));

            // Adjust total frames by setting position to the end of frames and try to read more data.
            // This is a workaround for https://github.com/cocos2d/cocos2d-x/issues/16938
            do
            {
                framesRead = decoder->read(framesToReadOnce, tmpBuf);
                if (framesRead > 0)
                {
                    adjustFrames += framesRead;
                    adjustFrameBuf.insert(adjustFrameBuf.end(), tmpBuf, tmpBuf + decoder->framesToBytes(framesRead));
                }

            } while (framesRead > 0);

            if (adjustFrames > 0)
            {
                ALOGV("Orignal total frames: %u, adjust frames: %u, current total frames: %u", totalFrames,
                      adjustFrames, totalFrames + adjustFrames);
                totalFrames += adjustFrames;
                _totalFrames = remainingFrames = totalFrames;
            }

            free(tmpBuf);

            // Reset to frame 0
            BREAK_IF_ERR_LOG(!decoder->seek(0), "AudioDecoder::seek(0) failed!");

            if (adjustFrames > 0)
            {
                pcmBuffer.insert(pcmBuffer.end(), adjustFrameBuf.data(), adjustFrameBuf.data() + adjustFrameBuf.size());
                pcmData  = pcmBuffer.data();
                dataSize = static_cast<uint32_t>(pcmBuffer.size());
            }
#    endif /* Adjust frames, may not needed */
            ALOGV(
                "pcm buffer was loaded successfully, total frames: %u, total read frames: %u, adjust frames: %u, "
                "remainingFrames: %u",
                totalFrames, _framesRead, adjustFrames, remainingFrames);
            _framesRead += adjustFrames;
            alBufferData(_alBufferId, _format, pcmData, (ALsizei)dataSize, (ALsizei)sampleRate);
#endif
            alError = alGetError();
            if (alError != AL_NO_ERROR)
            {
                ALOGE("%s:alBufferData error code:%x", __FUNCTION__, alError);
                break;
            }

            _state = State::READY;
        }
        else
        {
            _queBufferFrames = sampleRate * QUEUEBUFFER_TIME_STEP;
            BREAK_IF_ERR_LOG(_queBufferFrames == 0, "_queBufferFrames == 0");

            const uint32_t queBufferBytes = decoder->framesToBytes(_queBufferFrames);

            for (int index = 0; index < QUEUEBUFFER_NUM; ++index)
            {
                _queBuffers[index]    = (char*)malloc(queBufferBytes);
                _queBufferSize[index] = queBufferBytes;

                decoder->readFixedFrames(_queBufferFrames, _queBuffers[index]);
            }

            _state = State::READY;
        }

    } while (false);

    AudioDecoderManager::destroyDecoder(decoder);

    if (_state != State::READY)
    {
        _state = State::FAILED;
        if (_alBufferId != INVALID_AL_BUFFER_ID && alIsBuffer(_alBufferId))
        {
            ALOGV("readDataTask failed, delete buffer: %u", _alBufferId);
            alDeleteBuffers(1, &_alBufferId);
            _alBufferId = INVALID_AL_BUFFER_ID;
        }
    }

    // Set before invokingPlayCallbacks, otherwise, may cause dead-lock
    _isLoadingFinished = true;

    // FIXME: Why to invoke play callback first? Should it be after 'load' callback?
    invokingPlayCallbacks();
    invokingLoadCallbacks();

    _readDataTaskMutex.unlock();
    ALOGVV("readDataTask end, cache id=%u", selfId);
}

void AudioCache::addPlayCallback(const std::function<void()>& callback)
{
    std::lock_guard<std::mutex> lk(_playCallbackMutex);
    switch (_state)
    {
    case State::INITIAL:
    case State::LOADING:
        _playCallbacks.push_back(callback);
        break;

    case State::READY:
    // If state is failure, we still need to invoke the callback
    // since the callback will set the 'AudioPlayer::_removeByAudioEngine' flag to true.
    case State::FAILED:
        callback();
        break;

    default:
        ALOGE("Invalid state: %d", (int)_state);
        break;
    }
}

void AudioCache::invokingPlayCallbacks()
{
    std::lock_guard<std::mutex> lk(_playCallbackMutex);

    for (auto&& cb : _playCallbacks)
    {
        cb();
    }

    _playCallbacks.clear();
}

void AudioCache::addLoadCallback(const std::function<void(bool)>& callback)
{
    switch (_state)
    {
    case State::INITIAL:
    case State::LOADING:
        _loadCallbacks.push_back(callback);
        break;

    case State::READY:
        callback(true);
        break;
    case State::FAILED:
        callback(false);
        break;

    default:
        ALOGE("Invalid state: %d", (int)_state);
        break;
    }
}

void AudioCache::invokingLoadCallbacks()
{
    if (*_isDestroyed)
    {
        ALOGV("AudioCache (%p) was destroyed, don't invoke preload callback ...", this);
        return;
    }

    auto isDestroyed = _isDestroyed;
    auto scheduler   = Director::getInstance()->getScheduler();
    scheduler->performFunctionInCocosThread([&, isDestroyed]() {
        if (*isDestroyed)
        {
            ALOGV("invokingLoadCallbacks perform in cocos thread, AudioCache (%p) was destroyed!", this);
            return;
        }

        for (auto&& cb : _loadCallbacks)
        {
            cb(_state == State::READY);
        }

        _loadCallbacks.clear();
    });
}

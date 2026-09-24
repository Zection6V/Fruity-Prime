#include "AL.hpp"

#if !defined(__ANDROID__)

#include <vector>

#if defined(__APPLE__)
#include <OpenAL/al.h>
#else
#include <AL/al.h>
#endif

namespace OpenTK::Audio::OpenAL::AL
{
    std::int32_t GenSource()
    {
        ALuint name = 0;
        ::alGenSources(1, &name);
        return static_cast<std::int32_t>(name);
    }

    void GenBuffers(std::span<std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.size());
        ::alGenBuffers(static_cast<ALsizei>(names.size()), names.data());
        for (std::size_t i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = static_cast<std::int32_t>(names[i]);
        }
    }

    void DeleteSource(std::int32_t source)
    {
        const ALuint name = static_cast<ALuint>(source);
        ::alDeleteSources(1, &name);
    }

    void DeleteBuffers(std::span<const std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.begin(), buffers.end());
        ::alDeleteBuffers(static_cast<ALsizei>(names.size()), names.data());
    }

    void Source(std::int32_t source, ALSourcef param, float value)
    {
        ::alSourcef(static_cast<ALuint>(source), static_cast<ALenum>(param), value);
    }

    std::int32_t GetSource(std::int32_t source, ALGetSourcei param)
    {
        ALint value = 0;
        ::alGetSourcei(static_cast<ALuint>(source), static_cast<ALenum>(param), &value);
        return static_cast<std::int32_t>(value);
    }

    void SourcePlay(std::int32_t source)
    {
        ::alSourcePlay(static_cast<ALuint>(source));
    }

    void SourceStop(std::int32_t source)
    {
        ::alSourceStop(static_cast<ALuint>(source));
    }

    void SourceQueueBuffers(std::int32_t source, std::span<const std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.begin(), buffers.end());
        ::alSourceQueueBuffers(static_cast<ALuint>(source),
            static_cast<ALsizei>(names.size()), names.data());
    }

    void SourceUnqueueBuffers(std::int32_t source, std::span<std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.size());
        ::alSourceUnqueueBuffers(static_cast<ALuint>(source),
            static_cast<ALsizei>(names.size()), names.data());
        for (std::size_t i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = static_cast<std::int32_t>(names[i]);
        }
    }

    void BufferData(std::int32_t buffer, ALFormat format,
        std::span<const std::int16_t> data, std::int32_t frequency)
    {
        ::alBufferData(static_cast<ALuint>(buffer), static_cast<ALenum>(format),
            data.data(), static_cast<ALsizei>(data.size() * sizeof(std::int16_t)),
            frequency);
    }
}

#endif

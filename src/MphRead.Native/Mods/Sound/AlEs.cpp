#include "AlEs.hpp"

#if defined(__ANDROID__)
#include "SfxMixer.hpp"

namespace OpenTK::Audio::OpenAL
{
    const ALDevice ALDevice::Null{};
    const ALContext ALContext::Null{};
}

namespace MphRead::Mods::Sound
{
    using OpenTK::Audio::OpenAL::ALContext;
    using OpenTK::Audio::OpenAL::ALContextAttributes;
    using OpenTK::Audio::OpenAL::ALDevice;
    using OpenTK::Audio::OpenAL::ALDistanceModel;
    using OpenTK::Audio::OpenAL::ALError;
    using OpenTK::Audio::OpenAL::ALFormat;
    using OpenTK::Audio::OpenAL::ALGetSourcei;
    using OpenTK::Audio::OpenAL::ALListener3f;
    using OpenTK::Audio::OpenAL::ALListenerfv;
    using OpenTK::Audio::OpenAL::ALSource3f;
    using OpenTK::Audio::OpenAL::ALSourceb;
    using OpenTK::Audio::OpenAL::ALSourcef;
    using OpenTK::Audio::OpenAL::ALSourcei;
    using OpenTK::Audio::OpenAL::AlcError;
    using OpenTK::Audio::OpenAL::BufferLoopPoint;
    using OpenTK::Mathematics::Vector3;

    void AlEs::GenBuffers(std::span<std::int32_t> buffers)
    {
        for (std::size_t i = 0; i < buffers.size(); i++)
        {
            buffers[i] = SfxMixer::NewBuffer();
        }
    }

    std::int32_t AlEs::GenBuffer()
    {
        return SfxMixer::NewBuffer();
    }

    void AlEs::DeleteBuffers(std::span<std::int32_t> buffers)
    {
        for (std::size_t i = 0; i < buffers.size(); i++)
        {
            SfxMixer::DeleteBuffer(buffers[i]);
        }
    }

    void AlEs::DeleteBuffer(std::int32_t buffer)
    {
        SfxMixer::DeleteBuffer(buffer);
    }

    void AlEs::GenSources(std::span<std::int32_t> sources)
    {
        for (std::size_t i = 0; i < sources.size(); i++)
        {
            sources[i] = SfxMixer::NewSource();
        }
    }

    std::int32_t AlEs::GenSource()
    {
        return SfxMixer::NewSource();
    }

    void AlEs::DeleteSources(std::span<std::int32_t> sources)
    {
        for (std::size_t i = 0; i < sources.size(); i++)
        {
            SfxMixer::DeleteSource(sources[i]);
        }
    }

    void AlEs::DeleteSource(std::int32_t source)
    {
        SfxMixer::DeleteSource(source);
    }

    void AlEs::BufferDataBytes(std::int32_t buffer, ALFormat format,
        std::span<const std::uint8_t> data, std::int32_t sampleRate)
    {
        SfxMixer::FillBuffer(buffer, format, data, sampleRate);
    }

    void AlEs::Source(std::int32_t source, ALSourceb param, bool value)
    {
        SfxMixer::SetSource(source, param, value);
    }

    void AlEs::Source(std::int32_t source, ALSourcei param, std::int32_t value)
    {
        SfxMixer::SetSource(source, param, value);
    }

    void AlEs::Source(std::int32_t source, ALSourcef param, float value)
    {
        SfxMixer::SetSource(source, param, value);
    }

    void AlEs::Source(std::int32_t source, ALSource3f param, Vector3& value)
    {
        SfxMixer::SetSource(source, param, value);
    }

    void AlEs::Source(std::int32_t source, ALSource3f param, float x, float y, float z)
    {
        SfxMixer::SetSource(source, param, Vector3(x, y, z));
    }

    void AlEs::SourcePlay(std::int32_t source)
    {
        SfxMixer::Play(source);
    }

    void AlEs::SourceStop(std::int32_t source)
    {
        SfxMixer::StopSource(source);
    }

    void AlEs::SourcePause(std::int32_t source)
    {
        SfxMixer::Pause(source);
    }

    void AlEs::SourceQueueBuffers(std::int32_t source, std::span<std::int32_t> buffers)
    {
        SfxMixer::QueueBuffers(source, buffers);
    }

    void AlEs::SourceUnqueueBuffers(std::int32_t source, std::span<std::int32_t> buffers)
    {
        SfxMixer::UnqueueBuffers(source, buffers);
    }

    void AlEs::GetSource(std::int32_t source, ALGetSourcei param, std::int32_t& value)
    {
        value = SfxMixer::GetSource(source, param);
    }

    std::int32_t AlEs::GetSource(std::int32_t source, ALGetSourcei param)
    {
        return SfxMixer::GetSource(source, param);
    }

    void AlEs::Listener(ALListener3f param, Vector3& value)
    {
        SfxMixer::SetListener(param, value);
    }

    void AlEs::Listener(ALListenerfv param, Vector3& at, Vector3& up)
    {
        static_cast<void>(param);
        SfxMixer::SetListenerOrientation(at, up);
    }

    void AlEs::DistanceModel(ALDistanceModel model)
    {
        static_cast<void>(model);
    }

    ALError AlEs::GetError()
    {
        return ALError::NoError;
    }

    bool AlEs::LoopPoints::IsExtensionPresent()
    {
        return true;
    }

    void AlEs::LoopPoints::Buffer(
        std::int32_t buffer, BufferLoopPoint param, std::int32_t start, std::int32_t end)
    {
        static_cast<void>(param);
        SfxMixer::SetLoopPoints(buffer, start, end);
    }

    const std::intptr_t AlcEs::_handle = 1;

    ALDevice AlcEs::OpenDevice(std::optional<std::string_view> deviceName)
    {
        static_cast<void>(deviceName);
        if (!SfxMixer::Open())
        {
            throw System::DllNotFoundException("no audio device on this machine");
        }
        return ALDevice(_handle);
    }

    ALContext AlcEs::CreateContext(ALDevice device, const ALContextAttributes* attributes)
    {
        static_cast<void>(attributes);
        return device.Handle == 0 ? ALContext::Null : ALContext(_handle);
    }

    bool AlcEs::MakeContextCurrent(ALContext context)
    {
        static_cast<void>(context);
        return true;
    }

    bool AlcEs::DestroyContext(ALContext context)
    {
        static_cast<void>(context);
        return true;
    }

    bool AlcEs::CloseDevice(ALDevice device)
    {
        static_cast<void>(device);
        SfxMixer::Close();
        return true;
    }

    AlcError AlcEs::GetError(ALDevice device)
    {
        static_cast<void>(device);
        return AlcError::NoError;
    }
}
#endif

#include "Player.hpp"

#include "Channel.hpp"
#include "Track.hpp"

#include <memory>
#include <stdexcept>
#include <string>

namespace NCSFPlayer
{
    Player::Player()
    {
        for (auto& track : _tracks)
            track = std::make_shared<Track>();

        for (auto& channel : _channels)
            channel = std::make_shared<Channel>();
    }

    std::span<std::shared_ptr<NCSFCommon::Track>> Player::tracks()
    {
        return _tracks;
    }

    std::span<std::shared_ptr<NCSFCommon::Channel>> Player::channels()
    {
        return _channels;
    }

    std::uint32_t Player::SampleRate() const
    {
        return _sampleRate;
    }

    void Player::SampleRate(std::uint32_t value)
    {
        _sampleRate = value;
    }

    NCSFPlayer::Interpolation Player::Interpolation() const noexcept
    {
        return _interpolation;
    }

    void Player::Interpolation(NCSFPlayer::Interpolation value) noexcept
    {
        _interpolation = value;
    }

    std::uint16_t Player::TrackMutes() const noexcept
    {
        return _trackMutes;
    }

    void Player::TrackMutes(std::uint16_t value) noexcept
    {
        _trackMutes = value;
    }

    std::string Player::PrintTracks() const
    {
        const auto PrintTrack = [this](std::int32_t trackIndex)
        {
            return ((static_cast<std::int32_t>(_trackMutes) >> trackIndex) & 1) ^ 1;
        };

        std::string result;
        for (std::int32_t i = 0; i < TrackCount; ++i)
        {
            result += std::to_string(PrintTrack(i));
            result += ' ';
        }
        return result;
    }

    void Player::SequenceMain()
    {
        for (auto channel : channels())
        {
            if (!channel)
                throw std::runtime_error("NullReferenceException");
            channel->Update();
        }

        Main();
        UpdateChannel();

        for (auto channel : channels())
        {
            if (!channel)
                throw std::runtime_error("NullReferenceException");
            channel->Main();
        }
    }

    void Player::StepTicks()
    {
        for (std::int32_t i = 0; i < TrackCount; ++i)
        {
            auto track = GetTrack(i);
            if (track && track->CurrentPos() != -1)
            {
                if (!track->StepTicks())
                    StopTrack(i);
            }
        }
    }
}

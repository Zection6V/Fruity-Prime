#pragma once

#include "../Player.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace NCSFPlayer
{
    enum class Interpolation : std::int32_t
    {
        None = 0,
        Linear = 1,
        FourPointLagrange = 2,
        SixPointLagrange = 3,
        Sinc = 4,
        SimpleSinc = 5,
        Lanczos = 6
    };

    class Player : public NCSFCommon::Player
    {
    protected:
        [[nodiscard]] std::span<std::shared_ptr<NCSFCommon::Track>> tracks() override;
        [[nodiscard]] std::span<std::shared_ptr<NCSFCommon::Channel>> channels() override;

    public:
        Player();

        [[nodiscard]] std::uint32_t SampleRate() const override;
        void SampleRate(std::uint32_t value) override;

        [[nodiscard]] NCSFPlayer::Interpolation Interpolation() const noexcept;
        void Interpolation(NCSFPlayer::Interpolation value) noexcept;

        [[nodiscard]] std::uint16_t TrackMutes() const noexcept;
        void TrackMutes(std::uint16_t value) noexcept;

        [[nodiscard]] std::string PrintTracks() const;

        void SequenceMain() override;
        void StepTicks() override;

    private:
        std::array<std::shared_ptr<NCSFCommon::Track>, TrackCount> _tracks{};
        std::array<std::shared_ptr<NCSFCommon::Channel>, ChannelCount> _channels{};

        std::uint32_t _sampleRate = 0;
        NCSFPlayer::Interpolation _interpolation = NCSFPlayer::Interpolation::None;
        std::uint16_t _trackMutes = 0;
    };
}

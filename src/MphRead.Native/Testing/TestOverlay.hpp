#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MphRead::Testing
{
    class TestOverlay final
    {
    public:
        static void CompareGames(const std::string& game1, const std::string& game2);
        static void Translate(std::int32_t mask);

        static const std::shared_ptr<std::vector<std::int32_t>> OverlayMap;

        TestOverlay() = delete;
        TestOverlay(const TestOverlay&) = delete;
        TestOverlay& operator=(const TestOverlay&) = delete;
        TestOverlay(TestOverlay&&) = delete;
        TestOverlay& operator=(TestOverlay&&) = delete;

    private:
        static void Nop();
    };

    enum class MphOverlay : std::int32_t
    {
        None = 0x0,
        WiFiPlay = 0x1,
        DownloadPlay = 0x2,
        Bit02 = 0x4,
        VoiceChat = 0x8,
        Bit04 = 0x10,
        Frontend = 0x20,
        DownloadStation = 0x40,
        Movies = 0x80,
        Gameplay = 0x100,
        MpEntities = 0x200,
        SpEnt1Pause = 0x400,
        SpEntities2 = 0x800,
        Enemies = 0x1000,
        BotAi = 0x2000,
        Cretaphid = 0x4000,
        Gorea = 0x8000,
        Slench = 0x10000,
        Bit17 = 0x20000
    };
}

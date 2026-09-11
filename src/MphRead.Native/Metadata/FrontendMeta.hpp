#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MphRead
{
    class ModelMetadata;

    namespace Metadata
    {
        class MovieInfo
        {
        public:
            const std::optional<std::string> TopScreenPath;
            const std::optional<std::string> BottomScreenPath;

            explicit MovieInfo(std::optional<std::string> topScreenPath,
                std::optional<std::string> bottomScreenPath = std::nullopt);
        };

        extern const std::shared_ptr<ModelMetadata> Ad2Dm2;

        extern const std::vector<std::string> NavMapModelNames;

        extern const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> HudModels;
        extern const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> TouchToStartModels;
        extern const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> MultiplayerModels;
        extern const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> LogoModels;
        extern const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> FrontendModels;

        extern const std::vector<std::shared_ptr<MovieInfo>> MovieFiles;
        extern const std::vector<std::string> MovieDisplayInfo;
    }

    enum class Movie : std::int32_t
    {
        None = -1,
        Opening = 0,
        StoryIntro = 1,
        GoodEnding = 2,
        AlinosLanding = 3,
        AlinosTakeoff = 4,
        CALanding = 5,
        CATakeoff = 6,
        ArcterraLanding = 7,
        ArcterraTakeoff = 8,
        VDOLanding = 9,
        VDOTakeoff = 10,
        OublietteUnlock = 11,
        OublietteLanding = 12,
        Unused14TopBot = 13,
        OctolithPickUp = 14,
        CretaphidCA1Intro = 15,
        CretaphidCA1Defeat = 16,
        CretaphidVDO1Intro = 17,
        CretaphidVDO1Defeat = 18,
        CretaphidAlinso2Intro = 19,
        CretaphidAlinos2Defeat = 20,
        CretaphidArcterra2Intro = 21,
        CretaphidArcterra2Defeat = 22,
        SlenchAlinos1Intro = 23,
        SlenchAlinos1Defeat = 24,
        SlenchArcterra1Intro = 25,
        SlenchArcterra1Defeat = 26,
        SlenchCA2Intro = 27,
        SlenchCA2Defeat = 28,
        SlenchVDO2Intro = 29,
        SlenchVDO2Defeat = 30,
        Gorea1Intro = 31,
        BadEndingPart1 = 32,
        Gorea2Intro = 33,
        Unused35TopBot = 34,
        BadEndingPart2 = 35
    };
}

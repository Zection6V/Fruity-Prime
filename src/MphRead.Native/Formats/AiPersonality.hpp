#pragma once

#include "Enums.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace MphRead
{
    enum class GameMode : std::int32_t;
}

namespace MphRead::Formats
{
    class AiPersonalityData1;
    class AiPersonalityData2;
    class AiPersonalityData4;
    class AiPersonalityData5;

    class AiPersonality final
    {
    public:
        struct AiData1
        {
            std::int32_t Field0 = 0;
            std::int32_t Data1Count = 0;
            std::int32_t Data1Offset = 0;
            std::int32_t Data2Count = 0;
            std::int32_t Data2Offset = 0;
            std::int32_t Data3aCount = 0;
            std::int32_t Data3aOffset = 0;
            std::int32_t Data3bCount = 0;
            std::int32_t Data3bOffset = 0;
        };

        struct AiData2
        {
            std::int32_t Data5Type = 0;
            std::int32_t Data4Count = 0;
            std::int32_t Data4Offset = 0;
            std::int32_t FieldC = 0;
            std::int32_t Field10 = 0;
            std::int32_t Data5Offset = 0;
        };

        struct AiData4
        {
            std::int32_t Data5Type = 0;
            std::int32_t Data5Offset = 0;
        };

        static void LoadAll(GameMode mode);
        static void TestRead();

        AiPersonality() = delete;
        AiPersonality(const AiPersonality&) = delete;
        AiPersonality& operator=(const AiPersonality&) = delete;

    private:
        static const std::array<std::array<std::int32_t, 8>, 4> _encounterAiOffsets;
        static std::string _cachedVersion;
        static std::optional<std::vector<std::uint8_t>> _aiPersonalityData;

        static std::unordered_map<std::int32_t,
            std::vector<std::shared_ptr<AiPersonalityData1>>> _data1Cache;
        static std::unordered_map<std::int32_t, std::vector<std::int32_t>> _data3Cache;

        [[nodiscard]] static std::shared_ptr<AiPersonalityData1> LoadData(std::int32_t offset);
        [[nodiscard]] static std::vector<std::shared_ptr<AiPersonalityData1>> ParseData1(
            std::int32_t offset, std::int32_t count);

        static std::unordered_map<std::int32_t,
            std::vector<std::shared_ptr<AiPersonalityData2>>> _data2Cache;

        [[nodiscard]] static std::vector<std::shared_ptr<AiPersonalityData2>> ParseData2(
            std::int32_t offset, std::int32_t count);

        static std::unordered_map<std::int32_t,
            std::vector<std::shared_ptr<AiPersonalityData4>>> _data4Cache;

        [[nodiscard]] static std::vector<std::shared_ptr<AiPersonalityData4>> ParseData4(
            std::int32_t offset, std::int32_t count);

        static const std::shared_ptr<AiPersonalityData5> _emptyParams;
        static std::unordered_map<std::int32_t, std::shared_ptr<AiPersonalityData5>> _data5Cache;

        [[nodiscard]] static std::shared_ptr<AiPersonalityData5> ParseData5(
            std::int32_t type, std::int32_t offset);
    };

    static_assert(sizeof(AiPersonality::AiData1) == 36);
    static_assert(sizeof(AiPersonality::AiData2) == 24);
    static_assert(sizeof(AiPersonality::AiData4) == 8);

    class AiPersonalityData1
    {
    public:
        std::string Label = "?";
        std::int32_t Func24Id = 0;
        std::vector<std::shared_ptr<AiPersonalityData1>> Data1{};
        std::vector<std::shared_ptr<AiPersonalityData2>> Data2{};
        std::vector<std::int32_t> Data3a{};
        std::vector<std::int32_t> Data3b{};
        AiPersonalityData1* Parent = nullptr;

        AiPersonalityData1(std::int32_t field0,
            std::vector<std::shared_ptr<AiPersonalityData1>> data1,
            std::vector<std::shared_ptr<AiPersonalityData2>> data2,
            std::vector<std::int32_t> data3a, std::vector<std::int32_t> data3b);

        void SetLabels();
        void PrintAll();

        static void PrintNode(const std::shared_ptr<AiPersonalityData1>& node,
            std::unordered_set<std::string>& names1, std::unordered_set<std::string>& names2,
            std::unordered_set<std::string>& names3, std::unordered_set<std::string>& names4);
        static void PrintNode(const std::shared_ptr<AiPersonalityData1>& node, std::string& sb);

    private:
        [[nodiscard]] static std::string GetLabel(std::int32_t id);
        static void PrintNode(const AiPersonalityData1* node, std::string* sb,
            std::unordered_set<std::string>* names1, std::unordered_set<std::string>* names2,
            std::unordered_set<std::string>* names3, std::unordered_set<std::string>* names4);
    };

    class AiPersonalityData2
    {
    public:
        std::int32_t Data1SelectIndex = 0;
        std::int32_t Weight = 0;
        std::vector<std::shared_ptr<AiPersonalityData4>> Data4{};
        std::int32_t Func3Id = 0;
        std::shared_ptr<AiPersonalityData5> Parameters{};

        AiPersonalityData2(std::int32_t selIndex, std::int32_t weight,
            std::vector<std::shared_ptr<AiPersonalityData4>> data4,
            std::int32_t fund3Id, std::shared_ptr<AiPersonalityData5> param);
    };

    class AiPersonalityData4
    {
    public:
        std::int32_t Func3Id = 0;
        std::shared_ptr<AiPersonalityData5> Parameters{};

        AiPersonalityData4(std::int32_t data5Type, std::shared_ptr<AiPersonalityData5> data5);
    };

    class AiPersonalityData5
    {
    public:
        std::int32_t Param1 = 0;
        std::int32_t Param2 = 0;
        bool IsEmpty = false;

        AiPersonalityData5();
        AiPersonalityData5(std::int32_t param1, std::int32_t param2);
    };
}

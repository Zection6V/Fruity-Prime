#include "AiPersonality.hpp"

#include "../Entities/Players/PlayerAi.hpp"
#include "../GameState.hpp"
#include "../Read.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <queue>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace MphRead::Paths
{
    // Exact declaration seam for the C# Paths.MphKey property. The Paths owner
    // supplies the storage/accessor; this migration unit only consumes it.
    [[nodiscard]] const std::string& MphKey();
}

namespace
{
    using namespace MphRead;
    using namespace MphRead::Formats;

    using Data1List = std::vector<std::shared_ptr<AiPersonalityData1>>;
    using Data2List = std::vector<std::shared_ptr<AiPersonalityData2>>;
    using Data4List = std::vector<std::shared_ptr<AiPersonalityData4>>;

    struct AiPersonalityState final
    {
        std::string CachedVersion{};
        std::optional<std::vector<std::uint8_t>> Data{};
        std::unordered_map<std::int32_t, Data1List> Data1Cache{};
        std::unordered_map<std::int32_t, std::vector<std::int32_t>> Data3Cache{};
        std::unordered_map<std::int32_t, Data2List> Data2Cache{};
        std::unordered_map<std::int32_t, Data4List> Data4Cache{};
        std::shared_ptr<AiPersonalityData5> EmptyParams = std::make_shared<AiPersonalityData5>();
        std::unordered_map<std::int32_t, std::shared_ptr<AiPersonalityData5>> Data5Cache{};
    };

    [[nodiscard]] AiPersonalityState& State()
    {
        static AiPersonalityState state;
        return state;
    }

    [[nodiscard]] const std::array<std::array<std::int32_t, 8>, 4>& EncounterAiOffsets()
    {
        static const std::array<std::array<std::int32_t, 8>, 4> offsets{{
            {{33152, 33152, 33696, 33836, 33556, 33372, 33976, 13480}},
            {{33152, 33196, 37576, 41948, 35428, 33416, 41492, 13480}},
            {{33152, 33152, 39420, 42772, 33556, 40312, 33976, 13480}},
            {{33152, 33152, 33696, 45176, 33556, 40556, 33976, 13480}}
        }};
        return offsets;
    }

    [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file: " + path);
        }
        const std::streampos end = stream.tellg();
        if (end < 0)
        {
            throw std::ios_base::failure("Could not determine file length: " + path);
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!stream)
            {
                throw std::ios_base::failure("Could not read file: " + path);
            }
        }
        return bytes;
    }

    [[nodiscard]] std::span<const std::uint8_t> PersonalityBytes()
    {
        AiPersonalityState& state = State();
        if (!state.Data.has_value())
        {
            throw System::NullReferenceException();
        }
        return std::span<const std::uint8_t>(*state.Data);
    }

    void DictionaryAdd(std::unordered_map<std::int32_t, Data1List>& values,
        std::int32_t key, Data1List value)
    {
        if (!values.emplace(key, std::move(value)).second)
        {
            throw std::invalid_argument("An item with the same key has already been added.");
        }
    }

    void DictionaryAdd(std::unordered_map<std::int32_t, Data2List>& values,
        std::int32_t key, Data2List value)
    {
        if (!values.emplace(key, std::move(value)).second)
        {
            throw std::invalid_argument("An item with the same key has already been added.");
        }
    }

    void DictionaryAdd(std::unordered_map<std::int32_t, Data4List>& values,
        std::int32_t key, Data4List value)
    {
        if (!values.emplace(key, std::move(value)).second)
        {
            throw std::invalid_argument("An item with the same key has already been added.");
        }
    }

    void DictionaryAdd(std::unordered_map<std::int32_t, std::vector<std::int32_t>>& values,
        std::int32_t key, std::vector<std::int32_t> value)
    {
        if (!values.emplace(key, std::move(value)).second)
        {
            throw std::invalid_argument("An item with the same key has already been added.");
        }
    }

    [[nodiscard]] std::string JoinStrings(const std::vector<std::string>& values)
    {
        std::ostringstream out;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i != 0)
            {
                out << ", ";
            }
            out << values[i];
        }
        return out.str();
    }

    [[nodiscard]] std::string JoinInts(const std::vector<std::int32_t>& values)
    {
        std::ostringstream out;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i != 0)
            {
                out << ", ";
            }
            out << values[i];
        }
        return out.str();
    }

    [[nodiscard]] std::string PadLeft(std::string value, std::size_t width)
    {
        if (value.size() < width)
        {
            value.insert(value.begin(), width - value.size(), ' ');
        }
        return value;
    }

    void DebugWriteLine(const std::string& value)
    {
#ifndef NDEBUG
        std::clog << value << '\n';
#else
        (void)value;
#endif
    }

    [[nodiscard]] std::vector<std::string> Ordered(
        const std::unordered_set<std::string>& values)
    {
        std::vector<std::string> result(values.begin(), values.end());
        std::sort(result.begin(), result.end());
        return result;
    }
}

namespace MphRead::Formats
{
    void AiPersonality::LoadAll(GameMode mode)
    {
        for (std::size_t i = 0; i < Entities::PlayerEntity::Players.size(); ++i)
        {
            std::shared_ptr<Entities::PlayerEntity> player = Entities::PlayerEntity::Players[i];
            player->AiData->Reset();
            if (!player->IsBot)
            {
                continue;
            }

            std::int32_t aiOffset = 32896;
            if (mode == GameMode::SinglePlayer)
            {
                const std::int32_t encounterState = GameState::EncounterState[i];
                if (player->Hunter == Hunter::Guardian)
                {
                    aiOffset = encounterState == 2 ? 32932 : 13480;
                }
                else
                {
                    if (encounterState == 2)
                    {
                        aiOffset = 33232;
                    }
                    else
                    {
                        std::int32_t index = 0;
                        switch (encounterState)
                        {
                        case 1: index = 1; break;
                        case 3: index = 2; break;
                        case 4: index = 3; break;
                        default: index = 0; break;
                        }
                        aiOffset = EncounterAiOffsets().at(static_cast<std::size_t>(index)).at(
                            static_cast<std::size_t>(player->Hunter));
                    }
                    player->AiData->Flags1 = true;
                }
            }
            else if (mode == GameMode::Survival || mode == GameMode::SurvivalTeams)
            {
                aiOffset = 45696;
            }
            else if (mode == GameMode::Capture
                || mode == GameMode::Bounty || mode == GameMode::BountyTeams)
            {
                aiOffset = 32968;
            }
            else if (mode == GameMode::Nodes || mode == GameMode::NodesTeams
                || mode == GameMode::Defender || mode == GameMode::DefenderTeams)
            {
                aiOffset = 33012;
            }
            else if (mode == GameMode::PrimeHunter)
            {
                aiOffset = 45220;
            }
            player->AiData->Personality = LoadData(aiOffset);
        }
    }

    std::shared_ptr<AiPersonalityData1> AiPersonality::LoadData(std::int32_t offset)
    {
        AiPersonalityState& state = State();
        if (Paths::MphKey() != state.CachedVersion)
        {
            state.Data.reset();
            state.Data1Cache.clear();
            state.Data2Cache.clear();
            state.CachedVersion = Paths::MphKey();
        }
        if (!state.Data.has_value())
        {
            state.Data = FileReadAllBytes(Paths::Combine(
                Paths::FileSystem(), "aiPersonalityData\\aiPersonalityData.bin"));
        }

        Data1List data = ParseData1(offset, 1);
        std::shared_ptr<AiPersonalityData1> result = data.at(0);
        result->SetLabels();
        return result;
    }

    std::vector<std::shared_ptr<AiPersonalityData1>> AiPersonality::ParseData1(
        std::int32_t offset, std::int32_t count)
    {
        AiPersonalityState& state = State();
        auto cached = state.Data1Cache.find(offset);
        if (cached != state.Data1Cache.end())
        {
            return cached->second;
        }

        const std::span<const std::uint8_t> bytes = PersonalityBytes();
        Data1List results;
        if (count > 0)
        {
            results.reserve(static_cast<std::size_t>(count));
        }
        const std::shared_ptr<const std::vector<AiData1>> data1s
            = Read::DoOffsets<AiData1>(bytes, offset, count);
        for (const AiData1& data1 : *data1s)
        {
            Data1List data1Children;
            if (data1.Data1Count > 0 && data1.Data1Offset != offset)
            {
                data1Children = ParseData1(data1.Data1Offset, data1.Data1Count);
            }

            Data2List data2;
            if (data1.Data2Count > 0)
            {
                data2 = ParseData2(data1.Data2Offset, data1.Data2Count);
            }

            std::vector<std::int32_t> data3a;
            if (data1.Data3aCount > 0)
            {
                auto cached3a = state.Data3Cache.find(data1.Data3aOffset);
                if (cached3a != state.Data3Cache.end())
                {
                    data3a = cached3a->second;
                }
                else
                {
                    const std::shared_ptr<const std::vector<std::int32_t>> parsed
                        = Read::DoOffsets<std::int32_t>(
                            bytes, data1.Data3aOffset, data1.Data3aCount);
                    data3a.assign(parsed->begin(), parsed->end());
                    DictionaryAdd(state.Data3Cache, data1.Data3aOffset, data3a);
                }
            }

            std::vector<std::int32_t> data3b;
            if (data1.Data3bCount > 0)
            {
                auto cached3b = state.Data3Cache.find(data1.Data3bOffset);
                if (cached3b != state.Data3Cache.end())
                {
                    data3b = cached3b->second;
                }
                else
                {
                    const std::shared_ptr<const std::vector<std::int32_t>> parsed
                        = Read::DoOffsets<std::int32_t>(
                            bytes, data1.Data3bOffset, data1.Data3bCount);
                    data3b.assign(parsed->begin(), parsed->end());
                    DictionaryAdd(state.Data3Cache, data1.Data3bOffset, data3b);
                }
            }

            results.push_back(std::make_shared<AiPersonalityData1>(
                data1.Field0, std::move(data1Children), std::move(data2),
                std::move(data3a), std::move(data3b)));
        }

        Data1List stored = results;
        DictionaryAdd(state.Data1Cache, offset, std::move(stored));
        return results;
    }

    std::vector<std::shared_ptr<AiPersonalityData2>> AiPersonality::ParseData2(
        std::int32_t offset, std::int32_t count)
    {
        AiPersonalityState& state = State();
        auto cached = state.Data2Cache.find(offset);
        if (cached != state.Data2Cache.end())
        {
            return cached->second;
        }

        const std::span<const std::uint8_t> bytes = PersonalityBytes();
        Data2List results;
        if (count > 0)
        {
            results.reserve(static_cast<std::size_t>(count));
        }
        const std::shared_ptr<const std::vector<AiData2>> data2s
            = Read::DoOffsets<AiData2>(bytes, offset, count);
        for (const AiData2& data2 : *data2s)
        {
            Data4List data4;
            if (data2.Data4Count > 0)
            {
                data4 = ParseData4(data2.Data4Offset, data2.Data4Count);
            }

            std::shared_ptr<AiPersonalityData5> data5 = state.EmptyParams;
            if (data2.Data5Offset != 0)
            {
                data5 = ParseData5(data2.Data5Type, data2.Data5Offset);
            }

            results.push_back(std::make_shared<AiPersonalityData2>(
                data2.FieldC, data2.Field10, std::move(data4), data2.Data5Type, std::move(data5)));
        }

        Data2List stored = results;
        DictionaryAdd(state.Data2Cache, offset, std::move(stored));
        return results;
    }

    std::vector<std::shared_ptr<AiPersonalityData4>> AiPersonality::ParseData4(
        std::int32_t offset, std::int32_t count)
    {
        AiPersonalityState& state = State();
        auto cached = state.Data4Cache.find(offset);
        if (cached != state.Data4Cache.end())
        {
            return cached->second;
        }

        const std::span<const std::uint8_t> bytes = PersonalityBytes();
        Data4List results;
        if (count > 0)
        {
            results.reserve(static_cast<std::size_t>(count));
        }
        const std::shared_ptr<const std::vector<AiData4>> data4s
            = Read::DoOffsets<AiData4>(bytes, offset, count);
        for (const AiData4& data4 : *data4s)
        {
            std::shared_ptr<AiPersonalityData5> data5 = state.EmptyParams;
            if (data4.Data5Offset != 0)
            {
                data5 = ParseData5(data4.Data5Type, data4.Data5Offset);
            }
            results.push_back(std::make_shared<AiPersonalityData4>(
                data4.Data5Type, std::move(data5)));
        }

        Data4List stored = results;
        DictionaryAdd(state.Data4Cache, offset, std::move(stored));
        return results;
    }

    std::shared_ptr<AiPersonalityData5> AiPersonality::ParseData5(
        std::int32_t type, std::int32_t offset)
    {
        AiPersonalityState& state = State();
        auto cached = state.Data5Cache.find(offset);
        if (cached != state.Data5Cache.end())
        {
            return cached->second;
        }

        const std::span<const std::uint8_t> bytes = PersonalityBytes();
        const std::int32_t param1 = Read::SpanReadInt(bytes, offset);
        const std::int32_t param2 = type == 210 ? Read::SpanReadInt(bytes, offset + 4) : 0;
        return std::make_shared<AiPersonalityData5>(param1, param2);
    }

    void AiPersonality::TestRead()
    {
        const std::vector<std::uint8_t> storage = FileReadAllBytes(Paths::Combine(
            Paths::FileSystem(), "aiPersonalityData\\aiPersonalityData.bin"));
        const std::span<const std::uint8_t> bytes(storage);
        (void)bytes;

        const std::vector<std::int32_t> offsets{
            13480, 32896, 32932, 32968, 33012, 33152, 33196, 33232, 33372,
            33416, 33556, 33696, 33836, 33976, 35428, 37576, 39420, 40312,
            40556, 41492, 41948, 42772, 45176, 45220, 45696
        };
        std::vector<std::shared_ptr<AiPersonalityData1>> results;
        for (std::int32_t offset : offsets)
        {
            if (offset == 33196)
            {
                results.push_back(LoadData(offset));
                results.back()->PrintAll();
            }
        }
        [[maybe_unused]] const std::int32_t discard1 = 5;
        [[maybe_unused]] const std::int32_t discard2 = 5;
    }

    AiPersonalityData1::AiPersonalityData1(std::int32_t field0,
        std::vector<std::shared_ptr<AiPersonalityData1>> data1,
        std::vector<std::shared_ptr<AiPersonalityData2>> data2,
        std::vector<std::int32_t> data3a, std::vector<std::int32_t> data3b)
        : Func24Id(field0),
          Data1(std::move(data1)),
          Data2(std::move(data2)),
          Data3a(std::move(data3a)),
          Data3b(std::move(data3b))
    {
        for (const std::shared_ptr<AiPersonalityData1>& item : Data1)
        {
            if (!item)
            {
                throw System::NullReferenceException();
            }
            item->Parent = this;
        }
    }

    void AiPersonalityData1::SetLabels()
    {
        std::int32_t id = 0;
        std::queue<AiPersonalityData1*> queue;
        queue.push(this);
        while (!queue.empty())
        {
            std::size_t count = queue.size();
            while (count > 0)
            {
                AiPersonalityData1* node = queue.front();
                queue.pop();
                if (node == nullptr)
                {
                    throw System::NullReferenceException();
                }
                node->Label = GetLabel(id++);
                for (const std::shared_ptr<AiPersonalityData1>& child : node->Data1)
                {
                    queue.push(child.get());
                }
                --count;
            }
        }
    }

    void AiPersonalityData1::PrintAll()
    {
        std::unordered_set<std::string> names1;
        std::unordered_set<std::string> names2;
        std::unordered_set<std::string> names3;
        std::unordered_set<std::string> names4;
        [[maybe_unused]] std::int32_t depth = 0;
        std::queue<std::pair<AiPersonalityData1*, std::int32_t>> queue;
        std::int32_t offset = 0;
        queue.emplace(this, 0);
        while (!queue.empty())
        {
            std::size_t count = queue.size();
            while (count > 0)
            {
                const auto [node, offs] = queue.front();
                queue.pop();
                (void)offs;
                PrintNode(node, nullptr, &names1, &names2, &names3, &names4);
                for (const std::shared_ptr<AiPersonalityData1>& child : node->Data1)
                {
                    queue.emplace(child.get(), offset);
                }
                offset += static_cast<std::int32_t>(node->Data1.size());
                --count;
            }
            if (!queue.empty())
            {
                ++depth;
                offset = 0;
                DebugWriteLine(
                    "-------------------------------------------------------------------------------------------"
                    "-------------------------------------------------------------------------------------------------------");
                DebugWriteLine("");
            }
        }
        DebugWriteLine(JoinStrings(Ordered(names1)));
        DebugWriteLine(JoinStrings(Ordered(names4)));
        DebugWriteLine(JoinStrings(Ordered(names2)));
        DebugWriteLine(JoinStrings(Ordered(names3)));
        [[maybe_unused]] const std::int32_t discard1 = 5;
        [[maybe_unused]] const std::int32_t discard2 = 5;
    }

    std::string AiPersonalityData1::GetLabel(std::int32_t id)
    {
        std::string label = "Root";
        if (--id >= 0)
        {
            label.clear();
            constexpr std::string_view letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            while (id >= 0)
            {
                label.insert(label.begin(), letters[static_cast<std::size_t>(id % 26)]);
                id = id / 26 - 1;
            }
        }
        return label;
    }

    void AiPersonalityData1::PrintNode(const std::shared_ptr<AiPersonalityData1>& node,
        std::unordered_set<std::string>& names1, std::unordered_set<std::string>& names2,
        std::unordered_set<std::string>& names3, std::unordered_set<std::string>& names4)
    {
        PrintNode(node.get(), nullptr, &names1, &names2, &names3, &names4);
    }

    void AiPersonalityData1::PrintNode(
        const std::shared_ptr<AiPersonalityData1>& node, std::string& sb)
    {
        PrintNode(node.get(), &sb, nullptr, nullptr, nullptr, nullptr);
    }

    void AiPersonalityData1::PrintNode(const AiPersonalityData1* node, std::string* sb,
        std::unordered_set<std::string>* names1, std::unordered_set<std::string>* names2,
        std::unordered_set<std::string>* names3, std::unordered_set<std::string>* names4)
    {
        if (node == nullptr)
        {
            throw System::NullReferenceException();
        }

        const auto writeLine = [sb](const std::string& message)
        {
            if (sb != nullptr)
            {
                sb->append(message);
                sb->push_back('\n');
            }
            else
            {
                DebugWriteLine(message);
            }
        };

        writeLine(node->Label);
        std::string d3a = "-";
        std::string d3b = "-";
        std::string f4s = "-";
        std::string f2s = "-";

        if (!node->Data3a.empty())
        {
            const std::vector<std::string> names
                = Entities::PlayerEntity::PlayerAiData::GetFuncs1Names(node->Data3a);
            if (names1 != nullptr)
            {
                for (const std::string& item : names)
                {
                    names1->insert(item);
                }
            }
            d3a = JoinInts(node->Data3a) + " -> " + JoinStrings(names);
        }

        if (!node->Data3b.empty())
        {
            const std::vector<std::string> names
                = Entities::PlayerEntity::PlayerAiData::GetFuncs1Names(node->Data3b);
            if (names1 != nullptr)
            {
                for (const std::string& item : names)
                {
                    names1->insert(item);
                }
            }
            d3b = JoinInts(node->Data3b) + " -> " + JoinStrings(names);
        }

        if (node->Func24Id != 0)
        {
            const std::string name4
                = Entities::PlayerEntity::PlayerAiData::GetFuncs4Name(node->Func24Id);
            const std::string name2
                = Entities::PlayerEntity::PlayerAiData::GetFuncs2Name(node->Func24Id);
            if (names4 != nullptr)
            {
                names4->insert(name4);
            }
            if (names2 != nullptr)
            {
                names2->insert(name2);
            }
            f4s = std::to_string(node->Func24Id) + " -> " + name4;
            f2s = std::to_string(node->Func24Id) + " -> " + name2;
        }

        writeLine("Init (3a): " + d3a);
        writeLine("Init (F4): " + f4s);
        writeLine("Proc (3b): " + d3b);
        writeLine("Proc (F2): " + f2s);

        if (node->Data2.empty())
        {
            writeLine("Switch(x): -");
        }
        else
        {
            const std::size_t pad1 = std::to_string(node->Data2.size() - 1).size();
            std::int32_t maxFunc3Id = std::numeric_limits<std::int32_t>::min();
            std::int32_t maxWeight = std::numeric_limits<std::int32_t>::min();
            for (const std::shared_ptr<AiPersonalityData2>& data2 : node->Data2)
            {
                if (!data2)
                {
                    throw System::NullReferenceException();
                }
                maxFunc3Id = std::max(maxFunc3Id, data2->Func3Id);
                maxWeight = std::max(maxWeight, data2->Weight);
            }
            const std::size_t pad2 = std::to_string(maxFunc3Id).size();
            const std::size_t pad3 = std::to_string(maxWeight).size();

            for (std::size_t i = 0; i < node->Data2.size(); ++i)
            {
                const std::shared_ptr<AiPersonalityData2>& data2 = node->Data2[i];
                const std::string str1 = PadLeft(std::to_string(i), pad1);
                const std::string str2 = PadLeft(std::to_string(data2->Func3Id), pad2);
                const std::string str3 = PadLeft(std::to_string(data2->Weight), pad3);

                if (!data2->Data4.empty())
                {
                    std::vector<std::int32_t> ids;
                    ids.reserve(data2->Data4.size());
                    for (const std::shared_ptr<AiPersonalityData4>& data4 : data2->Data4)
                    {
                        if (!data4)
                        {
                            throw System::NullReferenceException();
                        }
                        ids.push_back(data4->Func3Id);
                    }
                    const std::vector<std::string> names
                        = Entities::PlayerEntity::PlayerAiData::GetFuncs3Names(ids);
                    if (names3 != nullptr)
                    {
                        for (const std::string& item : names)
                        {
                            names3->insert(item);
                        }
                    }
                    const std::string str4 = JoinInts(ids) + " -> " + JoinStrings(names);
                    writeLine("Precon(" + str1 + "): " + str4);
                }

                const std::string name
                    = Entities::PlayerEntity::PlayerAiData::GetFuncs3Name(data2->Func3Id);
                if (names3 != nullptr)
                {
                    names3->insert(name);
                }

                assert(node->Parent != nullptr);
                if (node->Parent == nullptr)
                {
                    throw System::NullReferenceException();
                }

                std::string selection = "-";
                if (data2->Data1SelectIndex < 20)
                {
                    selection = node->Parent->Data1.at(
                        static_cast<std::size_t>(data2->Data1SelectIndex))->Label;
                }
                writeLine("Switch(" + str1 + "): " + str2 + ", " + str3
                    + ", s = " + std::to_string(data2->Data1SelectIndex)
                    + " (" + selection + ") -> " + name);
            }
        }

        if (sb == nullptr)
        {
            writeLine("");
        }
    }

    AiPersonalityData2::AiPersonalityData2(std::int32_t selIndex, std::int32_t weight,
        std::vector<std::shared_ptr<AiPersonalityData4>> data4,
        std::int32_t fund3Id, std::shared_ptr<AiPersonalityData5> param)
        : Data1SelectIndex(selIndex),
          Weight(weight),
          Data4(std::move(data4)),
          Func3Id(fund3Id),
          Parameters(std::move(param))
    {
    }

    AiPersonalityData4::AiPersonalityData4(
        std::int32_t data5Type, std::shared_ptr<AiPersonalityData5> data5)
        : Func3Id(data5Type),
          Parameters(std::move(data5))
    {
    }

    AiPersonalityData5::AiPersonalityData5()
        : IsEmpty(true)
    {
    }

    AiPersonalityData5::AiPersonalityData5(std::int32_t param1, std::int32_t param2)
        : Param1(param1),
          Param2(param2)
    {
    }
}

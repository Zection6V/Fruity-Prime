#include "Metadata.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace MphRead
{
namespace
{
std::string ReplaceAll(std::string value, std::string_view from, std::string_view to)
{
    if (from.empty())
    {
        // C# String.Replace(oldValue, newValue) rejects an empty oldValue.
        throw std::invalid_argument("oldValue");
    }
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos)
    {
        value.replace(position, from.size(), to);
        position += to.size();
    }
    return value;
}

const std::string& DirectoryFor(MetaDir dir)
{
    static const std::map<MetaDir, std::string> dirs =
    {
        {MetaDir::CharSelect, "characterselect"},
        {MetaDir::CreateJoin, "createjoin"},
        {MetaDir::GameOption, "gameoptions"},
        {MetaDir::GamersCard, "gamerscard"},
        {MetaDir::Hud, "hud"},
        {MetaDir::Keyboard, "keyboard"},
        {MetaDir::Keypad, "keypad"},
        {MetaDir::Logo, R"(logo_screen\MAYA)"},
        {MetaDir::MainMenu, "main menu"},
        {MetaDir::Models, "models"},
        {MetaDir::MoviePlayer, "movieplayer"},
        {MetaDir::MultiMaster, "multimaster"},
        {MetaDir::Multiplayer, "multiplayer"},
        {MetaDir::PaxControls, "pax_controls"},
        {MetaDir::Popup, "popup"},
        {MetaDir::Results, "results"},
        {MetaDir::ScStartGame, "sc_startgame"},
        {MetaDir::Stage, "stage"},
        {MetaDir::StartGame, "startgame"},
        {MetaDir::ToStart, "tostart"},
        {MetaDir::TouchToStart, "touchtostart"},
        {MetaDir::TouchToStart2, "touchtostart_2"},
        {MetaDir::WifiCreate, "wifi_createjoin"},
        {MetaDir::WifiGames, "wifi_games"}
    };
    return dirs.at(dir);
}
}

RecolorMetadata::RecolorMetadata(std::string name, std::string modelPath)
    : Name(std::move(name)), ModelPath(std::move(modelPath)),
      TexturePath(ModelPath), PalettePath(ModelPath)
{
}

RecolorMetadata::RecolorMetadata(std::string name, std::string modelPath, std::string texturePath)
    : Name(std::move(name)), ModelPath(std::move(modelPath)),
      TexturePath(std::move(texturePath)), PalettePath(TexturePath)
{
}

RecolorMetadata::RecolorMetadata(std::string name, std::string modelPath, std::string texturePath,
    std::string palettePath, std::map<int, std::vector<int>> replaceIds, bool separateReplace)
    : Name(std::move(name)), ModelPath(std::move(modelPath)), TexturePath(std::move(texturePath)),
      PalettePath(separateReplace ? TexturePath : palettePath),
      ReplacePath(separateReplace ? std::optional<std::string>(std::move(palettePath)) : std::nullopt),
      ReplaceIds(std::move(replaceIds))
{
}

ModelMetadata::ModelMetadata(Values values)
    : Name(std::move(values.Name)), ModelPath(std::move(values.ModelPath)),
      AnimationPath(std::move(values.AnimationPath)), AnimationShare(std::move(values.AnimationShare)),
      CollisionPath(std::move(values.CollisionPath)), ExtraCollisionPath(std::move(values.ExtraCollisionPath)),
      Recolors(std::move(values.Recolors)), UseLightSources(values.UseLightSources), FirstHunt(values.FirstHunt)
{
}

ModelMetadata::ModelMetadata(std::string name, std::string modelPath,
    std::optional<std::string> animationPath, std::optional<std::string> collisionPath,
    std::vector<RecolorMetadata> recolors, std::optional<std::string> animationShare,
    bool useLightSources)
    : ModelMetadata([name = std::move(name), modelPath = std::move(modelPath),
        animationPath = std::move(animationPath), collisionPath = std::move(collisionPath),
        recolors = std::move(recolors), animationShare = std::move(animationShare), useLightSources]() mutable
    {
        Values values;
        values.Name = std::move(name);
        values.ModelPath = std::move(modelPath);
        values.AnimationPath = std::move(animationPath);
        values.AnimationShare = std::move(animationShare);
        values.CollisionPath = std::move(collisionPath);
        values.Recolors = std::move(recolors);
        values.UseLightSources = useLightSources;
        return values;
    }())
{
}

ModelMetadata::ModelMetadata(std::string name, MetaDir dir, std::optional<std::string> anim)
    : ModelMetadata([name = std::move(name), dir, anim = std::move(anim)]() mutable
    {
        Values values;
        values.Name = std::move(name);
        const std::string& directory = DirectoryFor(dir);
        values.ModelPath = directory + "\\" + values.Name + "_Model.bin";
        if (anim)
        {
            values.AnimationPath = directory + "\\" + *anim + "_Anim.bin";
        }
        values.Recolors.emplace_back("default", values.ModelPath, values.ModelPath);
        return values;
    }())
{
}

ModelMetadata::ModelMetadata(std::string name, std::string texturePath, MetaDir dir)
    : ModelMetadata([name = std::move(name), texturePath = std::move(texturePath), dir]() mutable
    {
        Values values;
        values.Name = std::move(name);
        const std::string& directory = DirectoryFor(dir);
        values.ModelPath = directory + "\\" + values.Name + "_Model.bin";
        values.Recolors.emplace_back("default", values.ModelPath, std::move(texturePath));
        return values;
    }())
{
}

ModelMetadata::ModelMetadata(std::string name, std::optional<std::string> animationPath,
    std::optional<std::string> texturePath)
    : ModelMetadata([name = std::move(name), animationPath = std::move(animationPath),
        texturePath = std::move(texturePath)]() mutable
    {
        Values values;
        values.Name = std::move(name);
        values.ModelPath = "models\\" + values.Name + "_Model.bin";
        values.AnimationPath = std::move(animationPath);
        values.Recolors.emplace_back("default", values.ModelPath,
            texturePath ? *texturePath : values.ModelPath);
        return values;
    }())
{
}

ModelMetadata::ModelMetadata(std::string name, std::string remove, bool animation,
    std::optional<std::string> animationPath, bool collision, bool firstHunt)
    : ModelMetadata([name = std::move(name), remove = std::move(remove), animation,
        animationPath = std::move(animationPath), collision, firstHunt]() mutable
    {
        Values values;
        values.Name = std::move(name);
        const std::string directory = "models";
        values.ModelPath = directory + "\\" + values.Name + "_Model.bin";
        const std::string removed = ReplaceAll(values.Name, remove, "");
        if (animation)
        {
            values.AnimationPath = animationPath ? std::move(animationPath)
                : std::optional<std::string>(directory + "\\" + removed + "_Anim.bin");
        }
        if (collision)
        {
            values.CollisionPath = directory + "\\" + removed + "_Collision.bin";
        }
        values.Recolors.emplace_back("default", values.ModelPath);
        values.FirstHunt = firstHunt;
        return values;
    }())
{
}

ModelMetadata::ModelMetadata(std::string name, std::vector<std::string> recolors,
    std::optional<std::string> remove, bool animation, std::optional<std::string> animationPath,
    bool texture, MdlSuffix mdlSuffix, std::optional<std::string> archive,
    std::optional<std::string> recolorName, std::optional<std::string> animationShare,
    bool useLightSources, bool firstHunt, bool noUnderscore)
    : ModelMetadata([name = std::move(name), recolors = std::move(recolors), remove = std::move(remove),
        animation, animationPath = std::move(animationPath), texture, mdlSuffix,
        archive = std::move(archive), recolorName = std::move(recolorName),
        animationShare = std::move(animationShare), useLightSources, firstHunt, noUnderscore]() mutable
    {
        Values values;
        values.Name = std::move(name);
        std::string suffix = mdlSuffix == MdlSuffix::None ? "" : "_mdl";
        if (!archive)
        {
            values.ModelPath = "models\\" + values.Name + suffix + "_Model.bin";
        }
        else
        {
            values.ModelPath = "_archives\\" + *archive + "\\" + values.Name + "_Model.bin";
        }
        const std::string pathName = remove ? ReplaceAll(values.Name, *remove, "") : values.Name;
        if (mdlSuffix != MdlSuffix::All)
        {
            suffix.clear();
        }
        if (animationPath)
        {
            values.AnimationPath = std::move(animationPath);
        }
        else if (animation)
        {
            values.AnimationPath = archive
                ? std::optional<std::string>("_archives\\" + *archive + "\\" + pathName + "_Anim.bin")
                : std::optional<std::string>("models\\" + pathName + suffix + "_Anim.bin");
        }
        values.AnimationShare = std::move(animationShare);
        for (const std::string& recolor : recolors)
        {
            std::string recolorString = (recolorName ? *recolorName : pathName)
                + (noUnderscore ? "" : "_") + recolor;
            if (!recolor.empty() && recolor.front() == '*')
            {
                recolorString = ReplaceAll(recolor, "*", "");
            }
            const std::string recolorModel = "models\\" + recolorString + "_Model.bin";
            const std::string texturePathValue = texture
                ? "models\\" + recolorString + "_Tex.bin" : recolorModel;
            values.Recolors.emplace_back(recolor, recolorModel, texturePathValue);
        }
        values.UseLightSources = useLightSources;
        values.FirstHunt = firstHunt;
        return values;
    }())
{
}

ModelMetadata::ModelMetadata(std::string name, bool animation, bool collision, bool texture,
    std::optional<std::string> share, MdlSuffix mdlSuffix, std::optional<std::string> archive,
    std::optional<std::string> addToAnim, bool firstHunt,
    std::optional<std::string> animationPath, std::optional<std::string> extraCollision)
    : ModelMetadata([name = std::move(name), animation, collision, texture,
        share = std::move(share), mdlSuffix, archive = std::move(archive), addToAnim = std::move(addToAnim),
        firstHunt, animationPath = std::move(animationPath), extraCollision = std::move(extraCollision)]() mutable
    {
        Values values;
        values.Name = std::move(name);
        const std::string path = archive ? "_archives\\" + *archive : "models";
        std::string suffix = mdlSuffix == MdlSuffix::None ? "" : "_mdl";
        values.ModelPath = path + "\\" + values.Name + suffix + "_Model.bin";
        if (mdlSuffix != MdlSuffix::All)
        {
            suffix.clear();
        }
        if (animation)
        {
            values.AnimationPath = animationPath ? std::move(animationPath)
                : std::optional<std::string>(path + "\\" + values.Name + addToAnim.value_or("") + suffix + "_Anim.bin");
        }
        if (collision)
        {
            values.CollisionPath = path + "\\" + values.Name + suffix + "_Collision.bin";
        }
        if (extraCollision)
        {
            values.ExtraCollisionPath = path + "\\" + *extraCollision + "_Collision.bin";
        }
        std::string recolorModel = values.ModelPath;
        if (share)
        {
            texture = false;
            recolorModel = *share;
        }
        values.Recolors.emplace_back("default", recolorModel,
            texture ? "models\\" + values.Name + suffix + "_Tex.bin" : recolorModel);
        values.FirstHunt = firstHunt;
        return values;
    }())
{
}

ModelMetadata::ModelMetadata(std::string name, std::string modelPath,
    std::optional<std::string> animationPath, std::optional<std::string> collisionPath,
    bool firstHunt)
    : ModelMetadata([name = std::move(name), modelPath = std::move(modelPath),
        animationPath = std::move(animationPath), collisionPath = std::move(collisionPath), firstHunt]() mutable
    {
        Values values;
        values.Name = std::move(name);
        values.ModelPath = std::move(modelPath);
        values.AnimationPath = std::move(animationPath);
        values.CollisionPath = std::move(collisionPath);
        values.Recolors.emplace_back("default", values.ModelPath, values.ModelPath);
        values.FirstHunt = firstHunt;
        return values;
    }())
{
}

namespace
{
std::vector<int> NormalizeAnimationIds(std::optional<std::vector<int>> animationIds,
    std::array<int, 4> defaults)
{
    if (!animationIds)
    {
        return {defaults.begin(), defaults.end()};
    }
    if (animationIds->size() != 4)
    {
        throw std::invalid_argument("animationIds");
    }
    return std::move(*animationIds);
}
}

ObjectMetadata::ObjectMetadata(std::string name, bool lighting, int paletteId,
    bool ignoreAnim, std::optional<std::vector<int>> animationIds)
    : Lighting(lighting), IgnoreAnimation(ignoreAnim), Name(std::move(name)),
      AnimationIds(NormalizeAnimationIds(std::move(animationIds), {0, 0, 0, 0})), RecolorId(paletteId)
{
}

PlatformMetadata::PlatformMetadata(std::string name, bool lighting,
    std::optional<std::vector<int>> animationIds)
    : Animation(animationIds.has_value()), Lighting(lighting), Name(std::move(name)),
      AnimationIds(NormalizeAnimationIds(std::move(animationIds), {-1, -1, -1, -1}))
{
}

DoorMetadata::DoorMetadata(std::string name, std::string lockName, float lockOffset, float radius)
    : Name(std::move(name)), LockName(std::move(lockName)), LockOffset(lockOffset), Radius(radius)
{
}
}

namespace MphRead::Metadata
{
namespace
{
OpenTK::Mathematics::Vector3 GetColor(std::uint16_t value) noexcept
{
    return OpenTK::Mathematics::Vector3(
        static_cast<float>((value >> 0) & 0x1F) / 31.0F,
        static_cast<float>((value >> 5) & 0x1F) / 31.0F,
        static_cast<float>((value >> 10) & 0x1F) / 31.0F);
}

const std::array<std::pair<int, std::vector<int>>, 13> ModeLayers{{
    {1, std::vector<int>{0, 1, 2}},
    {2, std::vector<int>{3}},
    {3, std::vector<int>{15}},
    {4, std::vector<int>{15}},
    {5, std::vector<int>{12}},
    {6, std::vector<int>{8, 9, 10}},
    {7, std::vector<int>{11}},
    {8, std::vector<int>{4, 5, 6}},
    {9, std::vector<int>{7}},
    {10, std::vector<int>{14}},
    {11, std::vector<int>{14}},
    {12, std::vector<int>{0, 1, 2}},
    {15, std::vector<int>{13}},
}};

std::string_view ModeName(int value)
{
    switch (value)
    {
    case 1: return "Battle";
    case 2: return "BattleTeams";
    case 3: return "Survival";
    case 4: return "SurvivalTeams";
    case 5: return "Capture";
    case 6: return "Bounty";
    case 7: return "BountyTeams";
    case 8: return "Nodes";
    case 9: return "NodesTeams";
    case 10: return "Defender";
    case 11: return "DefenderTeams";
    case 12: return "PrimeHunter";
    case 15: return "Unknown15";
    default: return "";
    }
}

const std::vector<int>& LayersFor(GameMode mode)
{
    const int value = static_cast<int>(mode);
    for (const auto& item : ModeLayers)
    {
        if (item.first == value)
        {
            return item.second;
        }
    }
    throw std::out_of_range("mode");
}

const ::MphRead::ModelMetadata* FindModel(
    const std::unordered_map<std::string, ::MphRead::ModelMetadata>& values,
    std::string_view name) noexcept
{
    const auto it = values.find(std::string(name));
    return it == values.end() ? nullptr : &it->second;
}

const ::MphRead::ModelMetadata* FindFrontendModel(
    const std::unordered_map<std::string, std::shared_ptr<::MphRead::ModelMetadata>>& values,
    std::string_view name) noexcept
{
    auto it = values.find(std::string(name));
    return it == values.end() ? nullptr : it->second.get();
}

const std::array<std::shared_ptr<ObjectMetadata>, 54> Objects = []
{
    std::array<std::shared_ptr<ObjectMetadata>, 54> values{};
    auto make = [](std::string name, bool lighting = false, int palette = 0,
        bool ignore = false, std::optional<std::vector<int>> ids = std::nullopt)
    {
        return std::make_shared<ObjectMetadata>(std::move(name), lighting, palette, ignore, std::move(ids));
    };
    values[0]=make("AlimbicGhost_01"); values[1]=make("AlimbicLightPole");
    values[2]=make("AlimbicStationShieldControl"); values[3]=make("AlimbicComputerStationControl");
    values[4]=make("AlimbicEnergySensor"); values[5]=make("SamusShip");
    values[6]=make("Guardbot01_Dead"); values[7]=make("Guardbot02_Dead");
    values[8]=make("Guardian_Dead"); values[9]=make("Psychobit_Dead");
    values[10]=make("AlimbicLightPole02"); values[11]=make("AlimbicComputerStationControl02");
    values[12]=make("Generic_Console",false,0,false,std::vector<int>{2,1,0,0});
    values[13]=make("Generic_Monitor",false,0,false,std::vector<int>{2,1,0,0});
    values[14]=make("Generic_Power"); values[15]=make("Generic_Scanner",false,0,false,std::vector<int>{2,1,0,0});
    values[16]=make("Generic_Switch",true,0,false,std::vector<int>{2,1,0,0});
    values[17]=make("Alimbic_Console",false,0,false,std::vector<int>{2,1,0,0});
    values[18]=make("Alimbic_Monitor",false,0,false,std::vector<int>{2,1,0,0});
    values[19]=make("Alimbic_Power"); values[20]=make("Alimbic_Scanner",false,0,false,std::vector<int>{2,1,0,0});
    values[21]=make("Alimbic_Switch",true,0,false,std::vector<int>{2,1,0,0});
    values[22]=make("Lava_Console",false,0,false,std::vector<int>{2,1,0,0});
    values[23]=make("Lava_Monitor",false,0,false,std::vector<int>{2,1,0,0}); values[24]=make("Lava_Power");
    values[25]=make("Lava_Scanner",false,0,false,std::vector<int>{2,1,0,0});
    values[26]=make("Lava_Switch",true,0,false,std::vector<int>{2,1,0,0});
    values[27]=make("Ice_Console",false,0,false,std::vector<int>{2,1,0,0});
    values[28]=make("Ice_Monitor",false,0,false,std::vector<int>{2,1,0,0}); values[29]=make("Ice_Power");
    values[30]=make("Ice_Scanner",false,0,false,std::vector<int>{2,1,0,0});
    values[31]=make("Ice_Switch",true,0,false,std::vector<int>{2,1,0,0});
    values[32]=make("Ruins_Console",false,0,false,std::vector<int>{2,1,0,0});
    values[33]=make("Ruins_Monitor",false,0,false,std::vector<int>{2,1,0,0}); values[34]=make("Ruins_Power");
    values[35]=make("Ruins_Scanner",false,0,false,std::vector<int>{2,1,0,0});
    values[36]=make("Ruins_Switch",true,0,false,std::vector<int>{2,1,0,0});
    values[37]=make("PlantCarnivarous_Branched"); values[38]=make("PlantCarnivarous_Pod");
    values[39]=make("PlantCarnivarous_PodLeaves"); values[40]=make("PlantCarnivarous_Vine");
    values[41]=make("GhostSwitch"); values[42]=make("Switch",true);
    values[43]=make("Guardian_Stasis",false,0,false,std::vector<int>{-1,0,0,0});
    values[44]=make("AlimbicStatue_lod0",false,0,true,std::vector<int>{-1,0,0,0});
    values[45]=make("AlimbicCapsule");
    values[46]=make("SniperTarget",true,0,false,std::vector<int>{0,2,1,0});
    for (int i=1;i<=6;i++) values[46+i]=make("SecretSwitch",false,i,false,std::vector<int>{1,2,0,0});
    values[53]=make("WallSwitch",true,0,false,std::vector<int>{2,0,1,0});
    return values;
}();

const std::array<std::shared_ptr<PlatformMetadata>, 45> Platforms = []
{
    std::array<std::shared_ptr<PlatformMetadata>,45> values{};
    auto make=[](std::string name, bool lighting=false, std::optional<std::vector<int>> ids=std::nullopt)
    { return std::make_shared<PlatformMetadata>(std::move(name),lighting,std::move(ids)); };
    values[0]=make("platform"); values[1]=values[0];
    values[3]=make("Elevator"); values[4]=make("smasher"); values[5]=make("Platform_Unit4_C1",true);
    values[6]=make("pillar"); values[7]=make("Door_Unit4_RM1");
    values[8]=make("SyluxShip",false,std::vector<int>{-1,1,0,2}); values[9]=make("pistonmp7");
    values[10]=make("unit3_brain",false,std::vector<int>{0,0,0,0});
    values[11]=make("unit4_mover1",false,std::vector<int>{0,0,0,0});
    values[12]=make("unit4_mover2",false,std::vector<int>{0,0,0,0});
    values[13]=make("ElectroField1",false,std::vector<int>{0,0,0,0}); values[14]=make("Unit3_platform1");
    values[15]=make("unit3_pipe1",false,std::vector<int>{0,0,0,0}); values[16]=make("unit3_pipe2",false,std::vector<int>{0,0,0,0});
    values[17]=make("cylinderbase"); values[18]=make("unit3_platform"); values[19]=make("unit3_platform2");
    values[20]=make("unit3_jar",false,std::vector<int>{0,2,1,0}); values[21]=make("SyluxTurret",false,std::vector<int>{3,2,1,0});
    values[22]=make("unit3_jartop",false,std::vector<int>{0,2,1,0}); values[23]=make("SamusShip",false,std::vector<int>{1,3,2,4});
    values[24]=make("unit1_land_plat1"); values[25]=make("unit1_land_plat2"); values[26]=make("unit1_land_plat3");
    values[27]=make("unit1_land_plat4"); values[28]=make("unit1_land_plat5"); values[29]=make("unit2_c4_plat");
    values[30]=make("unit2_land_elev"); values[31]=make("unit4_platform1");
    values[32]=make("Crate01",false,std::vector<int>{-1,-1,0,1});
    values[33]=make("unit1_mover1",false,std::vector<int>{0,0,0,0}); values[34]=make("unit1_mover2");
    values[35]=make("unit2_mover1"); values[36]=make("unit4_mover3"); values[37]=make("unit4_mover4"); values[38]=make("unit3_mover1");
    values[39]=make("unit2_c1_mover"); values[40]=make("unit3_mover2",false,std::vector<int>{0,0,0,0});
    values[41]=make("piston_gorealand"); values[42]=make("unit4_tp2_artifact_wo"); values[43]=make("unit4_tp1_artifact_wo");
    values[44]=make("SamusShip",false,std::vector<int>{1,0,2,4});
    return values;
}();
}

int GetMultiplayerEntityLayer(GameMode mode, int playerCount)
{
    const std::vector<int>& list = LayersFor(mode);
    if (list.size() == 1)
    {
        return list[0];
    }
    const int index = playerCount == 3 ? 1 : playerCount == 4 ? 2 : 0;
    return list.at(static_cast<std::size_t>(index));
}

std::string GetLayerName(int layerId, bool multiplayer)
{
    if (multiplayer)
    {
        const std::uint32_t bit = std::uint32_t{1} << (static_cast<std::uint32_t>(layerId) & 31U);
        return GetLayerNames(std::bit_cast<std::int32_t>(bit), true);
    }
    switch (layerId)
    {
    case 0: return "FirstVisit";
    case 1: return "Escape";
    case 2: return "Cleared";
    case 3: return "SpLayer3";
    default: return "NoLayer" + std::to_string(layerId);
    }
}

std::string GetLayerNames(int layerMask, bool multiplayer)
{
    if (!multiplayer)
    {
        switch (layerMask & 3)
        {
        case 0: return "FirstVisit";
        case 1: return "Escape";
        case 2: return "Cleared";
        case 3: return "SpLayer3";
        default: return "UNKNOWN" + std::to_string(layerMask);
        }
    }
    std::vector<int> layers;
    for (int i = 0; i < 16; ++i)
    {
        if ((static_cast<std::uint32_t>(layerMask) & (std::uint32_t{1} << i)) != 0)
        {
            layers.push_back(i);
        }
    }
    std::vector<std::string> names;
    for (const auto& item : ModeLayers)
    {
        const bool all = std::all_of(item.second.begin(), item.second.end(),
            [&](int value){ return std::find(layers.begin(), layers.end(), value) != layers.end(); });
        const std::string modeName(ModeName(item.first));
        if (all)
        {
            names.push_back(modeName);
        }
        else if (item.second.size() > 1)
        {
            std::vector<std::string> players;
            if (std::find(layers.begin(), layers.end(), item.second[0]) != layers.end()) players.emplace_back("2P");
            if (std::find(layers.begin(), layers.end(), item.second[1]) != layers.end()) players.emplace_back("3P");
            if (std::find(layers.begin(), layers.end(), item.second[2]) != layers.end()) players.emplace_back("4P");
            if (!players.empty())
            {
                std::string value = modeName;
                for (std::size_t i=0;i<players.size();++i)
                {
                    if (i != 0) value += '/';
                    value += players[i];
                }
                names.push_back(std::move(value));
            }
        }
    }
    std::string result;
    for (std::size_t i=0;i<names.size();++i)
    {
        if (i != 0) result += " | ";
        result += names[i];
    }
    return result;
}

const OpenTK::Mathematics::Vector3 EmissionOrange = GetColor(0x14F0);
const OpenTK::Mathematics::Vector3 EmissionGreen = GetColor(0x1565);
const OpenTK::Mathematics::Vector3 EmissionGray = GetColor(0x35AD);
const std::array<ColorRgb,2> TeamColors{{ColorRgb(31,19,0),ColorRgb(0,31,0)}};
const OpenTK::Mathematics::Vector3 OctolithLight1Vector(0.0F,0.3005371F,-0.5F);
const OpenTK::Mathematics::Vector3 OctolithLight2Vector(0.0F,0.0F,-0.5F);
const OpenTK::Mathematics::Vector3 OctolithLightColor(1.0F,1.0F,1.0F);
const std::array<OpenTK::Mathematics::Vector3,32> ToonTable{{
    GetColor(0x2000),
    GetColor(0x2000),
    GetColor(0x2020),
    GetColor(0x2021),
    GetColor(0x2021),
    GetColor(0x2041),
    GetColor(0x2441),
    GetColor(0x2461),
    GetColor(0x2461),
    GetColor(0x2462),
    GetColor(0x2482),
    GetColor(0x2482),
    GetColor(0x28C3),
    GetColor(0x2CE4),
    GetColor(0x3105),
    GetColor(0x3546),
    GetColor(0x3967),
    GetColor(0x3D88),
    GetColor(0x41C9),
    GetColor(0x45EA),
    GetColor(0x4A0B),
    GetColor(0x4E4B),
    GetColor(0x526C),
    GetColor(0x568D),
    GetColor(0x5ACE),
    GetColor(0x5EEF),
    GetColor(0x6310),
    GetColor(0x6751),
    GetColor(0x6B72),
    GetColor(0x6F93),
    GetColor(0x73D4),
    GetColor(0x77F5),
}};

const std::unordered_map<std::string, std::vector<PaletteData>> PowerPalettes{
    {R"(Alimbic_Power)", std::vector<PaletteData>{PaletteData(32576), PaletteData(32576), PaletteData(32608), PaletteData(32640), PaletteData(32711), PaletteData(32719), PaletteData(32758), PaletteData(32733)}},
    {R"(Generic_Power)", std::vector<PaletteData>{PaletteData(19393), PaletteData(18369), PaletteData(17345), PaletteData(16321), PaletteData(19400), PaletteData(23535), PaletteData(26614), PaletteData(31741)}},
    {R"(Ice_Power)", std::vector<PaletteData>{PaletteData(29453), PaletteData(29453), PaletteData(29485), PaletteData(29517), PaletteData(30578), PaletteData(30614), PaletteData(31705), PaletteData(32734)}},
    {R"(Lava_Power)", std::vector<PaletteData>{PaletteData(671), PaletteData(639), PaletteData(607), PaletteData(575), PaletteData(7807), PaletteData(16127), PaletteData(23391), PaletteData(30719)}},
};

const std::unordered_map<Hunter,float> HunterScales{
    {Hunter::Samus,1.0F},{Hunter::Kanden,static_cast<float>(0x10F5)/4096.0F},
    {Hunter::Trace,1.0F},{Hunter::Sylux,1.0F},{Hunter::Noxus,1.0F},
    {Hunter::Spire,static_cast<float>(0x123D)/4096.0F},{Hunter::Weavel,1.0F},{Hunter::Guardian,1.0F}
};
const std::unordered_map<Hunter,std::array<std::string,4>> HunterModels{
    {Hunter::Samus,{"Samus_lod0","Samus_lod1","SamusAlt_lod0","SamusGun"}},
    {Hunter::Kanden,{"Kanden_lod0","Kanden_lod1","KandenAlt_lod0","KandenGun"}},
    {Hunter::Trace,{"Trace_lod0","Trace_lod1","TraceAlt_lod0","TraceGun"}},
    {Hunter::Sylux,{"Sylux_lod0","Sylux_lod1","SyluxAlt_lod0","SyluxGun"}},
    {Hunter::Noxus,{"Nox_lod0","Nox_lod1","NoxAlt_lod0","NoxGun"}},
    {Hunter::Spire,{"Spire_lod0","Spire_lod1","SpireAlt_lod0","SpireGun"}},
    {Hunter::Weavel,{"Weavel_lod0","Weavel_lod1","WeavelAlt_lod0","WeavelGun"}},
    {Hunter::Guardian,{"Guardian_lod0","Guardian_lod1","SamusAlt_lod0","SamusGun"}}
};
const std::array<int,89> AdpcmTable{{7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767}};
const std::array<int,16> ImaIndexTable{{-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8}};
const std::array<std::string, 60> MusicSeqs{{
    R"(SEQ_BRINSTAR)",
    R"(SEQ_MP1)",
    R"(SEQ_MP2)",
    R"(SEQ_PARASITE)",
    R"(SEQ_SHIP)",
    R"(SEQ_YELLOW)",
    R"(SEQ_RESULTS)",
    R"(SEQ_TIMEOUT)",
    R"(SEQ_WIN)",
    R"(SEQ_GARLIC)",
    R"(SEQ_MP2_X)",
    R"(SEQ_PARASITE_X)",
    R"(SEQ_RED)",
    R"(SEQ_BLUE)",
    R"(SEQ_AMBIENT_1)",
    R"(SEQ_TELEPORT)",
    R"(SEQ_DRONE)",
    R"(SEQ_MENU1)",
    R"(SEQ_GREY)",
    R"(SEQ_SAFFRON)",
    R"(SEQ_GUMBO)",
    R"(SEQ_INTRO_SYLUX)",
    R"(SEQ_INTRO_TRACE)",
    R"(SEQ_INTRO_NOXUS)",
    R"(SEQ_INTRO_WEAVEL)",
    R"(SEQ_INTRO_KANDEN)",
    R"(SEQ_INTRO_SPIRE)",
    R"(SEQ_FLY_IN_2)",
    R"(SEQ_FLY_IN_1)",
    R"(SEQ_FLY_IN_3)",
    R"(SEQ_FLY_IN_4)",
    R"(SEQ_SHIP_LAND1)",
    R"(SEQ_SHIP_LAND2)",
    R"(SEQ_SHIP_LAND3)",
    R"(SEQ_SHIP_LAND4)",
    R"(SEQ_GET_WEAPON)",
    R"(SEQ_GET_OCTOLITH)",
    R"(SEQ_NEW_GAME)",
    R"(SEQ_BEAT_HUNTER1)",
    R"(SEQ_INTRO_GUARDIAN)",
    R"(SEQ_GUARDIAN)",
    R"(SEQ_BEAT_CYLBOSS1)",
    R"(SEQ_GREEN)",
    R"(SEQ_CHUTNEY)",
    R"(SEQ_DILL)",
    R"(SEQ_GOREA_1)",
    R"(SEQ_ENEMY_1)",
    R"(SEQ_GOREA_2)",
    R"(SEQ_PEPPER)",
    R"(SEQ_SINGLE_CART_MENU)",
    R"(SEQ_SINGLE_CART_INGAME)",
    R"(SEQ_SINGLE_CART_TIMEOUT)",
    R"(SEQ_OREGANO)",
    R"(SEQ_ENEMY_2)",
    R"(SEQ_WHITE)",
    R"(SEQ_ENERGY_TIMER)",
    R"(SEQ_BLACK)",
    R"(SEQ_INDIGO)",
    R"(SEQ_CREDITS)",
    R"(SEQ_FLY_IN_GOREA)",
}};
const std::array<float,3> DamageLevels{{0.75F,1.0F,1.25F}};
const std::array<DoorMetadata,4> Doors{{
    DoorMetadata("AlimbicDoor","AlimbicDoorLock",1.4F,2.4F),
    DoorMetadata("AlimbicMorphBallDoor","AlimbicMorphBallDoorLock",0.7F,1.0F),
    DoorMetadata("AlimbicBossDoor","AlimbicBossDoorLock",3.5F,3.5F),
    DoorMetadata("AlimbicThinDoor","ThinDoorLock",1.4F,2.0F)
}};
const std::array<std::string, 3> FhDoors{{
    R"(door)",
    R"(door2)",
    R"(door2_holo)",
}};
const std::array<int,10> DoorPalettes{{0,1,2,7,6,3,4,5,0,0}};
const std::array<std::string, 6> JumpPads{{
    R"(JumpPad)",
    R"(JumpPad_Alimbic)",
    R"(JumpPad_Ice)",
    R"(JumpPad_IceStation)",
    R"(JumpPad_Lava)",
    R"(JumpPad_Station)",
}};
const std::array<std::string, 23> Items{{
    R"(pick_health_B)",
    R"(pick_health_A)",
    R"(pick_health_C)",
    R"(pick_dblDamage)",
    R"(PickUp_EnergyExp)",
    R"(pick_wpn_electro)",
    R"(PickUp_MissileExp)",
    R"(pick_wpn_jackhammer)",
    R"(pick_wpn_snipergun)",
    R"(pick_wpn_shotgun)",
    R"(pick_wpn_mortar)",
    R"(pick_wpn_ghostbuster)",
    R"(pick_wpn_gorea)",
    R"(pick_ammo_green)",
    R"(pick_ammo_green)",
    R"(pick_ammo_orange)",
    R"(pick_ammo_orange)",
    R"(pick_invis)",
    R"(PickUp_AmmoExp)",
    R"(Artifact_Key)",
    R"(pick_deathball)",
    R"(pick_wpn_all)",
    R"(pick_wpn_missile)",
}};
const std::array<std::string, 8> FhItems{{
    R"(pick_ammo_A)",
    R"(pick_ammo_B)",
    R"(pick_health_A)",
    R"(pick_health_B)",
    R"(pick_dblDamage)",
    R"(pick_morphball)",
    R"(pick_wpn_electro)",
    R"(pick_wpn_missile)",
}};
std::array<OpenTK::Mathematics::Vector3,54> ObjectVisPosOffsets{{
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(-0.05F,1.5F,-0.4F),
    OpenTK::Mathematics::Vector3(-0.05F,1.5F,-0.4F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(-0.05F,1.5F,0.0F),
    OpenTK::Mathematics::Vector3(0.01F,1.5F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(-0.03F,1.5F,-0.3F),
    OpenTK::Mathematics::Vector3(0.0F,1.5F,-0.3F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,1.5F,-0.2F),
    OpenTK::Mathematics::Vector3(0.0F,1.5F,-0.2F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,1.4F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,1.4F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,1.75F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
    OpenTK::Mathematics::Vector3(0.0F,0.0F,0.0F),
}};
const PlatformMetadata InvisiblePlat("N/A");
const std::array<std::string, 11> WeaponNames{{
    R"(Power Beam)",
    R"(Volt Driver)",
    R"(Missiles)",
    R"(Battlehammer)",
    R"(Imperialist)",
    R"(Judicator)",
    R"(Magmaul)",
    R"(Shock Coil)",
    R"(Omega Cannon)",
    R"(Platform)",
    R"(Enemy)",
}};
const std::array<std::string, 11> WeaponNamesUpper{{
    R"(POWER BEAM)",
    R"(VOLT DRIVER)",
    R"(MISSILES)",
    R"(BATTLEHAMMER)",
    R"(IMPERIALIST)",
    R"(JUDICATOR)",
    R"(MAGMAUL)",
    R"(SHOCK COIL)",
    R"(OMEGA CANNON)",
    R"(PLATFORM)",
    R"(ENEMY)",
}};
const std::array<int,11> WeaponMessageIds{{0,109,0,110,111,112,113,114,115,0,0}};
const std::array<std::pair<std::string,std::optional<std::string>>,247> Effects{{
    {"", std::nullopt},
    {R"(powerBeam)", std::optional<std::string>{R"(effects)"}},
    {R"(powerBeamNoSplat)", std::optional<std::string>{R"(effects)"}},
    {R"(blastCapHit)", std::nullopt},
    {R"(blastCapBlow)", std::nullopt},
    {R"(missile1)", std::optional<std::string>{R"(effects)"}},
    {R"(mortar1)", std::optional<std::string>{R"(effects)"}},
    {R"(shotGunCol)", std::optional<std::string>{R"(effects)"}},
    {R"(shotGunShrapnel)", std::optional<std::string>{R"(effects)"}},
    {R"(bombStart)", std::nullopt},
    {R"(ballDeath)", std::nullopt},
    {R"(jackHammerCol)", std::optional<std::string>{R"(effects)"}},
    {R"(effectiveHitPB)", std::nullopt},
    {R"(effectiveHitElectric)", std::nullopt},
    {R"(effectiveHitMsl)", std::nullopt},
    {R"(effectiveHitJack)", std::nullopt},
    {R"(effectiveHitSniper)", std::nullopt},
    {R"(effectiveHitIce)", std::nullopt},
    {R"(effectiveHitMortar)", std::nullopt},
    {R"(effectiveHitGhost)", std::nullopt},
    {R"(sprEffectivePB)", std::nullopt},
    {R"(sprEffectiveElectric)", std::nullopt},
    {R"(sprEffectiveMsl)", std::nullopt},
    {R"(sprEffectiveJack)", std::nullopt},
    {R"(sprEffectiveSniper)", std::nullopt},
    {R"(sprEffectiveIce)", std::nullopt},
    {R"(sprEffectiveMortar)", std::nullopt},
    {R"(sprEffectiveGhost)", std::nullopt},
    {R"(sniperCol)", std::optional<std::string>{R"(effects)"}},
    {R"(shriekBatTrail)", std::nullopt},
    {R"(samusFurl)", std::nullopt},
    {R"(spawnEffect)", std::nullopt},
    {R"(test)", std::nullopt},
    {R"(spawnEffectMP)", std::nullopt},
    {R"(burstFlame)", std::nullopt},
    {R"(gunSmoke)", std::nullopt},
    {R"(jetFlame)", std::nullopt},
    {R"(spireAltSlam)", std::nullopt},
    {R"(steamBurst)", std::nullopt},
    {R"(steamSamusShip)", std::nullopt},
    {R"(steamDoorway)", std::nullopt},
    {R"(goreaArmChargeUp)", std::nullopt},
    {R"(goreaBallExplode)", std::nullopt},
    {R"(goreaShoulderDamageLoop)", std::nullopt},
    {R"(goreaShoulderHits)", std::nullopt},
    {R"(goreaShoulderKill)", std::nullopt},
    {R"(goreaChargeElc)", std::nullopt},
    {R"(goreaChargeIce)", std::nullopt},
    {R"(goreaChargeJak)", std::nullopt},
    {R"(goreaChargeMrt)", std::nullopt},
    {R"(goreaChargeSnp)", std::nullopt},
    {R"(goreaFireElc)", std::nullopt},
    {R"(goreaFireGst)", std::nullopt},
    {R"(goreaFireIce)", std::nullopt},
    {R"(goreaFireJak)", std::nullopt},
    {R"(goreaFireMrt)", std::nullopt},
    {R"(goreaFireSnp)", std::nullopt},
    {R"(muzzleElc)", std::nullopt},
    {R"(muzzleGst)", std::nullopt},
    {R"(muzzleIce)", std::nullopt},
    {R"(muzzleJak)", std::nullopt},
    {R"(muzzleMrt)", std::nullopt},
    {R"(muzzlePB)", std::nullopt},
    {R"(muzzleSnp)", std::nullopt},
    {R"(tear)", std::nullopt},
    {R"(cylCrystalCharge)", std::nullopt},
    {R"(cylCrystalKill)", std::nullopt},
    {R"(cylCrystalShot)", std::nullopt},
    {R"(tearSplat)", std::nullopt},
    {R"(eyeShieldCharge)", std::nullopt},
    {R"(eyeShieldHit)", std::nullopt},
    {R"(goreaSlam)", std::nullopt},
    {R"(goreaBallExplode2)", std::nullopt},
    {R"(cylCrystalKill2)", std::nullopt},
    {R"(cylCrystalKill3)", std::nullopt},
    {R"(goreaCrystalExplode)", std::nullopt},
    {R"(DeathBio1)", std::nullopt},
    {R"(DeathMech1)", std::nullopt},
    {R"(iceWave)", std::nullopt},
    {R"(goreaMeteor)", std::nullopt},
    {R"(goreaTeleport)", std::nullopt},
    {R"(tearChargeUp)", std::nullopt},
    {R"(eyeShield)", std::nullopt},
    {R"(eyeShieldDefeat)", std::nullopt},
    {R"(grateSparks)", std::nullopt},
    {R"(electroCharge)", std::nullopt},
    {R"(electroHit)", std::nullopt},
    {R"(torch)", std::nullopt},
    {R"(jetFlameBlue)", std::nullopt},
    {R"(lavaBurstLarge)", std::nullopt},
    {R"(lavaBurstSmall)", std::nullopt},
    {R"(ember)", std::nullopt},
    {R"(powerBeamCharge)", std::nullopt},
    {R"(lavaDemonDive)", std::nullopt},
    {R"(lavaDemonHurl)", std::nullopt},
    {R"(lavaDemonRise)", std::nullopt},
    {R"(iceDemonHurl)", std::nullopt},
    {R"(lavaBurstExtraLarge)", std::nullopt},
    {R"(powerBeamChargeNoSplat)", std::nullopt},
    {R"(powerBeamHolo)", std::nullopt},
    {R"(powerBeamLava)", std::nullopt},
    {R"(hangingDrip)", std::nullopt},
    {R"(hangingSpit)", std::nullopt},
    {R"(hangingSplash)", std::nullopt},
    {R"(goreaEyeFlash)", std::nullopt},
    {R"(smokeBurst)", std::nullopt},
    {R"(sparks)", std::nullopt},
    {R"(sparksFall)", std::nullopt},
    {R"(shriekBatCol)", std::nullopt},
    {R"(eyeTurretCharge)", std::nullopt},
    {R"(lavaDemonSplat)", std::nullopt},
    {R"(tearDrips)", std::nullopt},
    {R"(syluxShipExhaust)", std::nullopt},
    {R"(bombStartSylux)", std::nullopt},
    {R"(lockDefeat)", std::nullopt},
    {R"(ineffectivePsycho)", std::nullopt},
    {R"(cylCrystalProjectile)", std::nullopt},
    {R"(cylWeakSpotShot)", std::nullopt},
    {R"(eyeLaser)", std::nullopt},
    {R"(bombStartMP)", std::nullopt},
    {R"(enemyMslCol)", std::nullopt},
    {R"(powerBeamHoloBG)", std::nullopt},
    {R"(powerBeamHoloB)", std::nullopt},
    {R"(powerBeamIce)", std::nullopt},
    {R"(powerBeamRock)", std::nullopt},
    {R"(powerBeamSand)", std::nullopt},
    {R"(powerBeamSnow)", std::nullopt},
    {R"(bubblesRising)", std::nullopt},
    {R"(bombKanden)", std::nullopt},
    {R"(collapsingStreaks)", std::nullopt},
    {R"(fireProjectile)", std::nullopt},
    {R"(iceDemonSplat)", std::nullopt},
    {R"(iceDemonRise)", std::nullopt},
    {R"(iceDemonDive)", std::nullopt},
    {R"(hammerProjectile)", std::nullopt},
    {R"(synapseKill)", std::nullopt},
    {R"(samusDash)", std::nullopt},
    {R"(electroProjectile)", std::nullopt},
    {R"(cylHomingProjectile)", std::nullopt},
    {R"(cylHomingKill)", std::nullopt},
    {R"(energyRippleB)", std::nullopt},
    {R"(energyRippleBG)", std::nullopt},
    {R"(energyRippleO)", std::nullopt},
    {R"(columnCrash)", std::nullopt},
    {R"(artifactKeyEffect)", std::nullopt},
    {R"(bombBlue)", std::nullopt},
    {R"(bombSylux)", std::nullopt},
    {R"(columnBreak)", std::nullopt},
    {R"(grappleEnd)", std::nullopt},
    {R"(bombStartSyluxG)", std::nullopt},
    {R"(bombStartSyluxO)", std::nullopt},
    {R"(bombStartSyluxP)", std::nullopt},
    {R"(bombStartSyluxR)", std::nullopt},
    {R"(bombStartSyluxW)", std::nullopt},
    {R"(mpEffectivePB)", std::nullopt},
    {R"(mpEffectiveElectric)", std::nullopt},
    {R"(mpEffectiveMsl)", std::nullopt},
    {R"(mpEffectiveJack)", std::nullopt},
    {R"(mpEffectiveSniper)", std::nullopt},
    {R"(mpEffectiveIce)", std::nullopt},
    {R"(mpEffectiveMortar)", std::nullopt},
    {R"(mpEffectiveGhost)", std::nullopt},
    {R"(pipeTricity)", std::nullopt},
    {R"(breakableExplode)", std::nullopt},
    {R"(goreaCrystalHit)", std::nullopt},
    {R"(chargeElc)", std::nullopt},
    {R"(chargeIce)", std::nullopt},
    {R"(chargeJak)", std::nullopt},
    {R"(chargeMrt)", std::nullopt},
    {R"(chargePB)", std::nullopt},
    {R"(chargeMsl)", std::nullopt},
    {R"(electroChargeNA)", std::nullopt},
    {R"(mortarSecondary)", std::nullopt},
    {R"(jackHammerColNA)", std::optional<std::string>{R"(effects)"}},
    {R"(goreaMeteorLaunch)", std::nullopt},
    {R"(goreaReveal)", std::nullopt},
    {R"(goreaMeteorDamage)", std::nullopt},
    {R"(goreaMeteorDestroy)", std::nullopt},
    {R"(goreaMeteorHit)", std::nullopt},
    {R"(goreaGrappleDamage)", std::nullopt},
    {R"(goreaGrappleDie)", std::nullopt},
    {R"(deathBall)", std::nullopt},
    {R"(nozzleJet)", std::nullopt},
    {R"(syluxMissile)", std::nullopt},
    {R"(syluxMissileCol)", std::nullopt},
    {R"(syluxMissileFlash)", std::nullopt},
    {R"(sphereTricity)", std::nullopt},
    {R"(flamingAltForm)", std::nullopt},
    {R"(flamingGun)", std::nullopt},
    {R"(flamingHunter)", std::nullopt},
    {R"(missileCharged)", std::optional<std::string>{R"(effects)"}},
    {R"(mortarCharged)", std::optional<std::string>{R"(effects)"}},
    {R"(mortarChargedAffinity)", std::optional<std::string>{R"(effects)"}},
    {R"(DeathBio2)", std::nullopt},
    {R"(chargeLoopElc)", std::nullopt},
    {R"(chargeLoopIce)", std::nullopt},
    {R"(chargeLoopMrt)", std::nullopt},
    {R"(chargeLoopMsl)", std::nullopt},
    {R"(chargeLoopPB)", std::nullopt},
    {R"(sphereTricitySmall)", std::nullopt},
    {R"(generatorExplosion)", std::nullopt},
    {R"(eyeDamageLoop)", std::nullopt},
    {R"(eyeHit)", std::nullopt},
    {R"(eyelKill)", std::nullopt},
    {R"(eyeKill2)", std::nullopt},
    {R"(eyeKill3)", std::nullopt},
    {R"(eyeFinalKill)", std::nullopt},
    {R"(chargeTurret)", std::nullopt},
    {R"(flashTurret)", std::nullopt},
    {R"(ultimateProjectile)", std::nullopt},
    {R"(goreaLaserCharge)", std::nullopt},
    {R"(mortarProjectile)", std::nullopt},
    {R"(fallingSnow)", std::nullopt},
    {R"(fallingDust)", std::nullopt},
    {R"(fallingRock)", std::nullopt},
    {R"(DeathMech2)", std::nullopt},
    {R"(deathAlt)", std::nullopt},
    {R"(iceDemonDeath)", std::nullopt},
    {R"(lavaDemonDeath)", std::nullopt},
    {R"(DeathBio3)", std::nullopt},
    {R"(DeathBio4)", std::nullopt},
    {R"(DeathBio5)", std::nullopt},
    {R"(DeathStatue)", std::nullopt},
    {R"(DeathTick)", std::nullopt},
    {R"(goreaLaserCol)", std::nullopt},
    {R"(goreaHurt)", std::nullopt},
    {R"(explosionAbove)", std::nullopt},
    {R"(fireFlurry)", std::nullopt},
    {R"(snowFlurry)", std::nullopt},
    {R"(enemySpawn)", std::nullopt},
    {R"(teleporter)", std::nullopt},
    {R"(iceShatter)", std::optional<std::string>{R"(effects)"}},
    {R"(sphereTricityDeath)", std::nullopt},
    {R"(greenFlurry)", std::nullopt},
    {R"(pmagAbsorb)", std::nullopt},
    {R"(noxHit)", std::nullopt},
    {R"(spireBurst)", std::nullopt},
    {R"(electroProjectileUncharged)", std::nullopt},
    {R"(enemyProjectile1)", std::nullopt},
    {R"(enemyCol1)", std::nullopt},
    {R"(psychoCharge)", std::nullopt},
    {R"(hammerProjectileSml)", std::nullopt},
    {R"(nozzleJetOff)", std::nullopt},
    {R"(powerBeamChargeNoSplatMP)", std::nullopt},
    {R"(doubleDamageGun)", std::nullopt},
    {R"(ultimateCol)", std::nullopt},
    {R"(enemyMortarProjectile)", std::nullopt},
}};
const std::array<float,4> BeamRadiusValues{{0.15F,0.25F,0.5F,0.75F}};
const std::array<int,23> BeamDrawEffects{{0,237,137,0,211,130,0,0,0,0,134,209,64,0,102,94,96,0,116,138,183,238,246}};
const std::array<int,6> SyluxBombEffects{{113,152,151,153,150,149}};
const std::unordered_map<SingleType,std::pair<std::string,std::string>> SingleParticles{
    {SingleType::Death,{"deathParticle","death"}}, {SingleType::Fuzzball,{"particles","fuzzBall"}},
    {SingleType::Lore,{"icons","lore"}}, {SingleType::LoreDim,{"icons","lore_dim"}},
    {SingleType::Enemy,{"icons","enemy"}}, {SingleType::EnemyDim,{"icons","enemy_dim"}},
    {SingleType::Object,{"icons","object"}}, {SingleType::ObjectDim,{"icons","object_dim"}},
    {SingleType::Equipment,{"icons","equipment"}}, {SingleType::EquipmentDim,{"icons","equipment_dim"}},
    {SingleType::Red,{"icons","red"}}, {SingleType::RedDim,{"icons","red_dim"}}
};
const std::unordered_map<std::string,bool> PreloadResources{
    {"deathParticle",true},{"particles",true},{"particles2",true},{"TearParticle",true},
    {"icons",true},{"iceWave",true},{"sniperBeam",true},{"cylBossLaserBurn",true}
};
const OpenTK::Mathematics::Vector4 RedPalette(189.0F/255.0F,66.0F/255.0F,0.0F,1.0F);
const OpenTK::Mathematics::Vector4 WhitePalette(1.0F,1.0F,1.0F,1.0F);
const ::MphRead::ModelMetadata DoubleDamageImg("doubleDamage_img",false,false,false,
    std::nullopt,MdlSuffix::None,std::optional<std::string>{"common"},std::nullopt,false,std::nullopt,std::nullopt);

const std::unordered_map<std::string, ::MphRead::ModelMetadata> ModelMetadata{
    {R"(AlimbicBossDoorLock)", ::MphRead::ModelMetadata(R"(AlimbicBossDoorLock)", true, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::All, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicBossDoor)", ::MphRead::ModelMetadata(R"(AlimbicBossDoor)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicCapsule)", ::MphRead::ModelMetadata(R"(AlimbicCapsule)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::optional<std::string>{R"(AlmbCapsuleShld)"})},
    {R"(AlimbicComputerStationControl)", ::MphRead::ModelMetadata(R"(AlimbicComputerStationControl)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicComputerStationControl02)", ::MphRead::ModelMetadata(R"(AlimbicComputerStationControl02)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicDoorLock)", ::MphRead::ModelMetadata(R"(AlimbicDoorLock)", true, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::All, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicDoor)", ::MphRead::ModelMetadata(R"(AlimbicDoor)", R"(models\AlimbicDoor_Model.bin)", std::optional<std::string>{R"(models\AlimbicDoor_Anim.bin)"}, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(pal_01)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 0, std::vector<int>{1} }}, false), RecolorMetadata(R"(pal_02)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 1, std::vector<int>{1} }}, false), RecolorMetadata(R"(pal_03)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 2, std::vector<int>{1} }}, false), RecolorMetadata(R"(pal_04)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 3, std::vector<int>{1} }}, false), RecolorMetadata(R"(pal_05)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 4, std::vector<int>{1} }}, false), RecolorMetadata(R"(pal_06)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 5, std::vector<int>{1} }}, false), RecolorMetadata(R"(pal_07)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 6, std::vector<int>{1} }}, false), RecolorMetadata(R"(pal_08)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 7, std::vector<int>{1} }}, false)}, std::nullopt, false)},
    {R"(AlimbicEnergySensor)", ::MphRead::ModelMetadata(R"(AlimbicEnergySensor)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicGhost_01)", ::MphRead::ModelMetadata(R"(AlimbicGhost_01)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicLightPole)", ::MphRead::ModelMetadata(R"(AlimbicLightPole)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicLightPole02)", ::MphRead::ModelMetadata(R"(AlimbicLightPole02)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicMorphBallDoor)", ::MphRead::ModelMetadata(R"(AlimbicMorphBallDoor)", true, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::All, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicMorphBallDoorLock)", ::MphRead::ModelMetadata(R"(AlimbicMorphBallDoorLock)", true, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::All, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicStationShieldControl)", ::MphRead::ModelMetadata(R"(AlimbicStationShieldControl)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(AlimbicStatue_lod0)", ::MphRead::ModelMetadata(R"(AlimbicStatue_lod0)", R"(_lod0)", true, std::nullopt, true, false)},
    {R"(AlimbicThinDoor)", ::MphRead::ModelMetadata(R"(AlimbicThinDoor)", R"(models\AlimbicThinDoor_Model.bin)", std::optional<std::string>{R"(models\AlimbicThinDoor_Anim.bin)"}, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(pal_01)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 0, std::vector<int>{1, 2} }}, false), RecolorMetadata(R"(pal_02)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 1, std::vector<int>{1, 2} }}, false), RecolorMetadata(R"(pal_03)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 2, std::vector<int>{1, 2} }}, false), RecolorMetadata(R"(pal_04)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 3, std::vector<int>{1, 2} }}, false), RecolorMetadata(R"(pal_05)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 4, std::vector<int>{1, 2} }}, false), RecolorMetadata(R"(pal_06)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 5, std::vector<int>{1, 2} }}, false), RecolorMetadata(R"(pal_07)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 6, std::vector<int>{1, 2} }}, false), RecolorMetadata(R"(pal_08)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicThinDoor_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 7, std::vector<int>{1, 2} }}, false)}, std::nullopt, false)},
    {R"(Alimbic_Console)", ::MphRead::ModelMetadata(R"(Alimbic_Console)", true, true, false, std::optional<std::string>{R"(models\AlimbicEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Alimbic_Monitor)", ::MphRead::ModelMetadata(R"(Alimbic_Monitor)", true, true, false, std::optional<std::string>{R"(models\AlimbicEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Alimbic_Power)", ::MphRead::ModelMetadata(R"(Alimbic_Power)", true, false, false, std::optional<std::string>{R"(models\AlimbicEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Alimbic_Scanner)", ::MphRead::ModelMetadata(R"(Alimbic_Scanner)", true, false, false, std::optional<std::string>{R"(models\AlimbicEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Alimbic_Switch)", ::MphRead::ModelMetadata(R"(Alimbic_Switch)", true, true, false, std::optional<std::string>{R"(models\AlimbicEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Alimbic_Turret)", ::MphRead::ModelMetadata(R"(Alimbic_Turret)", std::vector<std::string>{R"(img_00)", R"(img_04)", R"(img_05)"}, std::nullopt, false, std::optional<std::string>{R"(models\AlimbicTurret_Anim.bin)"}, false, MdlSuffix::Model, std::nullopt, std::nullopt, std::nullopt, false, false, false)},
    {R"(alt_ice)", ::MphRead::ModelMetadata(R"(alt_ice)", R"(_archives\common\alt_ice_mdl_Model.bin)", std::nullopt, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(default)", R"(_archives\common\samus_ice_img_Model.bin)", R"(_archives\common\samus_ice_img_Model.bin)", R"(_archives\common\samus_ice_img_Model.bin)", std::map<int, std::vector<int>>{}, false)}, std::nullopt, true)},
    {R"(arcWelder)", ::MphRead::ModelMetadata(R"(arcWelder)", false, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(arcWelder1)", ::MphRead::ModelMetadata(R"(arcWelder1)", std::vector<std::string>{R"(1)", R"(2)", R"(3)", R"(4)", R"(5)"}, std::optional<std::string>{R"(1)"}, false, std::nullopt, false, MdlSuffix::None, std::nullopt, std::nullopt, std::nullopt, false, false, true)},
    {R"(ArtifactBase)", ::MphRead::ModelMetadata(R"(ArtifactBase)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Artifact_Key)", ::MphRead::ModelMetadata(R"(Artifact_Key)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Artifact01)", ::MphRead::ModelMetadata(R"(Artifact01)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(Artifact02)", ::MphRead::ModelMetadata(R"(Artifact02)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(Artifact03)", ::MphRead::ModelMetadata(R"(Artifact03)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(Artifact04)", ::MphRead::ModelMetadata(R"(Artifact04)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(Artifact05)", ::MphRead::ModelMetadata(R"(Artifact05)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(Artifact06)", ::MphRead::ModelMetadata(R"(Artifact06)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(Artifact07)", ::MphRead::ModelMetadata(R"(Artifact07)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(Artifact08)", ::MphRead::ModelMetadata(R"(Artifact08)", true, false, false, std::optional<std::string>{R"(models\ArtifactTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Artifact_Anim.bin)"}, std::nullopt)},
    {R"(balljump)", ::MphRead::ModelMetadata(R"(balljump)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(balljump_ray)", ::MphRead::ModelMetadata(R"(balljump_ray)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(BarbedWarWasp)", ::MphRead::ModelMetadata(R"(BarbedWarWasp)", std::vector<std::string>{R"(img_00)", R"(img_02)", R"(img_03)"}, std::nullopt, false, std::optional<std::string>{R"(models\warWasp_Anim.bin)"}, false, MdlSuffix::Model, std::nullopt, std::nullopt, std::nullopt, false, false, false)},
    {R"(BigEyeBall)", ::MphRead::ModelMetadata(R"(BigEyeBall)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(BigEyeNest)", ::MphRead::ModelMetadata(R"(BigEyeNest)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(BigEyeShield)", ::MphRead::ModelMetadata(R"(BigEyeShield)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(BigEyeSynapse_01)", ::MphRead::ModelMetadata(R"(BigEyeSynapse_01)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\BigEyeSynapse_Anim.bin)"}, std::nullopt)},
    {R"(BigEyeSynapse_02)", ::MphRead::ModelMetadata(R"(BigEyeSynapse_02)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\BigEyeSynapse_Anim.bin)"}, std::nullopt)},
    {R"(BigEyeSynapse_03)", ::MphRead::ModelMetadata(R"(BigEyeSynapse_03)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\BigEyeSynapse_Anim.bin)"}, std::nullopt)},
    {R"(BigEyeSynapse_04)", ::MphRead::ModelMetadata(R"(BigEyeSynapse_04)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\BigEyeSynapse_Anim.bin)"}, std::nullopt)},
    {R"(BigEyeTurret)", ::MphRead::ModelMetadata(R"(BigEyeTurret)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(blastcap)", ::MphRead::ModelMetadata(R"(blastcap)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(brain_unit3_c2)", ::MphRead::ModelMetadata(R"(brain_unit3_c2)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Chomtroid)", ::MphRead::ModelMetadata(R"(Chomtroid)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::optional<std::string>{R"(models\Mochtroid_Anim.bin)"}, std::nullopt)},
    {R"(Crate01)", ::MphRead::ModelMetadata(R"(Crate01)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(cylBossLaserBurn)", ::MphRead::ModelMetadata(R"(cylBossLaserBurn)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(cylBossLaserColl)", ::MphRead::ModelMetadata(R"(cylBossLaserColl)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(cylBossLaserG)", ::MphRead::ModelMetadata(R"(cylBossLaserG)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(cylBossLaserY)", ::MphRead::ModelMetadata(R"(cylBossLaserY)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(cylBossLaser)", ::MphRead::ModelMetadata(R"(cylBossLaser)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(cylinderbase)", ::MphRead::ModelMetadata(R"(cylinderbase)", R"(models\cylinderbase_model.bin)", std::nullopt, std::optional<std::string>{R"(models\cylinderbase_collision.bin)"}, false)},
    {R"(CylinderBossEye)", ::MphRead::ModelMetadata(R"(CylinderBossEye)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(CylinderBoss)", ::MphRead::ModelMetadata(R"(CylinderBoss)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(deepspace)", ::MphRead::ModelMetadata(R"(deepspace)", true, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(shipSpace)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Door_Unit4_RM1)", ::MphRead::ModelMetadata(R"(Door_Unit4_RM1)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(DripStank_lod0)", ::MphRead::ModelMetadata(R"(DripStank_lod0)", R"(_lod0)", true, std::nullopt, false, false)},
    {R"(ElectroField1)", ::MphRead::ModelMetadata(R"(ElectroField1)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(electroTrail)", ::MphRead::ModelMetadata(R"(electroTrail)", false, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Elevator)", ::MphRead::ModelMetadata(R"(Elevator)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(EnemySpawner)", ::MphRead::ModelMetadata(R"(EnemySpawner)", true, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::All, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(energyBeam)", ::MphRead::ModelMetadata(R"(energyBeam)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(filter)", ::MphRead::ModelMetadata(R"(filter)", false, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(hudfont)", ::MphRead::ModelMetadata(R"(hudfont)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(flagbase_bounty)", ::MphRead::ModelMetadata(R"(flagbase_bounty)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(flagbase_cap)", ::MphRead::ModelMetadata(R"(flagbase_cap)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(flagbase_ctf)", ::MphRead::ModelMetadata(R"(flagbase_ctf)", std::vector<std::string>{R"(orange_img)", R"(green_img)"}, std::nullopt, true, std::nullopt, false, MdlSuffix::Model, std::nullopt, std::nullopt, std::nullopt, false, false, false)},
    {R"(ForceField)", ::MphRead::ModelMetadata(R"(ForceField)", R"(models\ForceField_Model.bin)", std::optional<std::string>{R"(models\ForceField_Anim.bin)"}, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(pal_01)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 0, std::vector<int>{0} }}, false), RecolorMetadata(R"(pal_02)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 1, std::vector<int>{0} }}, false), RecolorMetadata(R"(pal_03)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 2, std::vector<int>{0} }}, false), RecolorMetadata(R"(pal_04)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 3, std::vector<int>{0} }}, false), RecolorMetadata(R"(pal_05)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 4, std::vector<int>{0} }}, false), RecolorMetadata(R"(pal_06)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 5, std::vector<int>{0} }}, false), RecolorMetadata(R"(pal_07)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 6, std::vector<int>{0} }}, false), RecolorMetadata(R"(pal_08)", R"(models\ForceField_Model.bin)", R"(models\ForceField_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 7, std::vector<int>{0} }}, false)}, std::nullopt, false)},
    {R"(ForceFieldLock)", ::MphRead::ModelMetadata(R"(ForceFieldLock)", R"(models\ForceFieldLock_mdl_Model.bin)", std::optional<std::string>{R"(models\ForceFieldLock_mdl_Anim.bin)"}, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(pal_01)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 0, std::vector<int>{3} }}, true), RecolorMetadata(R"(pal_02)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 1, std::vector<int>{3} }}, true), RecolorMetadata(R"(pal_03)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 2, std::vector<int>{3} }}, true), RecolorMetadata(R"(pal_04)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 3, std::vector<int>{3} }}, true), RecolorMetadata(R"(pal_05)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 4, std::vector<int>{3} }}, true), RecolorMetadata(R"(pal_06)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 5, std::vector<int>{3} }}, true), RecolorMetadata(R"(pal_07)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 6, std::vector<int>{3} }}, true), RecolorMetadata(R"(pal_08)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicTextureShare_img_Model.bin)", R"(models\AlimbicPalettes_pal_Model.bin)", std::map<int, std::vector<int>>{{ 7, std::vector<int>{3} }}, true)}, std::nullopt, false)},
    {R"(furlEffect)", ::MphRead::ModelMetadata(R"(furlEffect)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(geemer)", ::MphRead::ModelMetadata(R"(geemer)", R"(models\geemer_Model.bin)", std::optional<std::string>{R"(models\Geemer_Anim.bin)"}, std::nullopt, false)},
    {R"(Generic_Console)", ::MphRead::ModelMetadata(R"(Generic_Console)", true, true, false, std::optional<std::string>{R"(models\GenericEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Generic_Monitor)", ::MphRead::ModelMetadata(R"(Generic_Monitor)", true, true, false, std::optional<std::string>{R"(models\GenericEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Generic_Power)", ::MphRead::ModelMetadata(R"(Generic_Power)", true, false, false, std::optional<std::string>{R"(models\GenericEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Generic_Scanner)", ::MphRead::ModelMetadata(R"(Generic_Scanner)", true, false, false, std::optional<std::string>{R"(models\GenericEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Generic_Switch)", ::MphRead::ModelMetadata(R"(Generic_Switch)", true, true, false, std::optional<std::string>{R"(models\GenericEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(GhostSwitch)", ::MphRead::ModelMetadata(R"(GhostSwitch)", true, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::All, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Gorea1A_lod0)", ::MphRead::ModelMetadata(R"(Gorea1A_lod0)", R"(_lod0)", true, std::nullopt, false, false)},
    {R"(Gorea1B_lod0)", ::MphRead::ModelMetadata(R"(Gorea1B_lod0)", R"(_lod0)", true, std::nullopt, false, false)},
    {R"(Gorea2_lod0)", ::MphRead::ModelMetadata(R"(Gorea2_lod0)", R"(_lod0)", true, std::nullopt, false, false)},
    {R"(goreaArmRegen)", ::MphRead::ModelMetadata(R"(goreaArmRegen)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(goreaGeo)", ::MphRead::ModelMetadata(R"(goreaGeo)", false, false, true, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(goreaGrappleBeam)", ::MphRead::ModelMetadata(R"(goreaGrappleBeam)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},

    {R"(goreaLaserColl)", ::MphRead::ModelMetadata(R"(goreaLaserColl)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(goreaLaser)", ::MphRead::ModelMetadata(R"(goreaLaser)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(goreaMeteor)", ::MphRead::ModelMetadata(R"(goreaMeteor)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(goreaMindTrick)", ::MphRead::ModelMetadata(R"(goreaMindTrick)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(gorea_gun)", ::MphRead::ModelMetadata(R"(gorea_gun)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Guardbot01_Dead)", ::MphRead::ModelMetadata(R"(Guardbot01_Dead)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Guardbot02_Dead)", ::MphRead::ModelMetadata(R"(Guardbot02_Dead)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(GuardBot1)", ::MphRead::ModelMetadata(R"(GuardBot1)", std::vector<std::string>{R"(img_00)", R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)"}, std::nullopt, false, std::optional<std::string>{R"(models\GuardBot01_Anim.bin)"}, false, MdlSuffix::Model, std::nullopt, std::nullopt, std::nullopt, false, false, false)},
    {R"(GuardBot2_lod0)", ::MphRead::ModelMetadata(R"(GuardBot2_lod0)", R"(_lod0)", true, std::optional<std::string>{R"(models\GuardBot02_Anim.bin)"}, false, false)},
    {R"(Guardian_Dead)", ::MphRead::ModelMetadata(R"(Guardian_Dead)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Guardian_lod0)", ::MphRead::ModelMetadata(R"(Guardian_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, false, std::optional<std::string>{R"(_archives\Guardian\Guardian_Anim.bin)"}, true, MdlSuffix::None, std::optional<std::string>{R"(Guardian)"}, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Guardian_lod1)", ::MphRead::ModelMetadata(R"(Guardian_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod1)"}, false, std::optional<std::string>{R"(_archives\Guardian\Guardian_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Guardian_Stasis)", ::MphRead::ModelMetadata(R"(Guardian_Stasis)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(gunSmoke)", ::MphRead::ModelMetadata(R"(gunSmoke)", true, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ice_Console)", ::MphRead::ModelMetadata(R"(Ice_Console)", true, true, false, std::optional<std::string>{R"(models\IceEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ice_Monitor)", ::MphRead::ModelMetadata(R"(Ice_Monitor)", true, true, false, std::optional<std::string>{R"(models\IceEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ice_Power)", ::MphRead::ModelMetadata(R"(Ice_Power)", true, false, false, std::optional<std::string>{R"(models\IceEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ice_Scanner)", ::MphRead::ModelMetadata(R"(Ice_Scanner)", true, false, false, std::optional<std::string>{R"(models\IceEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ice_Switch)", ::MphRead::ModelMetadata(R"(Ice_Switch)", true, true, false, std::optional<std::string>{R"(models\IceEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(iceShard)", ::MphRead::ModelMetadata(R"(iceShard)", false, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(iceWave)", ::MphRead::ModelMetadata(R"(iceWave)", true, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(items_base)", ::MphRead::ModelMetadata(R"(items_base)", false, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(JumpPad_Alimbic)", ::MphRead::ModelMetadata(R"(JumpPad_Alimbic)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(JumpPad_Beam)", ::MphRead::ModelMetadata(R"(JumpPad_Beam)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(JumpPad_IceStation)", ::MphRead::ModelMetadata(R"(JumpPad_IceStation)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(JumpPad_Ice)", ::MphRead::ModelMetadata(R"(JumpPad_Ice)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(JumpPad_Lava)", ::MphRead::ModelMetadata(R"(JumpPad_Lava)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(JumpPad)", ::MphRead::ModelMetadata(R"(JumpPad)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(JumpPad_Station)", ::MphRead::ModelMetadata(R"(JumpPad_Station)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Kanden_lod0)", ::MphRead::ModelMetadata(R"(Kanden_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Kanden)"}, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Kanden_lod1)", ::MphRead::ModelMetadata(R"(Kanden_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod1)"}, true, std::optional<std::string>{R"(_archives\Kanden\Kanden_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(KandenAlt_lod0)", ::MphRead::ModelMetadata(R"(KandenAlt_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Kanden)"}, std::optional<std::string>{R"(Kanden)"}, std::nullopt, true, false, false)},
    {R"(KandenAlt_TailBomb)", ::MphRead::ModelMetadata(R"(KandenAlt_TailBomb)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::nullopt, false, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Kanden)"}, std::optional<std::string>{R"(Kanden)"}, std::nullopt, false, false, false)},
    {R"(KandenGun)", ::MphRead::ModelMetadata(R"(KandenGun)", std::vector<std::string>{R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)", R"(img_Team01)", R"(img_Team02)"}, std::nullopt, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(localKanden)"}, std::nullopt, std::nullopt, true, false, false)},
    {R"(koth_data_flow)", ::MphRead::ModelMetadata(R"(koth_data_flow)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(koth_terminal)", ::MphRead::ModelMetadata(R"(koth_terminal)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(LavaDemon)", ::MphRead::ModelMetadata(R"(LavaDemon)", std::vector<std::string>{R"(img_00)", R"(img_03)"}, std::nullopt, true, std::nullopt, false, MdlSuffix::Model, std::nullopt, std::nullopt, std::nullopt, false, false, false)},
    {R"(Lava_Console)", ::MphRead::ModelMetadata(R"(Lava_Console)", true, true, false, std::optional<std::string>{R"(models\LavaEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Lava_Monitor)", ::MphRead::ModelMetadata(R"(Lava_Monitor)", true, true, false, std::optional<std::string>{R"(models\LavaEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Lava_Power)", ::MphRead::ModelMetadata(R"(Lava_Power)", true, false, false, std::optional<std::string>{R"(models\LavaEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Lava_Scanner)", ::MphRead::ModelMetadata(R"(Lava_Scanner)", true, false, false, std::optional<std::string>{R"(models\LavaEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Lava_Switch)", ::MphRead::ModelMetadata(R"(Lava_Switch)", true, true, false, std::optional<std::string>{R"(models\LavaEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(lines)", ::MphRead::ModelMetadata(R"(lines)", true, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(frontend2d)"}, std::optional<std::string>{R"(_Idle)"}, false, std::nullopt, std::nullopt)},
    {R"(MoverTest)", ::MphRead::ModelMetadata(R"(MoverTest)", R"(models\MoverTest_Model.bin)", std::optional<std::string>{R"(models\movertest_Anim.bin)"}, std::nullopt, false)},
    {R"(Nox_lod0)", ::MphRead::ModelMetadata(R"(Nox_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Nox)"}, std::nullopt, std::optional<std::string>{R"(models\NoxSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Nox_lod1)", ::MphRead::ModelMetadata(R"(Nox_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod1)"}, true, std::optional<std::string>{R"(_archives\Nox\Nox_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\NoxSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(NoxAlt_lod0)", ::MphRead::ModelMetadata(R"(NoxAlt_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Nox)"}, std::optional<std::string>{R"(Nox)"}, std::nullopt, true, false, false)},
    {R"(NoxGun)", ::MphRead::ModelMetadata(R"(NoxGun)", std::vector<std::string>{R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)", R"(img_Team01)", R"(img_Team02)"}, std::nullopt, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(localNox)"}, std::nullopt, std::nullopt, true, false, false)},
    {R"(nox_ice)", ::MphRead::ModelMetadata(R"(nox_ice)", R"(_archives\common\nox_ice_mdl_Model.bin)", std::nullopt, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(default)", R"(_archives\common\samus_ice_img_Model.bin)", R"(_archives\common\samus_ice_img_Model.bin)", R"(_archives\common\samus_ice_img_Model.bin)", std::map<int, std::vector<int>>{}, false)}, std::nullopt, true)},
    {R"(octolith_ctf)", ::MphRead::ModelMetadata(R"(octolith_ctf)", std::vector<std::string>{R"(orange_img)", R"(green_img)", R"(*octolith_bounty_img)"}, std::nullopt, true, std::nullopt, false, MdlSuffix::Model, std::nullopt, std::nullopt, std::nullopt, false, false, false)},
    {R"(Octolith)", ::MphRead::ModelMetadata(R"(Octolith)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(octolith_simple)", ::MphRead::ModelMetadata(R"(octolith_simple)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PickUp_AmmoExp)", ::MphRead::ModelMetadata(R"(PickUp_AmmoExp)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PickUp_EnergyExp)", ::MphRead::ModelMetadata(R"(PickUp_EnergyExp)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PickUp_MissileExp)", ::MphRead::ModelMetadata(R"(PickUp_MissileExp)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_ammo_green)", ::MphRead::ModelMetadata(R"(pick_ammo_green)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_ammo_orange)", ::MphRead::ModelMetadata(R"(pick_ammo_orange)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_dblDamage)", ::MphRead::ModelMetadata(R"(pick_dblDamage)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_deathball)", ::MphRead::ModelMetadata(R"(pick_deathball)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_health_A)", ::MphRead::ModelMetadata(R"(pick_health_A)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_health_B)", ::MphRead::ModelMetadata(R"(pick_health_B)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_health_C)", ::MphRead::ModelMetadata(R"(pick_health_C)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_invis)", ::MphRead::ModelMetadata(R"(pick_invis)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_all)", ::MphRead::ModelMetadata(R"(pick_wpn_all)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_electro)", ::MphRead::ModelMetadata(R"(pick_wpn_electro)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_ghostbuster)", ::MphRead::ModelMetadata(R"(pick_wpn_ghostbuster)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_gorea)", ::MphRead::ModelMetadata(R"(pick_wpn_gorea)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_jackhammer)", ::MphRead::ModelMetadata(R"(pick_wpn_jackhammer)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_missile)", ::MphRead::ModelMetadata(R"(pick_wpn_missile)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_mortar)", ::MphRead::ModelMetadata(R"(pick_wpn_mortar)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_shotgun)", ::MphRead::ModelMetadata(R"(pick_wpn_shotgun)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pick_wpn_snipergun)", ::MphRead::ModelMetadata(R"(pick_wpn_snipergun)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pillar)", ::MphRead::ModelMetadata(R"(pillar)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(pistonmp7)", ::MphRead::ModelMetadata(R"(pistonmp7)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(piston_gorealand)", ::MphRead::ModelMetadata(R"(piston_gorealand)", R"(models\piston_gorealand_model.bin)", std::nullopt, std::optional<std::string>{R"(models\piston_gorealand_collision.bin)"}, false)},
    {R"(PlantCarnivarous_Branched)", ::MphRead::ModelMetadata(R"(PlantCarnivarous_Branched)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PlantCarnivarous_PodLeaves)", ::MphRead::ModelMetadata(R"(PlantCarnivarous_PodLeaves)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PlantCarnivarous_Pod)", ::MphRead::ModelMetadata(R"(PlantCarnivarous_Pod)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PlantCarnivarous_Vine)", ::MphRead::ModelMetadata(R"(PlantCarnivarous_Vine)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(platform)", ::MphRead::ModelMetadata(R"(platform)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Platform_Unit4_C1)", ::MphRead::ModelMetadata(R"(Platform_Unit4_C1)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PowerBomb)", ::MphRead::ModelMetadata(R"(PowerBomb)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Psychobit_Dead)", ::MphRead::ModelMetadata(R"(Psychobit_Dead)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(PsychoBit)", ::MphRead::ModelMetadata(R"(PsychoBit)", std::vector<std::string>{R"(img_00)", R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)"}, std::nullopt, true, std::nullopt, false, MdlSuffix::Model, std::nullopt, std::nullopt, std::nullopt, false, false, false)},
    {R"(quads)", ::MphRead::ModelMetadata(R"(quads)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ruins_Console)", ::MphRead::ModelMetadata(R"(Ruins_Console)", true, true, false, std::optional<std::string>{R"(models\RuinsEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ruins_Monitor)", ::MphRead::ModelMetadata(R"(Ruins_Monitor)", true, true, false, std::optional<std::string>{R"(models\RuinsEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ruins_Power)", ::MphRead::ModelMetadata(R"(Ruins_Power)", true, false, false, std::optional<std::string>{R"(models\RuinsEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ruins_Scanner)", ::MphRead::ModelMetadata(R"(Ruins_Scanner)", true, false, false, std::optional<std::string>{R"(models\RuinsEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Ruins_Switch)", ::MphRead::ModelMetadata(R"(Ruins_Switch)", true, true, false, std::optional<std::string>{R"(models\RuinsEquipTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(SamusShip)", ::MphRead::ModelMetadata(R"(SamusShip)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Samus_lod0)", ::MphRead::ModelMetadata(R"(Samus_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_team01)", R"(pal_team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Samus)"}, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Samus_lod1)", ::MphRead::ModelMetadata(R"(Samus_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_team01)", R"(pal_team02)"}, std::optional<std::string>{R"(_lod1)"}, true, std::optional<std::string>{R"(_archives\Samus\Samus_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(SamusAlt_lod0)", ::MphRead::ModelMetadata(R"(SamusAlt_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_team01)", R"(pal_team02)"}, std::optional<std::string>{R"(_lod0)"}, false, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Samus)"}, std::optional<std::string>{R"(Samus)"}, std::nullopt, true, false, false)},
    {R"(SamusGun)", ::MphRead::ModelMetadata(R"(SamusGun)", std::vector<std::string>{R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)", R"(img_Team01)", R"(img_Team02)"}, std::nullopt, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(localSamus)"}, std::nullopt, std::nullopt, true, false, false)},
    {R"(samus_ice)", ::MphRead::ModelMetadata(R"(samus_ice)", R"(_archives\common\samus_ice_mdl_Model.bin)", std::nullopt, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(default)", R"(_archives\common\samus_ice_img_Model.bin)", R"(_archives\common\samus_ice_img_Model.bin)", R"(_archives\common\samus_ice_img_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(dbl_dmg)", R"(_archives\common\doubleDamage_img_Model.bin)", R"(_archives\common\doubleDamage_img_Model.bin)", R"(_archives\common\doubleDamage_img_Model.bin)", std::map<int, std::vector<int>>{}, false)}, std::nullopt, true)},
    {R"(SecretSwitch)", ::MphRead::ModelMetadata(R"(SecretSwitch)", R"(models\SecretSwitch_Model.bin)", std::optional<std::string>{R"(models\SecretSwitch_Anim.bin)"}, std::optional<std::string>{R"(models\SecretSwitch_Collision.bin)"}, std::vector<RecolorMetadata>{RecolorMetadata(R"(default)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_01)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_pal_01_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_02)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_pal_02_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_03)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_pal_03_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_04)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_pal_04_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_05)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_pal_05_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_06)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_Model.bin)", R"(models\SecretSwitch_pal_06_Model.bin)", std::map<int, std::vector<int>>{}, false)}, std::nullopt, false)},
    {R"(shriekbat)", ::MphRead::ModelMetadata(R"(shriekbat)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(slots)", ::MphRead::ModelMetadata(R"(slots)", true, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(frontend2d)"}, std::optional<std::string>{R"(_Idle)"}, false, std::nullopt, std::nullopt)},
    {R"(smasher)", ::MphRead::ModelMetadata(R"(smasher)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(sniperBeam)", ::MphRead::ModelMetadata(R"(sniperBeam)", true, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(SniperTarget)", ::MphRead::ModelMetadata(R"(SniperTarget)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(SphinkTick_lod0)", ::MphRead::ModelMetadata(R"(SphinkTick_lod0)", R"(_lod0)", true, std::nullopt, false, false)},
    {R"(Spire_lod0)", ::MphRead::ModelMetadata(R"(Spire_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Spire)"}, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Spire_lod1)", ::MphRead::ModelMetadata(R"(Spire_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod1)"}, true, std::optional<std::string>{R"(_archives\Spire\Spire_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(SpireAlt_lod0)", ::MphRead::ModelMetadata(R"(SpireAlt_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Spire)"}, std::optional<std::string>{R"(Spire)"}, std::nullopt, true, false, false)},
    {R"(SpireGun)", ::MphRead::ModelMetadata(R"(SpireGun)", std::vector<std::string>{R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)", R"(img_Team01)", R"(img_Team02)"}, std::nullopt, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(localSpire)"}, std::nullopt, std::nullopt, true, false, false)},
    {R"(splashRing)", ::MphRead::ModelMetadata(R"(splashRing)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Switch)", ::MphRead::ModelMetadata(R"(Switch)", false, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::Model, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Sylux_lod0)", ::MphRead::ModelMetadata(R"(Sylux_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Sylux)"}, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Sylux_lod1)", ::MphRead::ModelMetadata(R"(Sylux_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod1)"}, true, std::optional<std::string>{R"(_archives\Sylux\Sylux_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(SyluxAlt_lod0)", ::MphRead::ModelMetadata(R"(SyluxAlt_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Sylux)"}, std::optional<std::string>{R"(Sylux)"}, std::nullopt, true, false, false)},
    {R"(SyluxGun)", ::MphRead::ModelMetadata(R"(SyluxGun)", std::vector<std::string>{R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)", R"(img_Team01)", R"(img_Team02)"}, std::nullopt, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(localSylux)"}, std::nullopt, std::nullopt, true, false, false)},
    {R"(SyluxShip)", ::MphRead::ModelMetadata(R"(SyluxShip)", R"(models\SyluxShip_Model.bin)", std::optional<std::string>{R"(models\Syluxship_Anim.bin)"}, std::optional<std::string>{R"(models\SyluxShip_Collision.bin)"}, false)},
    {R"(SyluxTurret)", ::MphRead::ModelMetadata(R"(SyluxTurret)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Teleporter)", ::MphRead::ModelMetadata(R"(Teleporter)", R"(models\Teleporter_mdl_Model.bin)", std::optional<std::string>{R"(models\Teleporter_mdl_Anim.bin)"}, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(pal_01)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_01_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_02)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_02_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_03)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_03_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_04)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_04_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_05)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_05_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_06)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_06_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_07)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_07_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_08)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_08_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_09)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_09_Model.bin)", std::map<int, std::vector<int>>{}, false)}, std::nullopt, false)},
    {R"(TeleporterSmall)", ::MphRead::ModelMetadata(R"(TeleporterSmall)", R"(models\TeleporterSmall_mdl_Model.bin)", std::optional<std::string>{R"(models\TeleporterSmall_mdl_Anim.bin)"}, std::nullopt, std::vector<RecolorMetadata>{RecolorMetadata(R"(pal_01)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_01_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_02)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_02_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_03)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_03_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_04)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_04_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_05)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_05_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_06)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_06_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_07)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_07_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_08)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_08_Model.bin)", std::map<int, std::vector<int>>{}, false), RecolorMetadata(R"(pal_09)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\TeleporterTextureShare_img_Model.bin)", R"(models\Teleporter_pal_09_Model.bin)", std::map<int, std::vector<int>>{}, false)}, std::nullopt, false)},
    {R"(TeleporterMP)", ::MphRead::ModelMetadata(R"(TeleporterMP)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Temroid_lod0)", ::MphRead::ModelMetadata(R"(Temroid_lod0)", R"(_lod0)", false, std::nullopt, false, false)},

    {R"(ThinDoorLock)", ::MphRead::ModelMetadata(R"(ThinDoorLock)", true, false, false, std::optional<std::string>{R"(models\AlimbicTextureShare_img_Model.bin)"}, MdlSuffix::All, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Trace_lod0)", ::MphRead::ModelMetadata(R"(Trace_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Trace)"}, std::nullopt, std::optional<std::string>{R"(models\NoxSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Trace_lod1)", ::MphRead::ModelMetadata(R"(Trace_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod1)"}, true, std::optional<std::string>{R"(_archives\Trace\Trace_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\NoxSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(TraceAlt_lod0)", ::MphRead::ModelMetadata(R"(TraceAlt_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Trace)"}, std::optional<std::string>{R"(Trace)"}, std::nullopt, true, false, false)},
    {R"(TraceGun)", ::MphRead::ModelMetadata(R"(TraceGun)", std::vector<std::string>{R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)", R"(img_Team01)", R"(img_Team02)"}, std::nullopt, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(localTrace)"}, std::nullopt, std::nullopt, true, false, false)},
    {R"(trail)", ::MphRead::ModelMetadata(R"(trail)", false, false, false, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(common)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(unit1_land_plat1)", ::MphRead::ModelMetadata(R"(unit1_land_plat1)", R"(models\unit1_land_plat1_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit1_land_plat1_collision.bin)"}, false)},
    {R"(unit1_land_plat2)", ::MphRead::ModelMetadata(R"(unit1_land_plat2)", R"(models\unit1_land_plat2_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit1_land_plat2_collision.bin)"}, false)},
    {R"(unit1_land_plat3)", ::MphRead::ModelMetadata(R"(unit1_land_plat3)", R"(models\unit1_land_plat3_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit1_land_plat3_collision.bin)"}, false)},
    {R"(unit1_land_plat4)", ::MphRead::ModelMetadata(R"(unit1_land_plat4)", R"(models\unit1_land_plat4_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit1_land_plat4_collision.bin)"}, false)},
    {R"(unit1_land_plat5)", ::MphRead::ModelMetadata(R"(unit1_land_plat5)", R"(models\unit1_land_plat5_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit1_land_plat5_collision.bin)"}, false)},
    {R"(unit1_mover1)", ::MphRead::ModelMetadata(R"(unit1_mover1)", R"(models\unit1_mover1_model.bin)", std::optional<std::string>{R"(models\unit1_mover1_anim.bin)"}, std::optional<std::string>{R"(models\unit1_mover1_collision.bin)"}, false)},
    {R"(unit1_mover2)", ::MphRead::ModelMetadata(R"(unit1_mover2)", R"(models\unit1_mover2_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit1_mover2_collision.bin)"}, false)},
    {R"(unit2_c1_mover)", ::MphRead::ModelMetadata(R"(unit2_c1_mover)", R"(models\unit2_c1_mover_Model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit2_c1_mover_collision.bin)"}, false)},
    {R"(unit2_c4_plat)", ::MphRead::ModelMetadata(R"(unit2_c4_plat)", R"(models\unit2_c4_plat_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit2_c4_plat_collision.bin)"}, false)},
    {R"(unit2_land_elev)", ::MphRead::ModelMetadata(R"(unit2_land_elev)", R"(models\unit2_land_elev_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit2_land_elev_collision.bin)"}, false)},
    {R"(unit2_mover1)", ::MphRead::ModelMetadata(R"(unit2_mover1)", R"(models\unit2_mover1_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit2_mover1_collision.bin)"}, false)},
    {R"(unit3_brain)", ::MphRead::ModelMetadata(R"(unit3_brain)", R"(models\unit3_brain_Model.bin)", std::optional<std::string>{R"(models\Unit3_brain_Anim.bin)"}, std::optional<std::string>{R"(models\unit3_brain_Collision.bin)"}, false)},
    {R"(unit3_jar)", ::MphRead::ModelMetadata(R"(unit3_jar)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(unit3_jartop)", ::MphRead::ModelMetadata(R"(unit3_jartop)", R"(models\unit3_jartop_model.bin)", std::optional<std::string>{R"(models\unit3_jartop_anim.bin)"}, std::nullopt, false)},
    {R"(unit3_mover1)", ::MphRead::ModelMetadata(R"(unit3_mover1)", R"(models\unit3_mover1_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit3_mover1_collision.bin)"}, false)},
    {R"(unit3_mover2)", ::MphRead::ModelMetadata(R"(unit3_mover2)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(unit3_pipe1)", ::MphRead::ModelMetadata(R"(unit3_pipe1)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(unit3_pipe2)", ::MphRead::ModelMetadata(R"(unit3_pipe2)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(Unit3_platform1)", ::MphRead::ModelMetadata(R"(Unit3_platform1)", R"(models\Unit3_platform1_Model.bin)", std::optional<std::string>{R"(models\unit3_platform1_Anim.bin)"}, std::optional<std::string>{R"(models\unit3_platform1_Collision.bin)"}, false)},
    {R"(unit3_platform)", ::MphRead::ModelMetadata(R"(unit3_platform)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(unit3_platform2)", ::MphRead::ModelMetadata(R"(unit3_platform2)", R"(models\unit3_platform2_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit3_platform2_collision.bin)"}, false)},
    {R"(unit4_mover1)", ::MphRead::ModelMetadata(R"(unit4_mover1)", true, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(unit4_mover2)", ::MphRead::ModelMetadata(R"(unit4_mover2)", R"(models\unit4_mover2_model.bin)", std::optional<std::string>{R"(models\unit4_mover2_anim.bin)"}, std::optional<std::string>{R"(models\unit4_mover2_collision.bin)"}, false)},
    {R"(unit4_mover3)", ::MphRead::ModelMetadata(R"(unit4_mover3)", R"(models\unit4_mover3_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit4_mover3_collision.bin)"}, false)},
    {R"(unit4_mover4)", ::MphRead::ModelMetadata(R"(unit4_mover4)", R"(models\unit4_mover4_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit4_mover4_collision.bin)"}, false)},
    {R"(unit4_platform1)", ::MphRead::ModelMetadata(R"(unit4_platform1)", R"(models\unit4_platform1_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit4_platform1_collision.bin)"}, false)},
    {R"(unit4_tp1_artifact_wo)", ::MphRead::ModelMetadata(R"(unit4_tp1_artifact_wo)", R"(models\unit4_tp1_artifact_wo_model.bin)", std::nullopt, std::optional<std::string>{R"(models\unit4_tp1_artifact_wo_collision.bin)"}, false)},
    {R"(unit4_tp2_artifact_wo)", ::MphRead::ModelMetadata(R"(unit4_tp2_artifact_wo)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(WallSwitch)", ::MphRead::ModelMetadata(R"(WallSwitch)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(warwasp_lod0)", ::MphRead::ModelMetadata(R"(warwasp_lod0)", R"(models\warwasp_lod0_Model.bin)", std::optional<std::string>{R"(models\warWasp_Anim.bin)"}, std::nullopt, false)},
    {R"(Weavel_lod0)", ::MphRead::ModelMetadata(R"(Weavel_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Weavel)"}, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(Weavel_lod1)", ::MphRead::ModelMetadata(R"(Weavel_lod1)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod1)"}, true, std::optional<std::string>{R"(_archives\Weavel\Weavel_Anim.bin)"}, true, MdlSuffix::None, std::nullopt, std::nullopt, std::optional<std::string>{R"(models\SamusSharedAnim_Anim.bin)"}, true, false, false)},
    {R"(WeavelAlt_lod0)", ::MphRead::ModelMetadata(R"(WeavelAlt_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Weavel)"}, std::optional<std::string>{R"(Weavel)"}, std::nullopt, true, false, false)},
    {R"(WeavelAlt_Turret_lod0)", ::MphRead::ModelMetadata(R"(WeavelAlt_Turret_lod0)", std::vector<std::string>{R"(pal_01)", R"(pal_02)", R"(pal_03)", R"(pal_04)", R"(pal_Team01)", R"(pal_Team02)"}, std::optional<std::string>{R"(_lod0)"}, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(Weavel)"}, std::optional<std::string>{R"(Weavel)"}, std::nullopt, true, false, false)},
    {R"(WeavelGun)", ::MphRead::ModelMetadata(R"(WeavelGun)", std::vector<std::string>{R"(img_01)", R"(img_02)", R"(img_03)", R"(img_04)", R"(img_Team01)", R"(img_Team02)"}, std::nullopt, true, std::nullopt, true, MdlSuffix::None, std::optional<std::string>{R"(localWeavel)"}, std::nullopt, std::nullopt, true, false, false)},
    {R"(zoomer)", ::MphRead::ModelMetadata(R"(zoomer)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(deathParticle)", ::MphRead::ModelMetadata(R"(deathParticle)", false, false, true, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(effectsBase)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(geo1)", ::MphRead::ModelMetadata(R"(geo1)", false, false, true, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(effectsBase)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(particles)", ::MphRead::ModelMetadata(R"(particles)", false, false, true, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(effectsBase)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(particles2)", ::MphRead::ModelMetadata(R"(particles2)", false, false, true, std::nullopt, MdlSuffix::None, std::optional<std::string>{R"(effectsBase)"}, std::nullopt, false, std::nullopt, std::nullopt)},
    {R"(TearParticle)", ::MphRead::ModelMetadata(R"(TearParticle)", false, false, true, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, false, std::nullopt, std::nullopt)},
};

const std::unordered_map<std::string, ::MphRead::ModelMetadata> FirstHuntModels{
    {R"(ballDeath)", ::MphRead::ModelMetadata(R"(ballDeath)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(balljump)", ::MphRead::ModelMetadata(R"(balljump)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(balljump_ray)", ::MphRead::ModelMetadata(R"(balljump_ray)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(bomb)", ::MphRead::ModelMetadata(R"(bomb)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(bombLite)", ::MphRead::ModelMetadata(R"(bombLite)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(bombStart)", ::MphRead::ModelMetadata(R"(bombStart)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(bombStartLite)", ::MphRead::ModelMetadata(R"(bombStartLite)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(bombStartLiter)", ::MphRead::ModelMetadata(R"(bombStartLiter)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(dashEffect)", ::MphRead::ModelMetadata(R"(dashEffect)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(door)", ::MphRead::ModelMetadata(R"(door)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(door2)", ::MphRead::ModelMetadata(R"(door2)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(door2_holo)", ::MphRead::ModelMetadata(R"(door2_holo)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(effWaspDeath)", ::MphRead::ModelMetadata(R"(effWaspDeath)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(furlEffect)", ::MphRead::ModelMetadata(R"(furlEffect)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(fuzzball)", ::MphRead::ModelMetadata(R"(fuzzball)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(genericMover)", ::MphRead::ModelMetadata(R"(genericMover)", R"(models\genericMover_Model.bin)", std::optional<std::string>{R"(models\genericmover_Anim.bin)"}, std::optional<std::string>{R"(models\genericMover_Collision.bin)"}, false)},
    {R"(gun_idle)", ::MphRead::ModelMetadata(R"(gun_idle)", R"(_idle)", true, std::nullopt, false, true)},
    {R"(gunEffElectroCharge)", ::MphRead::ModelMetadata(R"(gunEffElectroCharge)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(gunEffMissileCharge)", ::MphRead::ModelMetadata(R"(gunEffMissileCharge)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(gunLobFlash)", ::MphRead::ModelMetadata(R"(gunLobFlash)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(gunMuzzleFlash)", ::MphRead::ModelMetadata(R"(gunMuzzleFlash)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(gunSmoke)", ::MphRead::ModelMetadata(R"(gunSmoke)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(jumpad_ray)", ::MphRead::ModelMetadata(R"(jumpad_ray)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(jumppad_base)", ::MphRead::ModelMetadata(R"(jumppad_base)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(jumppad_ray)", ::MphRead::ModelMetadata(R"(jumppad_ray)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(lightningCol)", ::MphRead::ModelMetadata(R"(lightningCol)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(lightningColLite)", ::MphRead::ModelMetadata(R"(lightningColLite)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(lightningColLiter)", ::MphRead::ModelMetadata(R"(lightningColLiter)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(lightningColLiterER)", ::MphRead::ModelMetadata(R"(lightningColLiterER)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(lightningLob)", ::MphRead::ModelMetadata(R"(lightningLob)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(metroid)", ::MphRead::ModelMetadata(R"(metroid)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(Metroid_Lo)", ::MphRead::ModelMetadata(R"(Metroid_Lo)", R"(models\Metroid_Lo_Model.bin)", std::optional<std::string>{R"(models\metroid_Anim.bin)"}, std::nullopt, false)},
    {R"(missileCollide)", ::MphRead::ModelMetadata(R"(missileCollide)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(missileColLite)", ::MphRead::ModelMetadata(R"(missileColLite)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(missileColLiter)", ::MphRead::ModelMetadata(R"(missileColLiter)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(missileColLiterER)", ::MphRead::ModelMetadata(R"(missileColLiterER)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(Mochtroid)", ::MphRead::ModelMetadata(R"(Mochtroid)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(Mochtroid_Lo)", ::MphRead::ModelMetadata(R"(Mochtroid_Lo)", R"(_Lo)", true, std::nullopt, false, true)},
    {R"(morphBall)", ::MphRead::ModelMetadata(R"(morphBall)", std::vector<std::string>{R"(*morphBall)", R"(Green)", R"(White)", R"(Blue)"}, std::nullopt, false, std::nullopt, false, MdlSuffix::None, std::nullopt, std::nullopt, std::nullopt, false, true, false)},
    {R"(morphBall_Blue)", ::MphRead::ModelMetadata(R"(morphBall_Blue)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(morphBall_Green)", ::MphRead::ModelMetadata(R"(morphBall_Green)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(morphBall_White)", ::MphRead::ModelMetadata(R"(morphBall_White)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pb_charged)", ::MphRead::ModelMetadata(R"(pb_charged)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pb_normal)", ::MphRead::ModelMetadata(R"(pb_normal)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_ammo_A)", ::MphRead::ModelMetadata(R"(pick_ammo_A)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_ammo_B)", ::MphRead::ModelMetadata(R"(pick_ammo_B)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_dblDamage)", ::MphRead::ModelMetadata(R"(pick_dblDamage)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_health_A)", ::MphRead::ModelMetadata(R"(pick_health_A)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_health_B)", ::MphRead::ModelMetadata(R"(pick_health_B)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_morphball)", ::MphRead::ModelMetadata(R"(pick_morphball)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_wpn_electro)", ::MphRead::ModelMetadata(R"(pick_wpn_electro)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(pick_wpn_missile)", ::MphRead::ModelMetadata(R"(pick_wpn_missile)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(platform)", ::MphRead::ModelMetadata(R"(platform)", false, true, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(samus_hi_yellow)", ::MphRead::ModelMetadata(R"(samus_hi_yellow)", std::vector<std::string>{R"(*samus_hi_yellow)", R"(hi_green)", R"(hi_white)", R"(hi_blue)"}, std::optional<std::string>{R"(_hi_yellow)"}, false, std::optional<std::string>{R"(models\samus_Anim.bin)"}, false, MdlSuffix::None, std::nullopt, std::nullopt, std::nullopt, false, true, false)},
    {R"(samus_low_yellow)", ::MphRead::ModelMetadata(R"(samus_low_yellow)", std::vector<std::string>{R"(*samus_low_yellow)", R"(hi_green)", R"(hi_white)", R"(hi_blue)"}, std::optional<std::string>{R"(_low_yellow)"}, false, std::optional<std::string>{R"(models\samus_Anim.bin)"}, false, MdlSuffix::None, std::nullopt, std::nullopt, std::nullopt, false, true, false)},
    {R"(samus_hi_blue)", ::MphRead::ModelMetadata(R"(samus_hi_blue)", R"(_hi_blue)", true, std::nullopt, false, true)},
    {R"(samus_hi_green)", ::MphRead::ModelMetadata(R"(samus_hi_green)", R"(_hi_green)", true, std::nullopt, false, true)},
    {R"(samus_hi_white)", ::MphRead::ModelMetadata(R"(samus_hi_white)", R"(_hi_white)", true, std::nullopt, false, true)},
    {R"(spawnEffect)", ::MphRead::ModelMetadata(R"(spawnEffect)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(trail)", ::MphRead::ModelMetadata(R"(trail)", false, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(warWasp)", ::MphRead::ModelMetadata(R"(warWasp)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
    {R"(zoomer)", ::MphRead::ModelMetadata(R"(zoomer)", true, false, false, std::nullopt, MdlSuffix::None, std::nullopt, std::nullopt, true, std::nullopt, std::nullopt)},
};


const ::MphRead::ModelMetadata* GetModelByName(std::string_view name, MetaDir dir) noexcept
{
    if (name == "doubleDamage_img") return &DoubleDamageImg;
    if (name == "ad2_dm2") return Ad2Dm2.get();
    if (dir == MetaDir::Logo) return FindFrontendModel(LogoModels,name);
    if (dir == MetaDir::Multiplayer) return FindFrontendModel(MultiplayerModels,name);
    if (dir == MetaDir::TouchToStart) return FindFrontendModel(TouchToStartModels,name);
    if (dir == MetaDir::Hud) return FindFrontendModel(HudModels,name);
    if (dir != MetaDir::Models) return FindFrontendModel(FrontendModels,name);
    return FindModel(ModelMetadata,name);
}

const ::MphRead::ModelMetadata* GetFirstHuntModelByName(std::string_view name) noexcept
{
    return FindModel(FirstHuntModels,name);
}

const ::MphRead::ModelMetadata* GetEntityByPath(std::string_view path) noexcept
{
    for (const auto& value : ModelMetadata)
    {
        if (value.second.ModelPath == path) return &value.second;
    }
    return nullptr;
}

const ObjectMetadata& GetObjectById(int id)
{
    if (id < 0 || id > static_cast<int>(Objects.size())) throw std::invalid_argument("id");
    return *Objects.at(static_cast<std::size_t>(id));
}

const ObjectMetadata& GetObjectById(std::uint32_t id)
{
    return GetObjectById(std::bit_cast<std::int32_t>(id));
}

const PlatformMetadata* GetPlatformById(int id)
{
    if (id < 0 || id > static_cast<int>(Platforms.size())) throw std::invalid_argument("id");
    const auto& value=Platforms.at(static_cast<std::size_t>(id));
    return value.get();
}

OpenTK::Mathematics::Vector3 GetEventColor(Message eventId) noexcept
{
    using V=OpenTK::Mathematics::Vector3;
    switch (eventId)
    {
    case Message::None:return V(0,0,0); case Message::SetActive:return V(.615F,0,.909F);
    case Message::Damage:return V(1,0,0); case Message::Gravity:return V(.141F,1,1);
    case Message::Activate:return V(0,1,0); case Message::Death:return V(0,0,.858F);
    case Message::ShipHatch:return V(1,1,.6F); case Message::Unused25:return V(1,.792F,.6F);
    case Message::PreventFormSwitch:return V(.964F,1,.058F); case Message::PlatformWakeup:return V(.5F,.5F,.5F);
    case Message::DripMoatPlatform:return V(.596F,.658F,.964F); case Message::UnlockOubliette:return V(.964F,.596F,.596F);
    case Message::Checkpoint:return V(.972F,.086F,.831F); case Message::EscapeUpdate1:return V(.619F,.980F,.678F);
    case Message::Trigger:return V(.549F,.18F,.18F); case Message::UpdateMusic:return V(.094F,.506F,.51F);
    case Message::Unlock:return V(.094F,.094F,.557F); case Message::Lock:return V(.647F,.663F,.169F);
    case Message::ShowPrompt:return V(.118F,.588F,.118F); case Message::ShowWarning:return V(.784F,.325F,1);
    case Message::ShowOverlay:return V(1,.612F,.153F); case Message::UnlockConnectors:return V(.906F,.702F,1);
    case Message::LockConnectors:return V(.784F,.984F,.988F); case Message::Gorea2Trigger:return V(1,.325F,.294F);
    case Message::SetTriggerState:return V(.988F,.463F,.824F); case Message::PlatformSleep:return V(.165F,.894F,.678F);
    case Message::SetPlatformIndex:return V(.549F,.345F,.102F); case Message::PlaySfxScript:return V(.471F,.769F,.525F);
    case Message::LoadOubliette:return V(1,.765F,.49F); case Message::EscapeUpdate2:return V(.165F,.816F,.894F);
    default:return V(1,1,1);
    }
}

OpenTK::Mathematics::Vector3 GetEventColor(FhMessage eventId) noexcept
{
    using V=OpenTK::Mathematics::Vector3;
    switch(eventId)
    {
    case FhMessage::None:return V(0,0,0); case FhMessage::Activate:return V(0,1,0);
    case FhMessage::Unlock:return V(.094F,.094F,.557F); case FhMessage::SetActive:return V(.615F,0,.909F);
    case FhMessage::Death:return V(0,0,.858F); default:return V(1,1,1);
    }
}

std::pair<const ::MphRead::RoomMetadata*,int> GetRoomByName(std::string_view name)
{
    auto it=RoomMetadata.find(std::string(name));
    if (it==RoomMetadata.end()) return {nullptr,-1};
    int id=-1;
    for(std::size_t i=0;i<_roomIds.size();++i)
    {
        if(_roomIds[i]==it->second->Name){id=static_cast<int>(i);break;}
    }
    return {it->second.get(),id};
}

const ::MphRead::RoomMetadata* GetRoomById(int id, bool noThrow)
{
    if(id<0 || id>static_cast<int>(_roomIds.size()))
    {
        if(noThrow) return nullptr;
        throw std::invalid_argument("id");
    }
    const std::string& key=_roomIds.at(static_cast<std::size_t>(id));
    auto it=RoomMetadata.find(key);
    return it==RoomMetadata.end()?nullptr:it->second.get();
}

int GetAreaInfo(int roomId) noexcept
{
    int areaId=8;
    if(roomId>=27 && roomId<36) areaId=0;
    else if(roomId>=36 && roomId<45) areaId=1;
    else if(roomId>=45 && roomId<56) areaId=2;
    else if(roomId>=56 && roomId<65) areaId=3;
    else if(roomId>=65 && roomId<72) areaId=4;
    else if(roomId>=72 && roomId<77) areaId=5;
    else if(roomId>=77 && roomId<83) areaId=6;
    else if(roomId>=83 && roomId<89) areaId=7;
    return areaId;
}
}

namespace MphRead
{
namespace
{
template <typename T>
std::shared_ptr<std::vector<T>> MakeVector(std::array<T, 2> values)
{
    return std::make_shared<std::vector<T>>(values.begin(), values.end());
}

struct WeaponSpec
{
    std::string description;
    BeamType beam;
    BeamType beamKind;
    std::array<std::uint8_t, 2> drawFuncIds;
    std::array<std::uint16_t, 2> colors;
    std::uint8_t priority;
    WeaponFlags flags;
    std::uint16_t splashDamage;
    std::uint16_t minChargeSplashDamage;
    std::uint16_t chargedSplashDamage;
    std::array<std::uint8_t, 2> splashDmgTypes;
    std::uint8_t shotCooldown;
    std::uint8_t autofireCooldown;
    std::uint8_t ammoType;
    std::array<std::uint8_t, 2> colEffects;
    std::array<std::uint8_t, 2> muzzleEffects;
    std::array<std::uint8_t, 2> dmgDirTypes;
    std::array<std::uint8_t, 2> dmgInterp;
    std::array<Affliction, 2> afflictions;
    std::uint8_t padding21;
    std::uint16_t minCharge;
    std::uint16_t fullCharge;
    std::uint16_t ammoCost;
    std::uint16_t minChargeCost;
    std::uint16_t chargeCost;
    std::uint16_t unchargedDamage;
    std::uint16_t minChargeDamage;
    std::uint16_t chargedDamage;
    std::uint16_t headshotDamage;
    std::uint16_t minChargeHeadshotDamage;
    std::uint16_t chargedHeadshotDamage;
    std::uint16_t unchargedLifespan;
    std::uint16_t minChargeLifespan;
    std::uint16_t chargedLifespan;
    std::array<std::uint16_t, 2> speedDecay;
    std::uint16_t padding42;
    std::array<std::uint16_t, 2> speedInterp;
    std::int32_t unchargedDmgDirMag;
    std::int32_t minChargeDmgDirMag;
    std::int32_t chargedDmgDirMag;
    std::int32_t zoomFov;
    std::int32_t unchargedCylRadius;
    std::int32_t minChargeCylRadius;
    std::int32_t chargedCylRadius;
    std::int32_t unchargedSpeed;
    std::int32_t minChargeSpeed;
    std::int32_t chargedSpeed;
    std::int32_t unchargedFinalSpeed;
    std::int32_t minChargeFinalSpeed;
    std::int32_t chargedFinalSpeed;
    std::int32_t unchargedGravity;
    std::int32_t minChargeGravity;
    std::int32_t chargedGravity;
    std::int32_t unchargedHoming;
    std::int32_t minChargeHoming;
    std::int32_t chargedHoming;
    std::int32_t homingRange;
    std::int32_t homingTolerance;
    std::int32_t unchargedSplashRadius;
    std::int32_t minChargeSplashRadius;
    std::int32_t chargedSplashRadius;
    std::int32_t unchargedDistance;
    std::int32_t minChargeDistance;
    std::int32_t chargedDistance;
    std::int32_t unchargedSpread;
    std::int32_t minChargeSpread;
    std::int32_t chargedSpread;
    std::int32_t unRicoLossH;
    std::int32_t minRicoLossH;
    std::int32_t chRicoLossH;
    std::int32_t unRicoLossV;
    std::int32_t minRicoLossV;
    std::int32_t chRicoLossV;
    std::int32_t unchargedRicoWeaponIdx = -1;
    std::int32_t chargedRicoWeaponIdx = -1;
    std::uint16_t projectileCount;
    std::uint16_t minChargedProjectileCount;
    std::uint16_t chargeProjectileCount;
    std::uint16_t smokeStart;
    std::uint16_t smokeMinimum;
    std::uint16_t smokeDrain;
    std::uint16_t smokeShotAmount;
    std::uint16_t smokeChargeAmount;
};

std::shared_ptr<WeaponInfo> MakeWeapon(const WeaponSpec& s)
{
    return std::make_shared<WeaponInfo>(s.beam, s.beamKind, MakeVector(s.drawFuncIds), MakeVector(s.colors),
        s.priority, s.flags, s.splashDamage, s.minChargeSplashDamage, s.chargedSplashDamage,
        MakeVector(s.splashDmgTypes), s.shotCooldown, s.autofireCooldown, s.ammoType,
        MakeVector(s.colEffects), MakeVector(s.muzzleEffects), MakeVector(s.dmgDirTypes), MakeVector(s.dmgInterp),
        MakeVector(s.afflictions), s.padding21, s.minCharge, s.fullCharge, s.ammoCost, s.minChargeCost, s.chargeCost,
        s.unchargedDamage, s.minChargeDamage, s.chargedDamage, s.headshotDamage, s.minChargeHeadshotDamage,
        s.chargedHeadshotDamage, s.unchargedLifespan, s.minChargeLifespan, s.chargedLifespan,
        MakeVector(s.speedDecay), s.padding42, MakeVector(s.speedInterp), s.unchargedDmgDirMag,
        s.minChargeDmgDirMag, s.chargedDmgDirMag, s.zoomFov, s.unchargedCylRadius, s.minChargeCylRadius,
        s.chargedCylRadius, s.unchargedSpeed, s.minChargeSpeed, s.chargedSpeed, s.unchargedFinalSpeed,
        s.minChargeFinalSpeed, s.chargedFinalSpeed, s.unchargedGravity, s.minChargeGravity, s.chargedGravity,
        s.unchargedHoming, s.minChargeHoming, s.chargedHoming, s.homingRange, s.homingTolerance,
        s.unchargedSplashRadius, s.minChargeSplashRadius, s.chargedSplashRadius, s.unchargedDistance,
        s.minChargeDistance, s.chargedDistance, s.unchargedSpread, s.minChargeSpread, s.chargedSpread,
        s.unRicoLossH, s.minRicoLossH, s.chRicoLossH, s.unRicoLossV, s.minRicoLossV, s.chRicoLossV,
        s.projectileCount, s.minChargedProjectileCount, s.chargeProjectileCount, s.smokeStart, s.smokeMinimum,
        s.smokeDrain, s.smokeShotAmount, s.smokeChargeAmount, s.description,
        s.unchargedRicoWeaponIdx, s.chargedRicoWeaponIdx);
}

std::shared_ptr<Weapons::WeaponList> MakeWeaponList(const std::vector<WeaponSpec>& specs)
{
    auto values = std::make_shared<Weapons::WeaponList>();
    values->reserve(specs.size());
    for (const WeaponSpec& spec : specs)
    {
        values->push_back(MakeWeapon(spec));
    }
    return values;
}
}

EquipInfo::EquipInfo(std::shared_ptr<WeaponInfo> weapon, std::shared_ptr<BeamProjectileArray> beams)
    : Weapon(std::move(weapon)), Beams(std::move(beams))
{
}

const WeaponInfo& EquipInfo::RequireWeapon() const
{
    if (!Weapon)
    {
        throw std::runtime_error("Weapon");
    }
    return *Weapon;
}

int EquipInfo::Ammo() const
{
    if (!InfiniteAmmo && GetAmmo)
    {
        return GetAmmo();
    }
    return std::numeric_limits<std::int32_t>::max();
}

void EquipInfo::Ammo(int value)
{
    if (!InfiniteAmmo && SetAmmo)
    {
        SetAmmo(value);
    }
}

std::uint16_t EquipInfo::UnchargedDamage() const { return _unchargedDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().UnchargedDamage : _unchargedDamage; }
void EquipInfo::UnchargedDamage(std::uint16_t value) noexcept { _unchargedDamage = value; }
std::uint16_t EquipInfo::MinChargeDamage() const { return _minChargeDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().MinChargeDamage : _minChargeDamage; }
void EquipInfo::MinChargeDamage(std::uint16_t value) noexcept { _minChargeDamage = value; }
std::uint16_t EquipInfo::ChargedDamage() const { return _chargedDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().ChargedDamage : _chargedDamage; }
void EquipInfo::ChargedDamage(std::uint16_t value) noexcept { _chargedDamage = value; }
std::uint16_t EquipInfo::HeadshotDamage() const { return _headshotDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().HeadshotDamage : _headshotDamage; }
void EquipInfo::HeadshotDamage(std::uint16_t value) noexcept { _headshotDamage = value; }
std::uint16_t EquipInfo::MinChargeHeadshotDamage() const { return _minChargeHeadshotDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().MinChargeHeadshotDamage : _minChargeHeadshotDamage; }
void EquipInfo::MinChargeHeadshotDamage(std::uint16_t value) noexcept { _minChargeHeadshotDamage = value; }
std::uint16_t EquipInfo::ChargedHeadshotDamage() const { return _chargedHeadshotDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().ChargedHeadshotDamage : _chargedHeadshotDamage; }
void EquipInfo::ChargedHeadshotDamage(std::uint16_t value) noexcept { _chargedHeadshotDamage = value; }
std::uint16_t EquipInfo::SplashDamage() const { return _splashDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().SplashDamage : _splashDamage; }
void EquipInfo::SplashDamage(std::uint16_t value) noexcept { _splashDamage = value; }
std::uint16_t EquipInfo::MinChargeSplashDamage() const { return _minChargeSplashDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().MinChargeSplashDamage : _minChargeSplashDamage; }
void EquipInfo::MinChargeSplashDamage(std::uint16_t value) noexcept { _minChargeSplashDamage = value; }
std::uint16_t EquipInfo::ChargedSplashDamage() const { return _chargedSplashDamage == std::numeric_limits<std::uint16_t>::max() ? RequireWeapon().ChargedSplashDamage : _chargedSplashDamage; }
void EquipInfo::ChargedSplashDamage(std::uint16_t value) noexcept { _chargedSplashDamage = value; }
std::int32_t EquipInfo::HomingTolerance() const { return _homingTolerance == std::numeric_limits<std::int32_t>::max() ? RequireWeapon().HomingTolerance : _homingTolerance; }
void EquipInfo::HomingTolerance(std::int32_t value) noexcept { _homingTolerance = value; }

WeaponInfo::WeaponInfo(BeamType beam, BeamType beamKind,
    std::shared_ptr<std::vector<std::uint8_t>> drawFuncIds,
    std::shared_ptr<std::vector<std::uint16_t>> colors, std::uint8_t priority, WeaponFlags flags,
    std::uint16_t splashDamage, std::uint16_t minChargeSplashDamage, std::uint16_t chargedSplashDamage,
    std::shared_ptr<std::vector<std::uint8_t>> splashDmgTypes, std::uint8_t shotCooldown,
    std::uint8_t autofireCooldown, std::uint8_t ammoType, std::shared_ptr<std::vector<std::uint8_t>> colEffects,
    std::shared_ptr<std::vector<std::uint8_t>> muzzleEffects, std::shared_ptr<std::vector<std::uint8_t>> dmgDirTypes,
    std::shared_ptr<std::vector<std::uint8_t>> dmgInterp, std::shared_ptr<std::vector<Affliction>> afflictions,
    std::uint8_t padding21, std::uint16_t minCharge, std::uint16_t fullCharge, std::uint16_t ammoCost,
    std::uint16_t minChargeCost, std::uint16_t chargeCost, std::uint16_t unchargedDamage,
    std::uint16_t minChargeDamage, std::uint16_t chargedDamage, std::uint16_t headshotDamage,
    std::uint16_t minChargeHeadshotDamage, std::uint16_t chargedHeadshotDamage,
    std::uint16_t unchargedLifespan, std::uint16_t minChargeLifespan, std::uint16_t chargedLifespan,
    std::shared_ptr<std::vector<std::uint16_t>> speedDecay, std::uint16_t padding42,
    std::shared_ptr<std::vector<std::uint16_t>> speedInterp, std::int32_t unchargedDmgDirMag,
    std::int32_t minChargeDmgDirMag, std::int32_t chargedDmgDirMag, std::int32_t zoomFov,
    std::int32_t unchargedCylRadius, std::int32_t minChargeCylRadius, std::int32_t chargedCylRadius,
    std::int32_t unchargedSpeed, std::int32_t minChargeSpeed, std::int32_t chargedSpeed,
    std::int32_t unchargedFinalSpeed, std::int32_t minChargeFinalSpeed, std::int32_t chargedFinalSpeed,
    std::int32_t unchargedGravity, std::int32_t minChargeGravity, std::int32_t chargedGravity,
    std::int32_t unchargedHoming, std::int32_t minChargeHoming, std::int32_t chargedHoming,
    std::int32_t homingRange, std::int32_t homingTolerance, std::int32_t unchargedSplashRadius,
    std::int32_t minChargeSplashRadius, std::int32_t chargedSplashRadius, std::int32_t unchargedDistance,
    std::int32_t minChargeDistance, std::int32_t chargedDistance, std::int32_t unchargedSpread,
    std::int32_t minChargeSpread, std::int32_t chargedSpread, std::int32_t unRicoLossH,
    std::int32_t minRicoLossH, std::int32_t chRicoLossH, std::int32_t unRicoLossV,
    std::int32_t minRicoLossV, std::int32_t chRicoLossV, std::uint16_t projectileCount,
    std::uint16_t minChargedProjectileCount, std::uint16_t chargeProjectileCount, std::uint16_t smokeStart,
    std::uint16_t smokeMinimum, std::uint16_t smokeDrain, std::uint16_t smokeShotAmount,
    std::uint16_t smokeChargeAmount, std::string description, std::int32_t unchargedRicoWeaponIdx,
    std::int32_t chargedRicoWeaponIdx)
    : _unchargedRicoWeaponIdx(unchargedRicoWeaponIdx), _chargedRicoWeaponIdx(chargedRicoWeaponIdx),
      Beam(beam), BeamKind(beamKind), DrawFuncIds(std::move(drawFuncIds)), Colors(std::move(colors)),
      Priority(priority), Flags(flags), SplashDamage(splashDamage), MinChargeSplashDamage(minChargeSplashDamage),
      ChargedSplashDamage(chargedSplashDamage), SplashDamageTypes(std::move(splashDmgTypes)),
      ShotCooldown(shotCooldown), AutofireCooldown(autofireCooldown), AmmoType(ammoType),
      CollisionEffects(std::move(colEffects)), MuzzleEffects(std::move(muzzleEffects)),
      DmgDirTypes(std::move(dmgDirTypes)), DamageInterpolations(std::move(dmgInterp)),
      Afflictions(std::move(afflictions)), Padding21(padding21), MinCharge(minCharge), FullCharge(fullCharge),
      AmmoCost(ammoCost), MinChargeCost(minChargeCost), ChargeCost(chargeCost), UnchargedDamage(unchargedDamage),
      MinChargeDamage(minChargeDamage), ChargedDamage(chargedDamage), HeadshotDamage(headshotDamage),
      MinChargeHeadshotDamage(minChargeHeadshotDamage), ChargedHeadshotDamage(chargedHeadshotDamage),
      UnchargedLifespan(unchargedLifespan), MinChargeLifespan(minChargeLifespan), ChargedLifespan(chargedLifespan),
      SpeedDecayTimes(std::move(speedDecay)), Padding42(padding42), SpeedInterpolations(std::move(speedInterp)),
      UnchargedDmgDirMag(unchargedDmgDirMag), MinChargeDmgDirMag(minChargeDmgDirMag),
      ChargedDmgDirMag(chargedDmgDirMag), ZoomFov(zoomFov), UnchargedCylRadius(unchargedCylRadius),
      MinChargeCylRadius(minChargeCylRadius), ChargedCylRadius(chargedCylRadius), UnchargedSpeed(unchargedSpeed),
      MinChargeSpeed(minChargeSpeed), ChargedSpeed(chargedSpeed), UnchargedFinalSpeed(unchargedFinalSpeed),
      MinChargeFinalSpeed(minChargeFinalSpeed), ChargedFinalSpeed(chargedFinalSpeed), UnchargedGravity(unchargedGravity),
      MinChargeGravity(minChargeGravity), ChargedGravity(chargedGravity), UnchargedHoming(unchargedHoming),
      MinChargeHoming(minChargeHoming), ChargedHoming(chargedHoming), HomingRange(homingRange),
      HomingTolerance(homingTolerance), UnchargedSplashRadius(unchargedSplashRadius),
      MinChargeSplashRadius(minChargeSplashRadius), ChargedSplashRadius(chargedSplashRadius),
      UnchargedDistance(unchargedDistance), MinChargeDistance(minChargeDistance), ChargedDistance(chargedDistance),
      UnchargedSpread(unchargedSpread), MinChargeSpread(minChargeSpread), ChargedSpread(chargedSpread),
      UnchargedRicochetLossH(unRicoLossH), MinChargeRicochetLossH(minRicoLossH), ChargedRicochetLossH(chRicoLossH),
      UnchargedRicochetLossV(unRicoLossV), MinChargeRicochetLossV(minRicoLossV), ChargedRicochetLossV(chRicoLossV),
      Projectiles(projectileCount), MinChargeProjectiles(minChargedProjectileCount), ChargedProjectiles(chargeProjectileCount),
      SmokeStart(smokeStart), SmokeMinimum(smokeMinimum), SmokeDrain(smokeDrain), SmokeShotAmount(smokeShotAmount),
      SmokeChargeAmount(smokeChargeAmount), Description(std::move(description))
{
}

const std::string& WeaponInfo::Name() const
{
    return Metadata::WeaponNames.at(static_cast<std::size_t>(Beam));
}

std::shared_ptr<WeaponInfo> WeaponInfo::UnchargedRicochetWeapon() const
{
    return _unchargedRicoWeaponIdx == -1 ? nullptr
        : Weapons::Ricochets->at(static_cast<std::size_t>(_unchargedRicoWeaponIdx));
}

std::shared_ptr<WeaponInfo> WeaponInfo::ChargedRicochetWeapon() const
{
    return _chargedRicoWeaponIdx == -1 ? nullptr
        : Weapons::Ricochets->at(static_cast<std::size_t>(_chargedRicoWeaponIdx));
}

namespace Weapons
{
const std::vector<BeamType> AffinityWeapons{
    BeamType::Missile, BeamType::VoltDriver, BeamType::Imperialist, BeamType::ShockCoil,
    BeamType::Judicator, BeamType::Magmaul, BeamType::Battlehammer, BeamType::PowerBeam
};

BeamType GetAffinityBeam(Hunter hunter)
{
    return AffinityWeapons.at(static_cast<std::size_t>(hunter));
}

std::shared_ptr<const WeaponList> Current;

namespace
{
std::vector<WeaponSpec> BuildWeapons1PSpecs()
{
    using A = Affliction;
    using B = BeamType;
    using F = WeaponFlags;
    std::vector<WeaponSpec> v;
    v.reserve(18);
    v.push_back({
        .description="Power Beam 1P", .beam=B::PowerBeam, .beamKind=B::PowerBeam,
        .drawFuncIds={0,0}, .colors={9055,21407}, .priority=1,
        .flags=F::PartialCharge|F::CanCharge|F::RepeatFire|F::SurfaceCollision,
        .splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},
        .shotCooldown=5,.autofireCooldown=5,.ammoType=0,.colEffects={4,95},.muzzleEffects={65,65},
        .dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=18,.fullCharge=30,.ammoCost=0,.minChargeCost=0,.chargeCost=0,
        .unchargedDamage=6,.minChargeDamage=6,.chargedDamage=36,.headshotDamage=8,.minChargeHeadshotDamage=8,.chargedHeadshotDamage=48,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=1228,.zoomFov=40960,
        .unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=819,
        .unchargedSpeed=12288,.minChargeSpeed=6144,.chargedSpeed=6144,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,
        .homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=75,.smokeMinimum=25,.smokeDrain=5,.smokeShotAmount=10,.smokeChargeAmount=75});
    v.push_back({
        .description="Volt Driver 1P",.beam=B::VoltDriver,.beamKind=B::VoltDriver,.drawFuncIds={1,2},.colors={32767,32767},.priority=2,
        .flags=F::CanCharge|F::ForceEffectCharged|F::SurfaceCollision,
        .splashDamage=0,.minChargeSplashDamage=56,.chargedSplashDamage=56,.splashDmgTypes={2,2},.shotCooldown=5,.autofireCooldown=5,.ammoType=0,
        .colEffects={89,174},.muzzleEffects={60,60},.dmgDirTypes={0,0},.dmgInterp={2,2},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=10,.minChargeCost=30,.chargeCost=30,
        .unchargedDamage=14,.minChargeDamage=56,.chargedDamage=56,.headshotDamage=21,.minChargeHeadshotDamage=56,.chargedHeadshotDamage=56,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,7},.padding42=7,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=1638,.chargedDmgDirMag=1638,.zoomFov=0,.unchargedCylRadius=409,.minChargeCylRadius=1638,.chargedCylRadius=1638,
        .unchargedSpeed=20480,.minChargeSpeed=7168,.chargedSpeed=7168,.unchargedFinalSpeed=0,.minChargeFinalSpeed=2048,.chargedFinalSpeed=2048,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,
        .homingRange=204800,.homingTolerance=3849,.unchargedSplashRadius=1024,.minChargeSplashRadius=10240,.chargedSplashRadius=10240,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=100,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({
        .description="Missile 1P",.beam=B::Missile,.beamKind=B::Missile,.drawFuncIds={7,7},.colors={32140,32140},.priority=2,
        .flags=F::CanCharge|F::SurfaceCollision,
        .splashDamage=24,.minChargeSplashDamage=32,.chargedSplashDamage=32,.splashDmgTypes={3,3},.shotCooldown=20,.autofireCooldown=20,.ammoType=1,
        .colEffects={8,193},.muzzleEffects={65,65},.dmgDirTypes={2,2},.dmgInterp={2,2},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=45,.ammoCost=10,.minChargeCost=30,.chargeCost=30,
        .unchargedDamage=32,.minChargeDamage=48,.chargedDamage=48,.headshotDamage=32,.minChargeHeadshotDamage=48,.chargedHeadshotDamage=48,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={7,7},.padding42=7,.speedInterp={2,2},
        .unchargedDmgDirMag=1638,.minChargeDmgDirMag=2457,.chargedDmgDirMag=2457,.zoomFov=40960,.unchargedCylRadius=1433,.minChargeCylRadius=1433,.chargedCylRadius=1433,
        .unchargedSpeed=1024,.minChargeSpeed=1024,.chargedSpeed=1024,.unchargedFinalSpeed=6144,.minChargeFinalSpeed=6144,.chargedFinalSpeed=6144,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,
        .homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=6144,.minChargeSplashRadius=9216,.chargedSplashRadius=9216,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=75,.smokeMinimum=25,.smokeDrain=5,.smokeShotAmount=250,.smokeChargeAmount=25});
    v.push_back({
        .description="Battlehammer 1P",.beam=B::Battlehammer,.beamKind=B::Battlehammer,.drawFuncIds={10,10},.colors={16367,16367},.priority=2,
        .flags=F::RepeatFire|F::SurfaceCollision,.splashDamage=8,.minChargeSplashDamage=8,.chargedSplashDamage=8,.splashDmgTypes={3,3},
        .shotCooldown=10,.autofireCooldown=10,.ammoType=0,.colEffects={176,176},.muzzleEffects={63,63},.dmgDirTypes={2,2},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=10,.minChargeCost=30,.chargeCost=30,.unchargedDamage=12,.minChargeDamage=12,.chargedDamage=12,.headshotDamage=12,.minChargeHeadshotDamage=12,.chargedHeadshotDamage=12,
        .unchargedLifespan=60,.minChargeLifespan=60,.chargedLifespan=60,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=819,.minChargeDmgDirMag=819,.chargedDmgDirMag=819,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=4915,.minChargeSpeed=4915,.chargedSpeed=4915,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=-122,.minChargeGravity=-122,.chargedGravity=-122,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,
        .homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=6144,.minChargeSplashRadius=6144,.chargedSplashRadius=6144,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=1,.smokeMinimum=1,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({
        .description="Imperialist 1P",.beam=B::Imperialist,.beamKind=B::Imperialist,.drawFuncIds={8,8},.colors={32767,32767},.priority=2,
        .flags=F::RepeatFire|F::CanZoom|F::SurfaceCollision,.splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},
        .shotCooldown=60,.autofireCooldown=60,.ammoType=0,.colEffects={31,31},.muzzleEffects={66,66},.dmgDirTypes={0,0},.dmgInterp={3,3},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=10,.fullCharge=90,.ammoCost=10,.minChargeCost=30,.chargeCost=30,.unchargedDamage=72,.minChargeDamage=72,.chargedDamage=72,.headshotDamage=200,.minChargeHeadshotDamage=200,.chargedHeadshotDamage=200,
        .unchargedLifespan=2,.minChargeLifespan=2,.chargedLifespan=2,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=49152,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=819200,.minChargeSpeed=819200,.chargedSpeed=819200,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=204800,.homingTolerance=3896,
        .unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,.unchargedDistance=819200,.minChargeDistance=819200,.chargedDistance=819200,
        .unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=200,.smokeMinimum=0,.smokeDrain=10,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({
        .description="Judicator 1P",.beam=B::Judicator,.beamKind=B::Judicator,.drawFuncIds={3,3},.colors={32404,32404},.priority=2,
        .flags=F::CanCharge|F::SelfDamageUncharged|F::SurfaceCollision,.splashDamage=12,.minChargeSplashDamage=10,.chargedSplashDamage=10,.splashDmgTypes={0,0},
        .shotCooldown=15,.autofireCooldown=15,.ammoType=0,.colEffects={10,10},.muzzleEffects={62,62},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=10,.minChargeCost=30,.chargeCost=30,.unchargedDamage=24,.minChargeDamage=24,.chargedDamage=24,.headshotDamage=32,.minChargeHeadshotDamage=32,.chargedHeadshotDamage=32,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=819,.minChargeDmgDirMag=1228,.chargedDmgDirMag=1228,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=8192,.minChargeSpeed=8192,.chargedSpeed=8192,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=102400,.homingTolerance=3896,
        .unchargedSplashRadius=1024,.minChargeSplashRadius=1024,.chargedSplashRadius=1024,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,
        .unchargedSpread=0,.minChargeSpread=30720,.chargedSpread=30720,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=0,.chargedRicoWeaponIdx=0,.projectileCount=1,.minChargedProjectileCount=3,.chargeProjectileCount=3,
        .smokeStart=1,.smokeMinimum=1,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({
        .description="Magmaul 1P",.beam=B::Magmaul,.beamKind=B::Magmaul,.drawFuncIds={4,5},.colors={15711,15711},.priority=2,
        .flags=F::CanCharge|F::RicochetUncharged|F::SelfDamageUncharged|F::ForceEffectUncharged|F::SurfaceCollision,
        .splashDamage=16,.minChargeSplashDamage=28,.chargedSplashDamage=28,.splashDmgTypes={3,3},.shotCooldown=20,.autofireCooldown=15,.ammoType=0,
        .colEffects={9,194},.muzzleEffects={64,64},.dmgDirTypes={2,2},.dmgInterp={2,2},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=10,.minChargeCost=30,.chargeCost=30,.unchargedDamage=32,.minChargeDamage=56,.chargedDamage=56,.headshotDamage=32,.minChargeHeadshotDamage=56,.chargedHeadshotDamage=56,
        .unchargedLifespan=45,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={7,7},.padding42=7,.speedInterp={0,0},
        .unchargedDmgDirMag=2048,.minChargeDmgDirMag=2867,.chargedDmgDirMag=2867,.zoomFov=40960,.unchargedCylRadius=1433,.minChargeCylRadius=1433,.chargedCylRadius=1433,
        .unchargedSpeed=6963,.minChargeSpeed=6963,.chargedSpeed=6963,.unchargedFinalSpeed=2867,.minChargeFinalSpeed=2867,.chargedFinalSpeed=2867,
        .unchargedGravity=-122,.minChargeGravity=-122,.chargedGravity=-122,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=40960,.homingTolerance=3896,
        .unchargedSplashRadius=8192,.minChargeSplashRadius=10240,.chargedSplashRadius=10240,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,
        .unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=2867,.minRicoLossH=2867,.chRicoLossH=2867,.unRicoLossV=1843,.minRicoLossV=1843,.chRicoLossV=1843,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=50,.smokeMinimum=0,.smokeDrain=2,.smokeShotAmount=200,.smokeChargeAmount=500});
    v.push_back({
        .description="Shock Coil 1P",.beam=B::ShockCoil,.beamKind=B::ShockCoil,.drawFuncIds={9,9},.colors={32767,32767},.priority=2,
        .flags=F::RepeatFire|F::Continuous|F::SurfaceCollision,.splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},
        .shotCooldown=0,.autofireCooldown=0,.ammoType=0,.colEffects={255,255},.muzzleEffects={62,62},.dmgDirTypes={3,3},.dmgInterp={3,3},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=45,.ammoCost=10,.minChargeCost=30,.chargeCost=30,.unchargedDamage=10,.minChargeDamage=10,.chargedDamage=10,.headshotDamage=10,.minChargeHeadshotDamage=10,.chargedHeadshotDamage=10,
        .unchargedLifespan=2,.minChargeLifespan=2,.chargedLifespan=2,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=49152,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=28672,.minChargeSpeed=28672,.chargedSpeed=28672,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=409,.minChargeHoming=409,.chargedHoming=409,.homingRange=61440,.homingTolerance=3547,
        .unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,.unchargedDistance=61440,.minChargeDistance=61440,.chargedDistance=61440,
        .unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=1638,.minRicoLossH=1638,.chRicoLossH=1638,.unRicoLossV=1228,.minRicoLossV=1228,.chRicoLossV=1228,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=1000,.smokeMinimum=0,.smokeDrain=0,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({
        .description="Omega Cannon 1P",.beam=B::OmegaCannon,.beamKind=B::OmegaCannon,.drawFuncIds={11,11},.colors={32767,32767},.priority=5,
        .flags=F::SurfaceCollision,.splashDamage=200,.minChargeSplashDamage=200,.chargedSplashDamage=200,.splashDmgTypes={3,3},
        .shotCooldown=60,.autofireCooldown=60,.ammoType=0,.colEffects={248,248},.muzzleEffects={60,60},.dmgDirTypes={0,0},.dmgInterp={3,3},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=300,.ammoCost=10,.minChargeCost=30,.chargeCost=30,.unchargedDamage=200,.minChargeDamage=200,.chargedDamage=200,.headshotDamage=200,.minChargeHeadshotDamage=200,.chargedHeadshotDamage=200,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={7,7},.padding42=7,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=0,.unchargedCylRadius=1433,.minChargeCylRadius=1433,.chargedCylRadius=1433,
        .unchargedSpeed=2048,.minChargeSpeed=2048,.chargedSpeed=2048,.unchargedFinalSpeed=819,.minChargeFinalSpeed=819,.chargedFinalSpeed=819,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=204800,.homingTolerance=3896,
        .unchargedSplashRadius=102400,.minChargeSplashRadius=102400,.chargedSplashRadius=102400,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,
        .unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,
        .smokeStart=100,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0});
    {
        WeaponSpec s=v.at(0); s.description="Power Beam 1P Affinity"; s.shotCooldown=4; s.autofireCooldown=4;
        s.chargedDamage=40; s.chargedHeadshotDamage=52; s.chargedHoming=81; v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(1); s.description="Volt Driver 1P Affinity"; s.colEffects={89,88}; s.dmgInterp={0,0};
        s.afflictions={A::None,A::Disrupt}; s.minChargeFinalSpeed=1228; s.chargedFinalSpeed=1228;
        s.minChargeHoming=40; s.chargedHoming=40; s.homingRange=409600; s.homingTolerance=3937; v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(2); s.description="Missile 1P Affinity"; s.colors={32050,32050};
        s.unchargedHoming=12; s.minChargeHoming=12; s.chargedHoming=81; s.homingRange=204800; s.homingTolerance=3937;
        v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(3); s.description="Battlehammer 1P Affinity"; s.splashDamage=12; s.minChargeSplashDamage=12; s.chargedSplashDamage=12;
        s.shotCooldown=15; s.autofireCooldown=15; s.colEffects={14,14}; s.unchargedDamage=18; s.minChargeDamage=18; s.chargedDamage=18;
        s.headshotDamage=18; s.minChargeHeadshotDamage=18; s.chargedHeadshotDamage=18; s.unchargedLifespan=90; s.minChargeLifespan=90; s.chargedLifespan=90;
        s.unchargedDmgDirMag=2048; s.minChargeDmgDirMag=2048; s.chargedDmgDirMag=2048; s.unchargedCylRadius=1228; s.minChargeCylRadius=1228; s.chargedCylRadius=1228;
        s.unchargedGravity=-163; s.minChargeGravity=-163; s.chargedGravity=-163; s.unchargedSplashRadius=10240; s.minChargeSplashRadius=10240; s.chargedSplashRadius=10240;
        s.smokeStart=100; s.smokeMinimum=50; s.smokeDrain=10; s.smokeShotAmount=10; v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(4); s.description="Imperialist 1P Affinity"; v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(5); s.description="Judicator 1P Affinity"; s.flags=F::CanCharge|F::SelfDamageUncharged|F::AoeCharged|F::SurfaceCollision;
        s.minChargeSplashDamage=0; s.chargedSplashDamage=0; s.colEffects={10,0}; s.afflictions={A::None,A::Freeze};
        s.minChargeDamage=12; s.chargedDamage=12; s.minChargeHeadshotDamage=12; s.chargedHeadshotDamage=12;
        s.minChargeLifespan=5; s.chargedLifespan=5; s.minChargeDmgDirMag=3686; s.chargedDmgDirMag=3686;
        s.minChargeSpeed=4096; s.chargedSpeed=4096; s.minChargeDistance=15360; s.chargedDistance=15360;
        s.minChargeSpread=245760; s.chargedSpread=245760; s.chargedRicoWeaponIdx=-1; s.minChargedProjectileCount=1; s.chargeProjectileCount=1;
        v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(6); s.description="Magmaul 1P Affinity"; s.minChargeSplashDamage=18; s.chargedSplashDamage=18; s.autofireCooldown=20;
        s.colEffects={9,195}; s.dmgInterp={0,0}; s.afflictions={A::None,A::Burn}; s.minChargeDamage=48; s.chargedDamage=48;
        s.minChargeHeadshotDamage=48; s.chargedHeadshotDamage=48; v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(7); s.description="Shock Coil 1P Affinity"; s.flags=F::RepeatFire|F::Continuous|F::LifeDrainUncharged|F::SurfaceCollision;
        v.push_back(std::move(s));
    }
    {
        WeaponSpec s=v.at(8); s.description="Omega Cannon 1P Affinity"; s.splashDamage=60; s.minChargeSplashDamage=60; s.chargedSplashDamage=60;
        s.ammoCost=0; s.minChargeCost=0; s.chargeCost=0; s.unchargedDamage=60; s.minChargeDamage=60; s.chargedDamage=60;
        s.headshotDamage=60; s.minChargeHeadshotDamage=60; s.chargedHeadshotDamage=60; s.unchargedLifespan=60; s.minChargeLifespan=60; s.chargedLifespan=60;
        s.speedDecay={45,45}; s.padding42=45; s.unchargedCylRadius=409; s.minChargeCylRadius=409; s.chargedCylRadius=409;
        s.unchargedSpeed=4096; s.minChargeSpeed=4096; s.chargedSpeed=4096; s.unchargedFinalSpeed=491; s.minChargeFinalSpeed=491; s.chargedFinalSpeed=491;
        s.minChargeHoming=102; s.chargedHoming=102; s.unchargedSplashRadius=12288; s.minChargeSplashRadius=12288; s.chargedSplashRadius=12288;
        v.push_back(std::move(s));
    }
    return v;
}

std::vector<WeaponSpec> BuildWeaponsMPSpecs()
{
    std::vector<WeaponSpec> v=BuildWeapons1PSpecs();
    for (WeaponSpec& s : v)
    {
        const std::size_t p=s.description.find("1P");
        if (p != std::string::npos) s.description.replace(p,2,"MP");
    }
    auto costs=[&](std::size_t i,std::uint16_t a,std::uint16_t m,std::uint16_t c){v.at(i).ammoCost=a;v.at(i).minChargeCost=m;v.at(i).chargeCost=c;};
    costs(0,0,0,0); costs(1,5,25,25); costs(2,10,15,15); costs(3,4,4,4); costs(4,20,20,20);
    costs(5,5,25,25); costs(6,10,20,20); costs(7,10,10,10); costs(8,0,0,0);
    costs(9,0,0,0); costs(10,5,25,25); costs(11,10,15,15); costs(12,5,5,5); costs(13,20,20,20);
    costs(14,5,25,25); costs(15,10,20,20); costs(16,10,10,10); costs(17,0,0,0);
    v.at(11).unchargedHoming=0; v.at(11).minChargeHoming=81; v.at(11).chargedHoming=81;
    return v;
}

std::vector<WeaponSpec> BuildEnemySpecs()
{
    using A=Affliction; using B=BeamType; using F=WeaponFlags;
    std::vector<WeaponSpec> v;
    v.reserve(11);
    v.push_back({.description="",.beam=B::PowerBeam,.beamKind=B::Enemy,.drawFuncIds={21,21},.colors={9055,21407},.priority=1,
        .flags=F::PartialCharge|F::CanCharge|F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,
        .splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=3,.autofireCooldown=3,.ammoType=0,
        .colEffects={242,242},.muzzleEffects={65,65},.dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=30,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,.minChargeDamage=12,.chargedDamage=36,.headshotDamage=8,.minChargeHeadshotDamage=12,.chargedHeadshotDamage=36,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=2662,.minChargeSpeed=2662,.chargedSpeed=2662,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=20,.chargedHoming=20,.homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({.description="",.beam=B::VoltDriver,.beamKind=B::Enemy,.drawFuncIds={2,2},.colors={32767,32767},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::SurfaceCollision,
        .splashDamage=20,.minChargeSplashDamage=32,.chargedSplashDamage=32,.splashDmgTypes={0,0},.shotCooldown=8,.autofireCooldown=8,.ammoType=0,
        .colEffects={89,88},.muzzleEffects={60,60},.dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::Disrupt,A::Disrupt},.padding21=0,
        .minCharge=15,.fullCharge=45,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=20,.minChargeDamage=32,.chargedDamage=32,.headshotDamage=30,.minChargeHeadshotDamage=32,.chargedHeadshotDamage=32,
        .unchargedLifespan=90,.minChargeLifespan=90,.chargedLifespan=90,.speedDecay={7,7},.padding42=7,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=0,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=3276,.minChargeSpeed=3276,.chargedSpeed=3276,.unchargedFinalSpeed=1146,.minChargeFinalSpeed=1146,.chargedFinalSpeed=1146,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=204800,.homingTolerance=3896,.unchargedSplashRadius=12288,.minChargeSplashRadius=12288,.chargedSplashRadius=12288,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({.description="",.beam=B::Missile,.beamKind=B::Enemy,.drawFuncIds={7,7},.colors={32140,32140},.priority=2,
        .flags=F::PartialCharge|F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,
        .splashDamage=30,.minChargeSplashDamage=30,.chargedSplashDamage=30,.splashDmgTypes={2,2},.shotCooldown=20,.autofireCooldown=20,.ammoType=1,
        .colEffects={8,8},.muzzleEffects={65,65},.dmgDirTypes={2,2},.dmgInterp={2,2},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=30,.minChargeDamage=30,.chargedDamage=30,.headshotDamage=30,.minChargeHeadshotDamage=30,.chargedHeadshotDamage=30,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={7,7},.padding42=7,.speedInterp={2,2},
        .unchargedDmgDirMag=1638,.minChargeDmgDirMag=1638,.chargedDmgDirMag=1638,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=819,.minChargeSpeed=819,.chargedSpeed=819,.unchargedFinalSpeed=10240,.minChargeFinalSpeed=10240,.chargedFinalSpeed=10240,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=40,.chargedHoming=204,.homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=12288,.minChargeSplashRadius=12288,.chargedSplashRadius=12288,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=75,.smokeMinimum=25,.smokeDrain=5,.smokeShotAmount=125,.smokeChargeAmount=25});
    v.push_back({.description="",.beam=B::Battlehammer,.beamKind=B::Enemy,.drawFuncIds={0,0},.colors={16367,16367},.priority=2,
        .flags=F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,
        .splashDamage=2,.minChargeSplashDamage=2,.chargedSplashDamage=2,.splashDmgTypes={3,3},.shotCooldown=8,.autofireCooldown=8,.ammoType=0,
        .colEffects={176,176},.muzzleEffects={63,63},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=2,.minChargeDamage=2,.chargedDamage=2,.headshotDamage=2,.minChargeHeadshotDamage=2,.chargedHeadshotDamage=2,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=2048,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=2867,.minChargeSpeed=4096,.chargedSpeed=4096,.unchargedFinalSpeed=1638,.minChargeFinalSpeed=1638,.chargedFinalSpeed=1638,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=10240,.minChargeSplashRadius=10240,.chargedSplashRadius=10240,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=20480,.minChargeSpread=20480,.chargedSpread=20480,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=50,.smokeDrain=10,.smokeShotAmount=10,.smokeChargeAmount=0});
    v.push_back({.description="",.beam=B::Imperialist,.beamKind=B::Enemy,.drawFuncIds={8,8},.colors={32767,32767},.priority=2,
        .flags=F::RepeatFire|F::CanZoom|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,
        .splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=60,.autofireCooldown=60,.ammoType=0,
        .colEffects={31,31},.muzzleEffects={66,66},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=10,.fullCharge=90,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=99,.minChargeDamage=99,.chargedDamage=99,.headshotDamage=199,.minChargeHeadshotDamage=199,.chargedHeadshotDamage=199,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=2048,.minChargeDmgDirMag=2048,.chargedDmgDirMag=2048,.zoomFov=81920,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=122880,.minChargeSpeed=122880,.chargedSpeed=122880,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=204800,.homingTolerance=3896,.unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=200,.smokeMinimum=0,.smokeDrain=10,.smokeShotAmount=0,.smokeChargeAmount=0});
    v.push_back({.description="",.beam=B::Judicator,.beamKind=B::Enemy,.drawFuncIds={3,6},.colors={32404,32404},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::AoeCharged|F::SurfaceCollision,
        .splashDamage=27,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=15,.autofireCooldown=15,.ammoType=0,
        .colEffects={10,0},.muzzleEffects={62,62},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::Freeze,A::Freeze},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=27,.minChargeDamage=20,.chargedDamage=20,.headshotDamage=36,.minChargeHeadshotDamage=20,.chargedHeadshotDamage=20,
        .unchargedLifespan=40,.minChargeLifespan=15,.chargedLifespan=15,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=409,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=2048,.minChargeSpeed=4096,.chargedSpeed=4096,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=102400,.homingTolerance=3896,.unchargedSplashRadius=2048,.minChargeSplashRadius=4096,.chargedSplashRadius=4096,
        .unchargedDistance=0,.minChargeDistance=14336,.chargedDistance=14336,.unchargedSpread=0,.minChargeSpread=245760,.chargedSpread=245760,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=2,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,.smokeDrain=5,.smokeShotAmount=25,.smokeChargeAmount=50});
    v.push_back({.description="",.beam=B::Magmaul,.beamKind=B::Enemy,.drawFuncIds={22,22},.colors={15711,15711},.priority=2,
        .flags=F::CanCharge|F::SelfDamageUncharged|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,
        .splashDamage=29,.minChargeSplashDamage=58,.chargedSplashDamage=58,.splashDmgTypes={2,2},.shotCooldown=20,.autofireCooldown=20,.ammoType=0,
        .colEffects={9,9},.muzzleEffects={64,64},.dmgDirTypes={2,2},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=45,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=29,.minChargeDamage=58,.chargedDamage=58,.headshotDamage=29,.minChargeHeadshotDamage=58,.chargedHeadshotDamage=58,
        .unchargedLifespan=30,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={15,15},.padding42=15,.speedInterp={2,2},
        .unchargedDmgDirMag=1638,.minChargeDmgDirMag=1638,.chargedDmgDirMag=1638,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=4915,.minChargeSpeed=4915,.chargedSpeed=4915,.unchargedFinalSpeed=2867,.minChargeFinalSpeed=2867,.chargedFinalSpeed=2867,.unchargedGravity=0,.minChargeGravity=-204,.chargedGravity=-204,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=40960,.homingTolerance=3896,.unchargedSplashRadius=12288,.minChargeSplashRadius=12288,.chargedSplashRadius=12288,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=1638,.minRicoLossH=1638,.chRicoLossH=1638,.unRicoLossV=1228,.minRicoLossV=1228,.chRicoLossV=1228,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,.smokeDrain=2,.smokeShotAmount=200,.smokeChargeAmount=500});
    v.push_back({.description="",.beam=B::ShockCoil,.beamKind=B::Enemy,.drawFuncIds={9,9},.colors={32767,32767},.priority=2,
        .flags=F::RepeatFire|F::SelfDamageUncharged|F::ForceEffectUncharged|F::ForceEffectCharged|F::Continuous|F::SurfaceCollision,
        .splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=0,.autofireCooldown=0,.ammoType=0,
        .colEffects={255,255},.muzzleEffects={62,62},.dmgDirTypes={3,3},.dmgInterp={3,3},.afflictions={A::None,A::None},.padding21=0,
        .minCharge=15,.fullCharge=45,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=25,.minChargeDamage=25,.chargedDamage=25,.headshotDamage=25,.minChargeHeadshotDamage=25,.chargedHeadshotDamage=25,
        .unchargedLifespan=2,.minChargeLifespan=2,.chargedLifespan=2,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=28672,.minChargeSpeed=28672,.chargedSpeed=28672,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=32768,.homingTolerance=2896,.unchargedSplashRadius=4096,.minChargeSplashRadius=4096,.chargedSplashRadius=4096,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=1638,.minRicoLossH=1638,.chRicoLossH=1638,.unRicoLossV=1228,.minRicoLossV=1228,.chRicoLossV=1228,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=1,.smokeChargeAmount=1});
    {
        WeaponSpec s=v.at(0); s.drawFuncIds={14,14}; s.colEffects={4,4}; s.dmgDirTypes={3,3};
        s.unchargedDmgDirMag=2867; s.minChargeDmgDirMag=2867; s.chargedDmgDirMag=2867;
        s.unchargedSpeed=2457; s.minChargeSpeed=2457; s.chargedSpeed=2457; v.push_back(std::move(s));
    }
    v.push_back({.description="",.beam=B::Magmaul,.beamKind=B::Enemy,.drawFuncIds={15,15},.colors={9055,21407},.priority=1,
        .flags=F::PartialCharge|F::CanCharge|F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::SurfaceCollision,
        .splashDamage=15,.minChargeSplashDamage=15,.chargedSplashDamage=15,.splashDmgTypes={3,3},.shotCooldown=3,.autofireCooldown=3,.ammoType=0,
        .colEffects={113,113},.muzzleEffects={65,65},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::Burn,A::None},.padding21=0,
        .minCharge=30,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,.minChargeDamage=12,.chargedDamage=36,.headshotDamage=8,.minChargeHeadshotDamage=12,.chargedHeadshotDamage=36,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=2867,.minChargeDmgDirMag=2867,.chargedDmgDirMag=2867,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=2252,.minChargeSpeed=2252,.chargedSpeed=2252,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=20,.chargedHoming=20,.homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=8192,.minChargeSplashRadius=8192,.chargedSplashRadius=8192,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0});
    {
        WeaponSpec s=v.at(0); s.flags=F::PartialCharge|F::CanCharge|F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::SurfaceCollision;
        s.drawFuncIds={16,16}; s.colEffects={134,134}; s.dmgDirTypes={3,3}; s.afflictions={A::Freeze,A::None};
        s.unchargedDmgDirMag=2048; s.minChargeDmgDirMag=2048; s.chargedDmgDirMag=2048;
        s.unchargedSpeed=2252; s.minChargeSpeed=2252; s.chargedSpeed=2252; v.push_back(std::move(s));
    }
    return v;
}

std::vector<WeaponSpec> BuildBossSpecs()
{
    using A = Affliction;
    using B = BeamType;
    using F = WeaponFlags;
    std::vector<WeaponSpec> v;
    v.reserve(8);
    v.push_back({
        .description="Cretaphid Crystal",.beam=B::Battlehammer,.beamKind=B::Missile,.drawFuncIds={18,18},.colors={16367,16367},.priority=2,
        .flags=F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=6,.minChargeSplashDamage=1,
        .chargedSplashDamage=1,.splashDmgTypes={3,3},.shotCooldown=8,.autofireCooldown=8,.ammoType=0,.colEffects={14,14},.muzzleEffects={63,63},
        .dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,
        .chargeCost=0,.unchargedDamage=6,.minChargeDamage=5,.chargedDamage=5,.headshotDamage=6,.minChargeHeadshotDamage=5,.chargedHeadshotDamage=5,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=2048,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,
        .chargedCylRadius=409,.unchargedSpeed=2457,.minChargeSpeed=2457,.chargedSpeed=2457,.unchargedFinalSpeed=819,.minChargeFinalSpeed=819,
        .chargedFinalSpeed=819,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,
        .homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=10240,.minChargeSplashRadius=10240,.chargedSplashRadius=10240,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=20480,.minChargeSpread=20480,.chargedSpread=20480,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=50,
        .smokeDrain=10,.smokeShotAmount=10,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Cretaphid Plasma 1",.beam=B::VoltDriver,.beamKind=B::VoltDriver,.drawFuncIds={19,19},.colors={32767,32767},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::SurfaceCollision,
        .splashDamage=1,.minChargeSplashDamage=1,.chargedSplashDamage=1,.splashDmgTypes={0,0},.shotCooldown=8,.autofireCooldown=8,.ammoType=0,
        .colEffects={142,142},.muzzleEffects={60,60},.dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,
        .fullCharge=15,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,.minChargeDamage=5,.chargedDamage=5,.headshotDamage=5,
        .minChargeHeadshotDamage=5,.chargedHeadshotDamage=5,.unchargedLifespan=160,.minChargeLifespan=160,.chargedLifespan=160,.speedDecay={30,30},
        .padding42=30,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=0,.unchargedCylRadius=409,
        .minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=2048,.minChargeSpeed=2048,.chargedSpeed=2048,.unchargedFinalSpeed=0,
        .minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,
        .chargedHoming=0,.homingRange=204800,.homingTolerance=3896,.unchargedSplashRadius=2800,.minChargeSplashRadius=2800,.chargedSplashRadius=2800,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,
        .minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,
        .smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Cretaphid Plasma 2",.beam=B::VoltDriver,.beamKind=B::VoltDriver,.drawFuncIds={19,19},.colors={32767,32767},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::SurfaceCollision,
        .splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=8,.autofireCooldown=8,.ammoType=0,
        .colEffects={142,142},.muzzleEffects={60,60},.dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,
        .fullCharge=15,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,.minChargeDamage=5,.chargedDamage=5,.headshotDamage=5,
        .minChargeHeadshotDamage=5,.chargedHeadshotDamage=5,.unchargedLifespan=400,.minChargeLifespan=400,.chargedLifespan=400,.speedDecay={0,0},
        .padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=0,.unchargedCylRadius=409,
        .minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=409,.minChargeSpeed=409,.chargedSpeed=409,.unchargedFinalSpeed=0,
        .minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=40,
        .minChargeHoming=40,.chargedHoming=40,.homingRange=204800,.homingTolerance=3896,.unchargedSplashRadius=6144,.minChargeSplashRadius=6144,
        .chargedSplashRadius=6144,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,
        .chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,
        .unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,
        .smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Slench Tear",.beam=B::Missile,.beamKind=B::Missile,.drawFuncIds={12,12},.colors={32140,32140},.priority=2,
        .flags=F::PartialCharge|F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::RadIdx1Uncharged|F::RadIdx2Uncharged|F::RadIdx1Charged|F::RadIdx2Charged|F::SurfaceCollision,
        .splashDamage=5,.minChargeSplashDamage=5,.chargedSplashDamage=5,.splashDmgTypes={0,0},.shotCooldown=1,.autofireCooldown=1,.ammoType=1,
        .colEffects={71,71},.muzzleEffects={65,65},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,
        .fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=10,.minChargeDamage=10,.chargedDamage=10,.headshotDamage=10,
        .minChargeHeadshotDamage=10,.chargedHeadshotDamage=10,.unchargedLifespan=1800,.minChargeLifespan=1800,.chargedLifespan=1800,
        .speedDecay={7,7},.padding42=7,.speedInterp={2,2},.unchargedDmgDirMag=1638,.minChargeDmgDirMag=1638,.chargedDmgDirMag=1638,.zoomFov=40960,
        .unchargedCylRadius=1638,.minChargeCylRadius=1638,.chargedCylRadius=1638,.unchargedSpeed=4,.minChargeSpeed=4,.chargedSpeed=4,
        .unchargedFinalSpeed=512,.minChargeFinalSpeed=512,.chargedFinalSpeed=512,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=409600,.homingTolerance=71,.unchargedSplashRadius=4096,
        .minChargeSplashRadius=4096,.chargedSplashRadius=4096,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,
        .minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,
        .chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=75,.smokeMinimum=25,.smokeDrain=5,.smokeShotAmount=125,.smokeChargeAmount=25
    });
    v.push_back({
        .description="Slench Beam 1",.beam=B::PowerBeam,.beamKind=B::PowerBeam,.drawFuncIds={21,21},.colors={9055,21407},.priority=1,
        .flags=F::PartialCharge|F::CanCharge|F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=0,
        .minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=3,.autofireCooldown=3,.ammoType=0,.colEffects={242,242},
        .muzzleEffects={65,65},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=30,.fullCharge=60,
        .ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=3,.minChargeDamage=3,.chargedDamage=3,.headshotDamage=3,
        .minChargeHeadshotDamage=3,.chargedHeadshotDamage=3,.unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},
        .padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=614,.minChargeDmgDirMag=614,.chargedDmgDirMag=614,.zoomFov=40960,.unchargedCylRadius=409,
        .minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=2662,.minChargeSpeed=2662,.chargedSpeed=2662,.unchargedFinalSpeed=0,
        .minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=20,
        .chargedHoming=20,.homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,
        .minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,
        .smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Slench Beam 2",.beam=B::Magmaul,.beamKind=B::Magmaul,.drawFuncIds={4,4},.colors={15711,15711},.priority=2,
        .flags=F::CanCharge|F::SelfDamageUncharged|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=0,
        .minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={2,2},.shotCooldown=16,.autofireCooldown=16,.ammoType=0,.colEffects={9,9},
        .muzzleEffects={64,64},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::Burn,A::None},.padding21=0,.minCharge=15,.fullCharge=45,
        .ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=2,.minChargeDamage=2,.chargedDamage=2,.headshotDamage=2,
        .minChargeHeadshotDamage=2,.chargedHeadshotDamage=2,.unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={15,15},
        .padding42=15,.speedInterp={2,2},.unchargedDmgDirMag=1228,.minChargeDmgDirMag=1228,.chargedDmgDirMag=1228,.zoomFov=40960,
        .unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=1638,.minChargeSpeed=4915,.chargedSpeed=4915,
        .unchargedFinalSpeed=2867,.minChargeFinalSpeed=2867,.chargedFinalSpeed=2867,.unchargedGravity=0,.minChargeGravity=-204,.chargedGravity=-204,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=40960,.homingTolerance=3896,.unchargedSplashRadius=12288,
        .minChargeSplashRadius=12288,.chargedSplashRadius=12288,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,
        .minChargeSpread=0,.chargedSpread=0,.unRicoLossH=1638,.minRicoLossH=1638,.chRicoLossH=1638,.unRicoLossV=1228,.minRicoLossV=1228,
        .chRicoLossV=1228,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,.smokeDrain=2,.smokeShotAmount=200,.smokeChargeAmount=500
    });
    v.push_back({
        .description="Slench Beam 3",.beam=B::VoltDriver,.beamKind=B::VoltDriver,.drawFuncIds={2,2},.colors={32767,32767},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::SurfaceCollision,
        .splashDamage=1,.minChargeSplashDamage=1,.chargedSplashDamage=1,.splashDmgTypes={0,0},.shotCooldown=10,.autofireCooldown=10,.ammoType=0,
        .colEffects={89,88},.muzzleEffects={60,60},.dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::Disrupt,A::Disrupt},.padding21=0,
        .minCharge=15,.fullCharge=45,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=4,.minChargeDamage=4,.chargedDamage=4,
        .headshotDamage=4,.minChargeHeadshotDamage=4,.chargedHeadshotDamage=4,.unchargedLifespan=90,.minChargeLifespan=90,.chargedLifespan=90,
        .speedDecay={7,7},.padding42=7,.speedInterp={0,0},.unchargedDmgDirMag=1228,.minChargeDmgDirMag=1228,.chargedDmgDirMag=1228,.zoomFov=0,
        .unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=3276,.minChargeSpeed=3276,.chargedSpeed=3276,
        .unchargedFinalSpeed=1638,.minChargeFinalSpeed=1638,.chargedFinalSpeed=1638,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=204800,.homingTolerance=3896,.unchargedSplashRadius=4096,
        .minChargeSplashRadius=4096,.chargedSplashRadius=4096,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,
        .minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,
        .chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Slench Beam 4",.beam=B::Judicator,.beamKind=B::Judicator,.drawFuncIds={3,6},.colors={32404,32404},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::AoeCharged|F::SurfaceCollision,.splashDamage=27,
        .minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=12,.autofireCooldown=12,.ammoType=0,.colEffects={10,0},
        .muzzleEffects={62,62},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::Freeze,A::Freeze},.padding21=0,.minCharge=15,.fullCharge=60,
        .ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,.minChargeDamage=5,.chargedDamage=5,.headshotDamage=5,
        .minChargeHeadshotDamage=5,.chargedHeadshotDamage=5,.unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},
        .padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=614,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,
        .minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=1433,.minChargeSpeed=4096,.chargedSpeed=4096,.unchargedFinalSpeed=0,
        .minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,
        .chargedHoming=0,.homingRange=102400,.homingTolerance=3896,.unchargedSplashRadius=2048,.minChargeSplashRadius=4096,.chargedSplashRadius=4096,
        .unchargedDistance=0,.minChargeDistance=14336,.chargedDistance=14336,.unchargedSpread=0,.minChargeSpread=245760,.chargedSpread=245760,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=3,.minChargedProjectileCount=3,.chargeProjectileCount=3,.smokeStart=50,.smokeMinimum=0,
        .smokeDrain=5,.smokeShotAmount=25,.smokeChargeAmount=50
    });
    return v;
}
std::vector<WeaponSpec> BuildGoreaSpecs()
{
    using A = Affliction;
    using B = BeamType;
    using F = WeaponFlags;
    std::vector<WeaponSpec> v;
    v.reserve(6);
    v.push_back({
        .description="Gorea Battlehammer",.beam=B::Battlehammer,.beamKind=B::Missile,.drawFuncIds={0,0},.colors={16367,16367},.priority=2,
        .flags=F::CanCharge|F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=2,.minChargeSplashDamage=2,
        .chargedSplashDamage=2,.splashDmgTypes={3,3},.shotCooldown=6,.autofireCooldown=22,.ammoType=0,.colEffects={14,14},.muzzleEffects={255,255},
        .dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,
        .chargeCost=0,.unchargedDamage=3,.minChargeDamage=3,.chargedDamage=3,.headshotDamage=3,.minChargeHeadshotDamage=3,.chargedHeadshotDamage=3,
        .unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=2048,.minChargeDmgDirMag=2048,.chargedDmgDirMag=2048,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,
        .chargedCylRadius=409,.unchargedSpeed=2457,.minChargeSpeed=2457,.chargedSpeed=3686,.unchargedFinalSpeed=2048,.minChargeFinalSpeed=2048,
        .chargedFinalSpeed=2048,.unchargedGravity=-61,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,
        .homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=6144,.minChargeSplashRadius=6144,.chargedSplashRadius=6144,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=20480,.minChargeSpread=20480,.chargedSpread=20480,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=50,
        .smokeDrain=10,.smokeShotAmount=10,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Gorea Volt Driver",.beam=B::VoltDriver,.beamKind=B::VoltDriver,.drawFuncIds={1,2},.colors={32767,32767},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::DestroyableUncharged|F::DestroyableCharged|F::RadIdx1Uncharged|F::RadIdx1Charged|F::RadIdx2Charged|F::SurfaceCollision,
        .splashDamage=1,.minChargeSplashDamage=7,.chargedSplashDamage=7,.splashDmgTypes={0,0},.shotCooldown=23,.autofireCooldown=28,.ammoType=0,
        .colEffects={89,88},.muzzleEffects={255,255},.dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::None,A::Disrupt},.padding21=0,
        .minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=2,.minChargeDamage=10,.chargedDamage=10,
        .headshotDamage=2,.minChargeHeadshotDamage=10,.chargedHeadshotDamage=10,.unchargedLifespan=250,.minChargeLifespan=150,.chargedLifespan=150,
        .speedDecay={0,15},.padding42=15,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=0,
        .unchargedCylRadius=1638,.minChargeCylRadius=1638,.chargedCylRadius=1638,.unchargedSpeed=768,.minChargeSpeed=327,.chargedSpeed=327,
        .unchargedFinalSpeed=0,.minChargeFinalSpeed=682,.chargedFinalSpeed=682,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=409,.minChargeHoming=409,.chargedHoming=409,.homingRange=204800,.homingTolerance=3896,.unchargedSplashRadius=2048,
        .minChargeSplashRadius=6144,.chargedSplashRadius=6144,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,
        .minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,
        .chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Gorea Magmaul",.beam=B::Magmaul,.beamKind=B::Missile,.drawFuncIds={4,4},.colors={15711,15711},.priority=2,
        .flags=F::CanCharge|F::RicochetUncharged|F::SelfDamageUncharged|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,
        .splashDamage=12,.minChargeSplashDamage=12,.chargedSplashDamage=12,.splashDmgTypes={0,0},.shotCooldown=20,.autofireCooldown=20,.ammoType=0,
        .colEffects={9,9},.muzzleEffects={255,255},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::Burn},.padding21=0,.minCharge=15,
        .fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=10,.minChargeDamage=15,.chargedDamage=15,.headshotDamage=10,
        .minChargeHeadshotDamage=15,.chargedHeadshotDamage=15,.unchargedLifespan=120,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={15,15},
        .padding42=15,.speedInterp={0,0},.unchargedDmgDirMag=1228,.minChargeDmgDirMag=819,.chargedDmgDirMag=819,.zoomFov=40960,
        .unchargedCylRadius=1638,.minChargeCylRadius=1638,.chargedCylRadius=1638,.unchargedSpeed=2867,.minChargeSpeed=2867,.chargedSpeed=2048,
        .unchargedFinalSpeed=2048,.minChargeFinalSpeed=2048,.chargedFinalSpeed=2048,.unchargedGravity=-20,.minChargeGravity=-20,.chargedGravity=-20,
        .unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=40960,.homingTolerance=3896,.unchargedSplashRadius=20480,
        .minChargeSplashRadius=16384,.chargedSplashRadius=16384,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,
        .minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3072,.minRicoLossH=3072,.chRicoLossH=3072,.unRicoLossV=3072,.minRicoLossV=3072,
        .chRicoLossV=3072,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=4,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,.smokeDrain=1,.smokeShotAmount=200,.smokeChargeAmount=500
    });
    v.push_back({
        .description="Gorea Judicator",.beam=B::Judicator,.beamKind=B::Missile,.drawFuncIds={3,6},.colors={32404,32404},.priority=2,
        .flags=F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::AoeCharged|F::SurfaceCollision,.splashDamage=3,.minChargeSplashDamage=0,
        .chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=40,.autofireCooldown=36,.ammoType=0,.colEffects={10,0},.muzzleEffects={255,255},
        .dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::Freeze},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,
        .chargeCost=0,.unchargedDamage=5,.minChargeDamage=30,.chargedDamage=30,.headshotDamage=5,.minChargeHeadshotDamage=30,
        .chargedHeadshotDamage=30,.unchargedLifespan=50,.minChargeLifespan=15,.chargedLifespan=15,.speedDecay={15,0},.padding42=0,.speedInterp={0,0},
        .unchargedDmgDirMag=409,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=1638,.minChargeCylRadius=1638,
        .chargedCylRadius=1638,.unchargedSpeed=2048,.minChargeSpeed=4096,.chargedSpeed=4096,.unchargedFinalSpeed=3686,.minChargeFinalSpeed=0,
        .chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,
        .homingRange=102400,.homingTolerance=3896,.unchargedSplashRadius=4096,.minChargeSplashRadius=4096,.chargedSplashRadius=4096,
        .unchargedDistance=0,.minChargeDistance=16384,.chargedDistance=16384,.unchargedSpread=20480,.minChargeSpread=245760,.chargedSpread=245760,
        .unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=5,
        .chargedRicoWeaponIdx=-1,.projectileCount=3,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,
        .smokeDrain=5,.smokeShotAmount=25,.smokeChargeAmount=50
    });
    v.push_back({
        .description="Gorea Imperialist",.beam=B::Imperialist,.beamKind=B::Imperialist,.drawFuncIds={8,8},.colors={32767,32767},.priority=2,
        .flags=F::RepeatFire|F::CanZoom|F::SurfaceCollision,.splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},
        .shotCooldown=23,.autofireCooldown=23,.ammoType=0,.colEffects={31,31},.muzzleEffects={255,255},.dmgDirTypes={0,0},.dmgInterp={0,0},
        .afflictions={A::None,A::None},.padding21=0,.minCharge=10,.fullCharge=90,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=50,
        .minChargeDamage=50,.chargedDamage=50,.headshotDamage=50,.minChargeHeadshotDamage=50,.chargedHeadshotDamage=50,.unchargedLifespan=255,
        .minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,
        .chargedDmgDirMag=0,.zoomFov=57344,.unchargedCylRadius=819,.minChargeCylRadius=819,.chargedCylRadius=819,.unchargedSpeed=10240,
        .minChargeSpeed=10240,.chargedSpeed=10240,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,
        .minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=204800,.homingTolerance=3896,
        .unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,
        .unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,
        .minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=200,.smokeMinimum=0,.smokeDrain=10,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Gorea Shock Coil",.beam=B::ShockCoil,.beamKind=B::ShockCoil,.drawFuncIds={9,9},.colors={32767,32767},.priority=2,
        .flags=F::CanCharge|F::RepeatFire|F::SelfDamageUncharged|F::ForceEffectUncharged|F::ForceEffectCharged|F::Continuous|F::SurfaceCollision,
        .splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=8,.autofireCooldown=18,.ammoType=0,
        .colEffects={255,255},.muzzleEffects={255,255},.dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,
        .fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=10,.minChargeDamage=10,.chargedDamage=10,.headshotDamage=10,
        .minChargeHeadshotDamage=10,.chargedHeadshotDamage=10,.unchargedLifespan=2,.minChargeLifespan=2,.chargedLifespan=2,.speedDecay={0,0},
        .padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,
        .minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=28672,.minChargeSpeed=28672,.chargedSpeed=28672,.unchargedFinalSpeed=0,
        .minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=409,
        .minChargeHoming=409,.chargedHoming=409,.homingRange=49152,.homingTolerance=-4034,.unchargedSplashRadius=4096,.minChargeSplashRadius=4096,
        .chargedSplashRadius=4096,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,
        .chargedSpread=0,.unRicoLossH=12,.minRicoLossH=12,.chRicoLossH=12,.unRicoLossV=9,.minRicoLossV=9,.chRicoLossV=9,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,
        .smokeDrain=1,.smokeShotAmount=1,.smokeChargeAmount=1
    });
    return v;
}
std::vector<WeaponSpec> BuildPlatformSpecs()
{
    using A = Affliction;
    using B = BeamType;
    using F = WeaponFlags;
    std::vector<WeaponSpec> v;
    v.reserve(4);
    v.push_back({
        .description="Sylux Ship Missile",.beam=B::Missile,.beamKind=B::Platform,.drawFuncIds={20,20},.colors={32140,32140},.priority=2,
        .flags=F::PartialCharge|F::CanCharge|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=2,
        .minChargeSplashDamage=2,.chargedSplashDamage=2,.splashDmgTypes={2,2},.shotCooldown=20,.autofireCooldown=20,.ammoType=1,
        .colEffects={187,187},.muzzleEffects={188,188},.dmgDirTypes={2,2},.dmgInterp={2,2},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,
        .fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=3,.minChargeDamage=3,.chargedDamage=3,.headshotDamage=3,
        .minChargeHeadshotDamage=3,.chargedHeadshotDamage=3,.unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={7,7},
        .padding42=7,.speedInterp={2,2},.unchargedDmgDirMag=1638,.minChargeDmgDirMag=1638,.chargedDmgDirMag=1638,.zoomFov=40960,
        .unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=819,.minChargeSpeed=819,.chargedSpeed=819,
        .unchargedFinalSpeed=10240,.minChargeFinalSpeed=10240,.chargedFinalSpeed=10240,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,
        .unchargedHoming=0,.minChargeHoming=40,.chargedHoming=204,.homingRange=409600,.homingTolerance=3896,.unchargedSplashRadius=6144,
        .minChargeSplashRadius=6144,.chargedSplashRadius=6144,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,
        .minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,
        .chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=75,.smokeMinimum=25,.smokeDrain=5,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Platform Unused",.beam=B::PowerBeam,.beamKind=B::Platform,.drawFuncIds={0,0},.colors={9055,9055},.priority=1,
        .flags=F::PartialCharge|F::RepeatFire|F::ForceEffectUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=0,
        .minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=3,.autofireCooldown=3,.ammoType=0,.colEffects={255,255},
        .muzzleEffects={65,65},.dmgDirTypes={0,0},.dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=60,
        .ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,.minChargeDamage=5,.chargedDamage=5,.headshotDamage=8,
        .minChargeHeadshotDamage=8,.chargedHeadshotDamage=8,.unchargedLifespan=255,.minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},
        .padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,
        .minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=8192,.minChargeSpeed=8192,.chargedSpeed=8192,.unchargedFinalSpeed=0,
        .minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,
        .chargedHoming=0,.homingRange=102400,.homingTolerance=3896,.unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,
        .minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,
        .smokeDrain=1,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="Platform Energy Beam",.beam=B::ShockCoil,.beamKind=B::Platform,.drawFuncIds={17,17},.colors={32767,32767},.priority=2,
        .flags=F::RepeatFire|F::SelfDamageUncharged|F::ForceEffectUncharged|F::ForceEffectCharged|F::Continuous|F::SurfaceCollision,.splashDamage=0,
        .minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=0,.autofireCooldown=0,.ammoType=0,.colEffects={255,255},
        .muzzleEffects={61,61},.dmgDirTypes={3,3},.dmgInterp={3,3},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=45,
        .ammoCost=15,.minChargeCost=15,.chargeCost=15,.unchargedDamage=60,.minChargeDamage=60,.chargedDamage=60,.headshotDamage=60,
        .minChargeHeadshotDamage=60,.chargedHeadshotDamage=60,.unchargedLifespan=2,.minChargeLifespan=2,.chargedLifespan=2,.speedDecay={0,0},
        .padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,.chargedDmgDirMag=0,.zoomFov=40960,.unchargedCylRadius=409,
        .minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=233472,.minChargeSpeed=233472,.chargedSpeed=233472,.unchargedFinalSpeed=0,
        .minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,
        .chargedHoming=0,.homingRange=32768,.homingTolerance=2896,.unchargedSplashRadius=4096,.minChargeSplashRadius=4096,.chargedSplashRadius=4096,
        .unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=1638,
        .minRicoLossH=1638,.chRicoLossH=1638,.unRicoLossV=1228,.minRicoLossV=1228,.chRicoLossV=1228,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=50,.smokeMinimum=0,
        .smokeDrain=1,.smokeShotAmount=1,.smokeChargeAmount=1
    });
    v.push_back({
        .description="Platform Arc Welder",.beam=B::ShockCoil,.beamKind=B::Platform,.drawFuncIds={9,9},.colors={32767,32767},.priority=2,
        .flags=F::RepeatFire|F::Continuous,.splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=0,
        .autofireCooldown=0,.ammoType=0,.colEffects={255,255},.muzzleEffects={62,62},.dmgDirTypes={3,3},.dmgInterp={0,0},
        .afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=45,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=20,
        .minChargeDamage=20,.chargedDamage=20,.headshotDamage=20,.minChargeHeadshotDamage=20,.chargedHeadshotDamage=20,.unchargedLifespan=2,
        .minChargeLifespan=2,.chargedLifespan=2,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=0,.minChargeDmgDirMag=0,
        .chargedDmgDirMag=0,.zoomFov=61440,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,.unchargedSpeed=8192,
        .minChargeSpeed=8192,.chargedSpeed=8192,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,.unchargedGravity=0,
        .minChargeGravity=0,.chargedGravity=0,.unchargedHoming=409,.minChargeHoming=409,.chargedHoming=409,.homingRange=24576,.homingTolerance=2048,
        .unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,.unchargedDistance=0,.minChargeDistance=0,.chargedDistance=0,
        .unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=1638,.minRicoLossH=1638,.chRicoLossH=1638,.unRicoLossV=1228,
        .minRicoLossV=1228,.chRicoLossV=1228,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,
        .chargeProjectileCount=1,.smokeStart=1000,.smokeMinimum=0,.smokeDrain=0,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    return v;
}
std::vector<WeaponSpec> BuildRicochetSpecs()
{
    using A = Affliction;
    using B = BeamType;
    using F = WeaponFlags;
    std::vector<WeaponSpec> v;
    v.reserve(6);
    v.push_back({
        .description="Judicator Player Ricochet",.beam=B::Judicator,.beamKind=B::Judicator,.drawFuncIds={3,3},.colors={32404,32404},.priority=2,
        .flags=F::CanCharge|F::RicochetUncharged|F::SurfaceCollision,.splashDamage=0,.minChargeSplashDamage=0,.chargedSplashDamage=0,
        .splashDmgTypes={0,0},.shotCooldown=20,.autofireCooldown=20,.ammoType=0,.colEffects={11,11},.muzzleEffects={255,255},.dmgDirTypes={3,3},
        .dmgInterp={0,0},.afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,
        .unchargedDamage=24,.minChargeDamage=24,.chargedDamage=24,.headshotDamage=24,.minChargeHeadshotDamage=24,.chargedHeadshotDamage=24,
        .unchargedLifespan=15,.minChargeLifespan=15,.chargedLifespan=15,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=819,
        .minChargeDmgDirMag=819,.chargedDmgDirMag=819,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=8192,.minChargeSpeed=8192,.chargedSpeed=8192,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=102400,
        .homingTolerance=3896,.unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,.unchargedDistance=0,.minChargeDistance=0,
        .chargedDistance=0,.unchargedSpread=0,.minChargeSpread=0,.chargedSpread=0,.unRicoLossH=3686,.minRicoLossH=3686,.chRicoLossH=3686,
        .unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,.projectileCount=1,
        .minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=50,.smokeDrain=2,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    v.push_back({
        .description="",.beam=B::Magmaul,.beamKind=B::Enemy,.drawFuncIds={4,4},.colors={575,575},.priority=2,
        .flags=F::PartialCharge|F::SurfaceCollision,.splashDamage=1,.minChargeSplashDamage=1,.chargedSplashDamage=1,.splashDmgTypes={2,2},
        .shotCooldown=20,.autofireCooldown=20,.ammoType=0,.colEffects={9,9},.muzzleEffects={255,255},.dmgDirTypes={3,3},.dmgInterp={0,0},
        .afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,
        .minChargeDamage=5,.chargedDamage=5,.headshotDamage=5,.minChargeHeadshotDamage=5,.chargedHeadshotDamage=5,.unchargedLifespan=255,
        .minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={2,2},.unchargedDmgDirMag=1638,
        .minChargeDmgDirMag=1638,.chargedDmgDirMag=1638,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=2867,.minChargeSpeed=2867,.chargedSpeed=2867,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=-204,.minChargeGravity=-204,.chargedGravity=-204,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=40960,
        .homingTolerance=3896,.unchargedSplashRadius=12288,.minChargeSplashRadius=12288,.chargedSplashRadius=12288,.unchargedDistance=0,
        .minChargeDistance=0,.chargedDistance=0,.unchargedSpread=20480,.minChargeSpread=20480,.chargedSpread=20480,.unRicoLossH=2048,
        .minRicoLossH=2048,.chRicoLossH=2048,.unRicoLossV=2048,.minRicoLossV=2048,.chRicoLossV=2048,.unchargedRicoWeaponIdx=3,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,
        .smokeDrain=10,.smokeShotAmount=100,.smokeChargeAmount=10
    });
    v.push_back({
        .description="",.beam=B::Judicator,.beamKind=B::Enemy,.drawFuncIds={3,3},.colors={32404,32404},.priority=2,
        .flags=F::CanCharge|F::RicochetUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=0,.minChargeSplashDamage=0,
        .chargedSplashDamage=0,.splashDmgTypes={0,0},.shotCooldown=20,.autofireCooldown=20,.ammoType=0,.colEffects={11,11},.muzzleEffects={255,255},
        .dmgDirTypes={3,3},.dmgInterp={0,0},.afflictions={A::None,A::Freeze},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,
        .chargeCost=0,.unchargedDamage=4,.minChargeDamage=4,.chargedDamage=4,.headshotDamage=4,.minChargeHeadshotDamage=4,.chargedHeadshotDamage=4,
        .unchargedLifespan=15,.minChargeLifespan=15,.chargedLifespan=15,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=409,
        .minChargeDmgDirMag=2048,.chargedDmgDirMag=2048,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=12288,.minChargeSpeed=0,.chargedSpeed=0,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=102400,
        .homingTolerance=3896,.unchargedSplashRadius=0,.minChargeSplashRadius=0,.chargedSplashRadius=0,.unchargedDistance=0,.minChargeDistance=8192,
        .chargedDistance=8192,.unchargedSpread=81920,.minChargeSpread=81920,.chargedSpread=81920,.unRicoLossH=3686,.minRicoLossH=3686,
        .chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=3,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=50,.smokeDrain=2,.smokeShotAmount=0,
        .smokeChargeAmount=0
    });
    v.push_back({
        .description="",.beam=B::Magmaul,.beamKind=B::Enemy,.drawFuncIds={4,4},.colors={575,575},.priority=2,
        .flags=F::PartialCharge|F::SurfaceCollision,.splashDamage=1,.minChargeSplashDamage=1,.chargedSplashDamage=1,.splashDmgTypes={2,2},
        .shotCooldown=20,.autofireCooldown=20,.ammoType=0,.colEffects={9,9},.muzzleEffects={255,255},.dmgDirTypes={3,3},.dmgInterp={0,0},
        .afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=5,
        .minChargeDamage=5,.chargedDamage=5,.headshotDamage=5,.minChargeHeadshotDamage=5,.chargedHeadshotDamage=5,.unchargedLifespan=255,
        .minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={2,2},.unchargedDmgDirMag=1638,
        .minChargeDmgDirMag=1638,.chargedDmgDirMag=1638,.zoomFov=40960,.unchargedCylRadius=409,.minChargeCylRadius=409,.chargedCylRadius=409,
        .unchargedSpeed=2867,.minChargeSpeed=2867,.chargedSpeed=2867,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=-204,.minChargeGravity=-204,.chargedGravity=-204,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=40960,
        .homingTolerance=3896,.unchargedSplashRadius=12288,.minChargeSplashRadius=12288,.chargedSplashRadius=12288,.unchargedDistance=0,
        .minChargeDistance=0,.chargedDistance=0,.unchargedSpread=20480,.minChargeSpread=20480,.chargedSpread=20480,.unRicoLossH=2048,
        .minRicoLossH=2048,.chRicoLossH=2048,.unRicoLossV=2048,.minRicoLossV=2048,.chRicoLossV=2048,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=1,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,
        .smokeDrain=10,.smokeShotAmount=100,.smokeChargeAmount=10
    });
    v.push_back({
        .description="Gorea Magmaul Ricochet",.beam=B::Magmaul,.beamKind=B::Missile,.drawFuncIds={4,4},.colors={575,575},.priority=2,
        .flags=F::PartialCharge|F::SurfaceCollision,.splashDamage=12,.minChargeSplashDamage=12,.chargedSplashDamage=12,.splashDmgTypes={0,0},
        .shotCooldown=0,.autofireCooldown=0,.ammoType=0,.colEffects={9,9},.muzzleEffects={255,255},.dmgDirTypes={3,3},.dmgInterp={0,0},
        .afflictions={A::None,A::None},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,.unchargedDamage=10,
        .minChargeDamage=15,.chargedDamage=15,.headshotDamage=10,.minChargeHeadshotDamage=15,.chargedHeadshotDamage=15,.unchargedLifespan=255,
        .minChargeLifespan=255,.chargedLifespan=255,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=819,
        .minChargeDmgDirMag=819,.chargedDmgDirMag=819,.zoomFov=40960,.unchargedCylRadius=1638,.minChargeCylRadius=1638,.chargedCylRadius=1638,
        .unchargedSpeed=2867,.minChargeSpeed=2867,.chargedSpeed=2867,.unchargedFinalSpeed=2048,.minChargeFinalSpeed=2048,.chargedFinalSpeed=2048,
        .unchargedGravity=-40,.minChargeGravity=-40,.chargedGravity=-40,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=40960,
        .homingTolerance=3896,.unchargedSplashRadius=16384,.minChargeSplashRadius=16384,.chargedSplashRadius=16384,.unchargedDistance=0,
        .minChargeDistance=0,.chargedDistance=0,.unchargedSpread=143360,.minChargeSpread=143360,.chargedSpread=143360,.unRicoLossH=2048,
        .minRicoLossH=2048,.chRicoLossH=2048,.unRicoLossV=512,.minRicoLossV=512,.chRicoLossV=512,.unchargedRicoWeaponIdx=-1,.chargedRicoWeaponIdx=-1,
        .projectileCount=4,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=0,.smokeDrain=10,.smokeShotAmount=100,
        .smokeChargeAmount=10
    });
    v.push_back({
        .description="Gorea Judicator Ricochet",.beam=B::Judicator,.beamKind=B::Missile,.drawFuncIds={3,3},.colors={32404,32404},.priority=2,
        .flags=F::RicochetUncharged|F::ForceEffectCharged|F::SurfaceCollision,.splashDamage=3,.minChargeSplashDamage=20,.chargedSplashDamage=20,
        .splashDmgTypes={0,0},.shotCooldown=0,.autofireCooldown=0,.ammoType=0,.colEffects={11,11},.muzzleEffects={255,255},.dmgDirTypes={3,3},
        .dmgInterp={0,0},.afflictions={A::Freeze,A::Freeze},.padding21=0,.minCharge=15,.fullCharge=60,.ammoCost=0,.minChargeCost=0,.chargeCost=0,
        .unchargedDamage=3,.minChargeDamage=20,.chargedDamage=20,.headshotDamage=3,.minChargeHeadshotDamage=20,.chargedHeadshotDamage=20,
        .unchargedLifespan=30,.minChargeLifespan=12,.chargedLifespan=12,.speedDecay={0,0},.padding42=0,.speedInterp={0,0},.unchargedDmgDirMag=409,
        .minChargeDmgDirMag=2048,.chargedDmgDirMag=2048,.zoomFov=40960,.unchargedCylRadius=819,.minChargeCylRadius=819,.chargedCylRadius=819,
        .unchargedSpeed=1228,.minChargeSpeed=0,.chargedSpeed=0,.unchargedFinalSpeed=0,.minChargeFinalSpeed=0,.chargedFinalSpeed=0,
        .unchargedGravity=0,.minChargeGravity=0,.chargedGravity=0,.unchargedHoming=0,.minChargeHoming=0,.chargedHoming=0,.homingRange=102400,
        .homingTolerance=3896,.unchargedSplashRadius=4096,.minChargeSplashRadius=4096,.chargedSplashRadius=4096,.unchargedDistance=0,
        .minChargeDistance=8192,.chargedDistance=8192,.unchargedSpread=81920,.minChargeSpread=81920,.chargedSpread=81920,.unRicoLossH=3686,
        .minRicoLossH=3686,.chRicoLossH=3686,.unRicoLossV=3686,.minRicoLossV=3686,.chRicoLossV=3686,.unchargedRicoWeaponIdx=-1,
        .chargedRicoWeaponIdx=-1,.projectileCount=3,.minChargedProjectileCount=1,.chargeProjectileCount=1,.smokeStart=100,.smokeMinimum=50,
        .smokeDrain=2,.smokeShotAmount=0,.smokeChargeAmount=0
    });
    return v;
}
}

const std::shared_ptr<const WeaponList> Weapons1P = MakeWeaponList(BuildWeapons1PSpecs());
const std::shared_ptr<const WeaponList> WeaponsMP = MakeWeaponList(BuildWeaponsMPSpecs());
const std::shared_ptr<const WeaponList> EnemyWeapons = MakeWeaponList(BuildEnemySpecs());
const std::shared_ptr<const WeaponList> BossWeapons = MakeWeaponList(BuildBossSpecs());
const std::shared_ptr<const WeaponList> GoreaWeapons = MakeWeaponList(BuildGoreaSpecs());
const std::shared_ptr<const WeaponList> PlatformWeapons = MakeWeaponList(BuildPlatformSpecs());
const std::shared_ptr<const WeaponList> Ricochets = MakeWeaponList(BuildRicochetSpecs());

const std::shared_ptr<const BotWeaponTable> BotWeapons = []
{
    auto table = std::make_shared<BotWeaponTable>();
    table->reserve(5);
    {
        auto values = std::make_shared<BotWeaponList>();
        values->reserve(8);
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{1,5,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{4,10,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{7,13,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{7,0,7,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{20,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{5,10,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{4,10,4,10}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{2,0,0,0}));
        table->push_back(std::move(values));
    }
    {
        auto values = std::make_shared<BotWeaponList>();
        values->reserve(8);
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{3,6,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{4,0,4,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{10,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{7,10,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{4,6,4,6}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{4,0,0,0}));
        table->push_back(std::move(values));
    }
    {
        auto values = std::make_shared<BotWeaponList>();
        values->reserve(8);
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{3,6,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{5,0,5,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{15,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{7,10,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{4,6,4,6}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{4,0,0,0}));
        table->push_back(std::move(values));
    }
    {
        auto values = std::make_shared<BotWeaponList>();
        values->reserve(8);
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{6,12,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{8,0,8,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{20,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{10,15,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{8,15,8,15}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{7,0,0,0}));
        table->push_back(std::move(values));
    }
    {
        auto values = std::make_shared<BotWeaponList>();
        values->reserve(8);
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{8,18,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{0,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{10,0,10,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{25,0,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{12,18,0,0}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{12,18,12,18}));
        values->push_back(std::make_shared<BotWeaponValues>(BotWeaponValues{10,0,0,0}));
        table->push_back(std::move(values));
    }
    return std::shared_ptr<const BotWeaponTable>(std::move(table));
}();

}
}

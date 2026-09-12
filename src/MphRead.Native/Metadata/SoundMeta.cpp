#include "SoundMeta.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace MphRead
{
    namespace
    {
        std::shared_ptr<std::vector<std::vector<std::int32_t>>> platformSfx;
        std::shared_ptr<std::vector<std::vector<std::int32_t>>> hunterSfx;
        std::shared_ptr<std::vector<std::vector<std::int32_t>>> beamSfx;
        std::shared_ptr<std::vector<std::vector<std::int32_t>>> terrainSfx;
        std::shared_ptr<std::vector<std::int32_t>> enemyDamageSfx;
        std::shared_ptr<std::vector<std::int32_t>> enemyDeathSfx;

        std::shared_ptr<std::vector<std::vector<std::int32_t>>> ParseSfxData2(
            const std::vector<std::uint8_t>& data, std::int32_t rows, std::int32_t columns)
        {
            assert(!data.empty() && data.size() % 2 == 0);
            assert(data.size() / 2 == static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns));
            auto dest = std::make_shared<std::vector<std::vector<std::int32_t>>>(
                static_cast<std::size_t>(rows),
                std::vector<std::int32_t>(static_cast<std::size_t>(columns)));
            for (std::int32_t r = 0; r < rows; r++)
            {
                for (std::int32_t c = 0; c < columns; c++)
                {
                    std::int32_t start = r * columns * 2 + c * 2;
                    std::array<std::uint8_t, 2> bytes =
                    {
                        data.at(static_cast<std::size_t>(start)),
                        data.at(static_cast<std::size_t>(start + 1))
                    };
                    std::uint16_t value = 0;
                    std::memcpy(&value, bytes.data(), bytes.size());
                    (*dest)[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]
                        = value == 0xFFFFU ? -1 : static_cast<std::int32_t>(value);
                }
            }
            return dest;
        }

        std::shared_ptr<std::vector<std::int32_t>> ParseSfxData4(
            const std::vector<std::uint8_t>& data, std::int32_t count)
        {
            assert(!data.empty() && data.size() % 4 == 0);
            assert(data.size() / 4 == static_cast<std::size_t>(count));
            auto dest = std::make_shared<std::vector<std::int32_t>>(static_cast<std::size_t>(count));
            for (std::int32_t i = 0; i < count; i++)
            {
                std::int32_t start = i * 4;
                std::array<std::uint8_t, 4> bytes =
                {
                    data.at(static_cast<std::size_t>(start)),
                    data.at(static_cast<std::size_t>(start + 1)),
                    data.at(static_cast<std::size_t>(start + 2)),
                    data.at(static_cast<std::size_t>(start + 3))
                };
                std::uint32_t value = 0;
                std::memcpy(&value, bytes.data(), bytes.size());
                (*dest)[static_cast<std::size_t>(i)] = value == 0xFFFFFFFFU
                    ? -1
                    : std::bit_cast<std::int32_t>(value);
            }
            return dest;
        }
    }

    namespace Metadata
    {
        std::shared_ptr<std::vector<std::vector<std::int32_t>>> PlatformSfx()
        {
            return platformSfx;
        }

        std::shared_ptr<std::vector<std::vector<std::int32_t>>> HunterSfx()
        {
            return hunterSfx;
        }

        std::shared_ptr<std::vector<std::vector<std::int32_t>>> BeamSfx()
        {
            return beamSfx;
        }

        std::shared_ptr<std::vector<std::vector<std::int32_t>>> TerrainSfx()
        {
            return terrainSfx;
        }

        std::shared_ptr<std::vector<std::int32_t>> EnemyDamageSfx()
        {
            return enemyDamageSfx;
        }

        std::shared_ptr<std::vector<std::int32_t>> EnemyDeathSfx()
        {
            return enemyDeathSfx;
        }

        void SetHunterSfxData(const std::vector<std::uint8_t>& data)
        {
            hunterSfx = ParseSfxData2(data, 8, 17);
        }

        void SetBeamSfxData(const std::vector<std::uint8_t>& data)
        {
            beamSfx = ParseSfxData2(data, 9, 10);
        }

        void SetTerrainSfxData(const std::vector<std::uint8_t>& data)
        {
            terrainSfx = ParseSfxData2(data, 12, 6);
        }

        void SetPlatformSfxData(const std::vector<std::uint8_t>& data)
        {
            platformSfx = ParseSfxData2(data, 45, 4);
        }

        void SetEnemyDamageSfxData(const std::vector<std::uint8_t>& data)
        {
            enemyDamageSfx = ParseSfxData4(data, 52);
        }

        void SetEnemyDeathSfxData(const std::vector<std::uint8_t>& data)
        {
            enemyDeathSfx = ParseSfxData4(data, 52);
        }

        const std::vector<std::string> SequenceFiles =
        {
            "0000 - SEQ_BRINSTAR.minincsf",
            "0001 - SEQ_MP1.minincsf",
            "0002 - SEQ_MP2.minincsf",
            "0003 - SEQ_PARASITE.minincsf",
            "0004 - SEQ_SHIP.minincsf",
            "0005 - SEQ_YELLOW.minincsf",
            "0006 - SEQ_RESULTS.minincsf",
            "0007 - SEQ_TIMEOUT.minincsf",
            "0008 - SEQ_WIN.minincsf",
            "0009 - SEQ_GARLIC.minincsf",
            "000A - SEQ_MP2_X.minincsf",
            "000B - SEQ_PARASITE_X.minincsf",
            "000C - SEQ_RED.minincsf",
            "000D - SEQ_BLUE.minincsf",
            "000E - SEQ_AMBIENT_1.minincsf",
            "000F - SEQ_TELEPORT.minincsf",
            "0010 - SEQ_DRONE.minincsf",
            "0011 - SEQ_MENU1.minincsf",
            "0012 - SEQ_GREY.minincsf",
            "0013 - SEQ_SAFFRON.minincsf",
            "0014 - SEQ_GUMBO.minincsf",
            "0015 - SEQ_INTRO_SYLUX.minincsf",
            "0016 - SEQ_INTRO_TRACE.minincsf",
            "0017 - SEQ_INTRO_NOXUS.minincsf",
            "0018 - SEQ_INTRO_WEAVEL.minincsf",
            "0019 - SEQ_INTRO_KANDEN.minincsf",
            "001A - SEQ_INTRO_SPIRE.minincsf",
            "001B - SEQ_FLY_IN_2.minincsf",
            "001C - SEQ_FLY_IN_1.minincsf",
            "001D - SEQ_FLY_IN_3.minincsf",
            "001E - SEQ_FLY_IN_4.minincsf",
            "001F - SEQ_SHIP_LAND1.minincsf",
            "0020 - SEQ_SHIP_LAND2.minincsf",
            "0021 - SEQ_SHIP_LAND3.minincsf",
            "0022 - SEQ_SHIP_LAND4.minincsf",
            "0023 - SEQ_GET_WEAPON.minincsf",
            "0024 - SEQ_GET_OCTOLITH.minincsf",
            "0025 - SEQ_NEW_GAME.minincsf",
            "0026 - SEQ_BEAT_HUNTER1.minincsf",
            "0027 - SEQ_INTRO_GUARDIAN.minincsf",
            "0028 - SEQ_GUARDIAN.minincsf",
            "0029 - SEQ_BEAT_CYLBOSS1.minincsf",
            "002A - SEQ_GREEN.minincsf",
            "002B - SEQ_CHUTNEY.minincsf",
            "002C - SEQ_DILL.minincsf",
            "002D - SEQ_GOREA_1.minincsf",
            "002E - SEQ_ENEMY_1.minincsf",
            "002F - SEQ_GOREA_2.minincsf",
            "0030 - SEQ_PEPPER.minincsf",
            "0031 - SEQ_SINGLE_CART_MENU.minincsf",
            "0032 - SEQ_SINGLE_CART_INGAME.minincsf",
            "0033 - SEQ_SINGLE_CART_TIMEOUT.minincsf",
            "0034 - SEQ_OREGANO.minincsf",
            "0035 - SEQ_ENEMY_2.minincsf",
            "0036 - SEQ_WHITE.minincsf",
            "0037 - SEQ_ENERGY_TIMER.minincsf",
            "0038 - SEQ_BLACK.minincsf",
            "0039 - SEQ_INDIGO.minincsf",
            "003A - SEQ_CREDITS.minincsf",
            "003B - SEQ_FLY_IN_GOREA.minincsf"
        };
    }
}

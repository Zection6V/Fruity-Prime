#include "WeaponDps.hpp"

#include "NetLaunch.hpp"
#include "NetTestScript.hpp"
#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Culling.hpp"
#include "../../Scene.hpp"


#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace MphRead::Mods::Network::Detail
{
    // Scene.cs/Renderer.cs and the PlayerEntity network partials are owned by
    // later Native aggregate slices. Each declaration below represents exactly
    // one consumed managed operation and carries no fallback behavior.
    void WeaponDpsSetForceEveryone(bool value);
    [[nodiscard]] std::unique_ptr<MphRead::Scene> WeaponDpsCreateScene(WeaponDps& owner);
    void WeaponDpsSceneSetSize(
        MphRead::Scene& scene, OpenTK::Mathematics::Vector2i value);
    void WeaponDpsSceneOnLoad(MphRead::Scene& scene);
    void WeaponDpsSceneOnResize(MphRead::Scene& scene);
    void WeaponDpsSceneOnUpdateFrame(MphRead::Scene& scene);
    [[nodiscard]] bool WeaponDpsSceneOnRenderFrame(MphRead::Scene& scene);
    void WeaponDpsSceneAfterRenderFrame(MphRead::Scene& scene);
    void WeaponDpsSceneAddPlayer(
        MphRead::Scene& scene, Hunter hunter, std::int32_t recolor, std::int32_t team);
    void WeaponDpsSceneAddBattleRoom(
        MphRead::Scene& scene, const std::string& room, std::int32_t playerCount);
    [[nodiscard]] MphRead::Formats::Culling::NodeRef WeaponDpsSceneGetNodeRefByPosition(
        MphRead::Scene& scene, OpenTK::Mathematics::Vector3 position);

    void WeaponDpsApplyPause();

    void WeaponDpsPlayerModArmWeapon(Entities::PlayerEntity& player, BeamType beam);
    void WeaponDpsPlayerModSetAim(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 aim);
    [[nodiscard]] std::int32_t WeaponDpsPlayerModAmmoUa(Entities::PlayerEntity& player);

    // .NET formatting and exception metadata are runtime contracts rather than
    // C++ standard-library contracts, so these seams preserve those operations.
    [[nodiscard]] std::string WeaponDpsFormatFixed(double value, std::int32_t digits);
    [[nodiscard]] std::string WeaponDpsFormatInt32(std::int32_t value);
    [[nodiscard]] std::string WeaponDpsExceptionTypeName(const std::exception& ex);
    [[nodiscard]] std::string WeaponDpsExceptionMessage(const std::exception& ex);
    [[nodiscard]] std::optional<std::string> WeaponDpsExceptionStackTrace(
        const std::exception& ex);
    void WeaponDpsConsoleWriteLine(std::optional<std::string> value);
    void WeaponDpsDispose(WeaponDps& window);
}

namespace
{
    using OpenTK::Mathematics::Vector3;

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] std::int32_t AddInt32Unchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t SubtractInt32Unchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    void IncrementInt32Unchecked(std::int32_t& value) noexcept
    {
        value = AddInt32Unchecked(value, 1);
    }

    [[nodiscard]] float LengthSquared(Vector3 value) noexcept
    {
        return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
    }

    [[nodiscard]] Vector3 Normalize(Vector3 value)
    {
        const float length = std::sqrt(LengthSquared(value));
        return Vector3(value.X / length, value.Y / length, value.Z / length);
    }

    [[nodiscard]] Vector3 Add(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
    }

    [[nodiscard]] Vector3 Subtract(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
    }

    [[nodiscard]] Vector3 Scale(Vector3 value, float scale) noexcept
    {
        return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
    }

    [[nodiscard]] Vector3 Negate(Vector3 value) noexcept
    {
        return Vector3(-value.X, -value.Y, -value.Z);
    }

    [[nodiscard]] Vector3 AddY(Vector3 value, float y) noexcept
    {
        value.Y += y;
        return value;
    }

    [[nodiscard]] float DegreesToRadians(float degrees) noexcept
    {
        constexpr float DegreesToRadiansFactor = 0.01745329251994329576923690768489F;
        return degrees * DegreesToRadiansFactor;
    }

    [[nodiscard]] std::string HunterName(MphRead::Hunter value)
    {
        switch (value)
        {
        case MphRead::Hunter::Samus: return "Samus";
        case MphRead::Hunter::Kanden: return "Kanden";
        case MphRead::Hunter::Trace: return "Trace";
        case MphRead::Hunter::Sylux: return "Sylux";
        case MphRead::Hunter::Noxus: return "Noxus";
        case MphRead::Hunter::Spire: return "Spire";
        case MphRead::Hunter::Weavel: return "Weavel";
        case MphRead::Hunter::Guardian: return "Guardian";
        case MphRead::Hunter::Random: return "Random";
        }
        return std::to_string(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::string BeamName(MphRead::BeamType value)
    {
        switch (value)
        {
        case MphRead::BeamType::None: return "None";
        case MphRead::BeamType::PowerBeam: return "PowerBeam";
        case MphRead::BeamType::VoltDriver: return "VoltDriver";
        case MphRead::BeamType::Missile: return "Missile";
        case MphRead::BeamType::Battlehammer: return "Battlehammer";
        case MphRead::BeamType::Imperialist: return "Imperialist";
        case MphRead::BeamType::Judicator: return "Judicator";
        case MphRead::BeamType::Magmaul: return "Magmaul";
        case MphRead::BeamType::ShockCoil: return "ShockCoil";
        case MphRead::BeamType::OmegaCannon: return "OmegaCannon";
        case MphRead::BeamType::Platform: return "Platform";
        case MphRead::BeamType::Enemy: return "Enemy";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] float ClampDistance(float value) noexcept
    {
        if (value < 0.5F)
        {
            return 0.5F;
        }
        if (value > 40.0F)
        {
            return 40.0F;
        }
        return value;
    }
}

namespace MphRead::Mods::Network
{
    using OpenTK::Mathematics::Vector2i;
    using OpenTK::Mathematics::Vector3;

    std::int32_t WeaponDps::FullHealth(Entities::PlayerEntity& player)
    {
        return std::max<std::int32_t>(1, player.HealthMax());
    }

    RendererPlatform::WindowSettings WeaponDps::GameSettings()
    {
        RendererPlatform::WindowSettings settings{};
        settings.UpdateFrequency = 60;
        return settings;
    }

    RendererPlatform::WindowSettings WeaponDps::WindowSettings()
    {
        RendererPlatform::WindowSettings settings = GameSettings();
        settings.ClientSize = Vector2i(320, 180);
        settings.Title = "MphRead weapon probe";
        settings.Profile = RendererPlatform::WindowSettings::ContextProfile::Compatability;
        settings.Flags = RendererPlatform::WindowSettings::ContextFlags::Default;
        settings.ApiMajor = 3;
        settings.ApiMinor = 2;
        settings.StartVisible = false;
        return settings;
    }

    void WeaponDps::Run()
    {
        _window->Run(*this);
    }

    OpenTK::Mathematics::Vector2i WeaponDps::ClientSize() const
    {
        return _window->Size();
    }

    void WeaponDps::Close()
    {
        _window->Close();
    }

    void WeaponDps::SwapBuffers()
    {
        _window->SwapBuffers();
    }

    WeaponDps::WeaponDps(
        std::string room,
        Hunter hunter,
        BeamType beam,
        double seconds,
        float distance,
        bool bombs)
        : _window(RendererPlatform::CreateWindow(WindowSettings())),
          _room(std::move(room)),
          _hunter(hunter),
          _beam(beam),
          _seconds(seconds),
          _distance(distance),
          _bombs(bombs),
          _scene(nullptr)
    {
        Entities::PlayerEntity::SetMaxPlayers(
            std::max<std::int32_t>(Entities::PlayerEntity::MaxPlayers(), 2));
        Detail::WeaponDpsSetForceEveryone(true);
        _scene = Detail::WeaponDpsCreateScene(*this);

        Detail::WeaponDpsSceneAddPlayer(*_scene, Hunter::Samus, 0, -1);
        Detail::WeaponDpsSceneAddPlayer(*_scene, hunter, 0, -1);

        const auto& players = Entities::PlayerEntity::Players();
        for (std::int32_t i = 2;
            i < static_cast<std::int32_t>(players.size());
            i = AddInt32Unchecked(i, 1))
        {
            Entities::PlayerEntity& player
                = RequireReference(players[static_cast<std::size_t>(i)]);
            player.SetLoadFlags(player.LoadFlags() & ~Entities::LoadFlags::Active);
        }
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(players.size());
            i = AddInt32Unchecked(i, 1))
        {
            RequireReference(players[static_cast<std::size_t>(i)]).SetIsBot(false);
        }

        Entities::PlayerEntity::SetPlayerCount(2);
        Entities::PlayerEntity::SetMainPlayerIndex(0);
        Detail::WeaponDpsSceneAddBattleRoom(*_scene, _room, NetLaunch::RoomPlayerCount);
    }

    WeaponDps::~WeaponDps() = default;

    MphRead::Scene& WeaponDps::Scene() noexcept
    {
        return *_scene;
    }

    void WeaponDps::OnLoad()
    {
        Detail::WeaponDpsSceneSetSize(*_scene, ClientSize());
        Detail::WeaponDpsSceneOnLoad(*_scene);
        _window->BaseOnLoad();
        OpenTK::Graphics::OpenGL::GL::Viewport(
            0, 0, ClientSize().X, ClientSize().Y);
        Detail::WeaponDpsSceneOnResize(*_scene);
    }

    void WeaponDps::OnRenderFrame(const RendererPlatform::FrameEventArgs& args)
    {
        Detail::WeaponDpsApplyPause();
        Detail::WeaponDpsSceneOnUpdateFrame(*_scene);
        if (!Detail::WeaponDpsSceneOnRenderFrame(*_scene))
        {
            return;
        }

        IncrementInt32Unchecked(_frame);
        Step();
        SwapBuffers();
        Detail::WeaponDpsSceneAfterRenderFrame(*_scene);
        _window->BaseOnRenderFrame(args);

        if (_placed
            && static_cast<double>(SubtractInt32Unchecked(_frame, _placedFrame))
                >= _seconds * 60.0)
        {
            Close();
        }
        else if (static_cast<double>(_frame) > (_seconds + 20.0) * 60.0)
        {
            Close();
        }
    }

    bool WeaponDps::Alive(Entities::PlayerEntity& player)
    {
        const Entities::LoadFlags flags = player.LoadFlags();
        return (flags & Entities::LoadFlags::Active) == Entities::LoadFlags::Active
            && (flags & Entities::LoadFlags::Spawned) == Entities::LoadFlags::Spawned
            && player.Health() > 0;
    }

    void WeaponDps::Account(Entities::PlayerEntity& victim)
    {
        if (!_placed)
        {
            return;
        }

        if (_lastHealth >= 0 && victim.Health() < _lastHealth)
        {
            const std::int32_t drop
                = SubtractInt32Unchecked(_lastHealth, victim.Health());
            _damage = AddInt32Unchecked(_damage, drop);
            IncrementInt32Unchecked(_hits);
            _lastHitFrame = _firingFrames;
        }

        if (_killFrames < 0 && _lastHealth > 0 && victim.Health() == 0)
        {
            _killFrames = _firingFrames;
        }

        _lastHealth = victim.Health();
    }

    void WeaponDps::Step()
    {
        const auto& players = Entities::PlayerEntity::Players();
        if (players.size() < 2)
        {
            return;
        }

        const std::shared_ptr<Entities::PlayerEntity> victimReference = players[0];
        const std::shared_ptr<Entities::PlayerEntity> shooterReference = players[1];
        Entities::PlayerEntity& victim = RequireReference(victimReference);
        Entities::PlayerEntity& shooter = RequireReference(shooterReference);

        Account(victim);
        if (!Alive(shooter) || !Alive(victim))
        {
            NetTestScript::Rest(shooterReference, true);
            NetTestScript::Rest(victimReference, true);
            return;
        }

        if (!_bombs
            && (shooter.IsAltForm() || shooter.IsMorphing() || shooter.IsUnmorphing()))
        {
            NetTestScript::Rest(shooterReference, true);
            NetTestScript::Rest(victimReference, true);
            return;
        }

        if (!_placed)
        {
            Vector3 facing = victim.FacingVector();
            facing = Vector3(facing.X, 0.0F, facing.Z);
            facing = LengthSquared(facing) < 0.001F
                ? Vector3(0.0F, 0.0F, 1.0F)
                : Normalize(facing);

            const Vector3 victimPosition
                = static_cast<Vector3>(victim.Position);
            const Vector3 spot = Add(
                victimPosition,
                Scale(facing, _bombs ? 0.6F : _distance));
            shooter.Teleport(
                spot,
                Negate(facing),
                Detail::WeaponDpsSceneGetNodeRefByPosition(*_scene, spot));
            _placed = true;
            _placedFrame = _frame;
            _lastHealth = victim.Health();
            _startHealth = victim.Health();
        }

        if (_killFrames >= 0)
        {
            NetTestScript::Rest(shooterReference, true);
            NetTestScript::Rest(victimReference, true);
            return;
        }

        if (_bombs)
        {
            if (_hunter == Hunter::Sylux)
            {
                const float angle = DegreesToRadians(
                    static_cast<float>(_firingFrames) * 6.0F);
                const Vector3 offset(
                    std::cos(angle) * 2.4F,
                    0.0F,
                    std::sin(angle) * 2.4F);
                const Vector3 ring = Add(
                    static_cast<Vector3>(victim.Position), offset);
                shooter.Teleport(
                    ring,
                    Negate(Normalize(offset)),
                    Detail::WeaponDpsSceneGetNodeRefByPosition(*_scene, ring));
            }

            NetTestScript::LayBombs(shooterReference, _frame);
            NetTestScript::Rest(victimReference, true);

            auto enumerator = _scene->Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<Entities::EntityBase> entityReference
                    = enumerator.Current();
                Entities::EntityBase& entity = RequireReference(entityReference);
                if (entity.Type == EntityType::Bomb)
                {
                    IncrementInt32Unchecked(_beamFrames);
                    break;
                }
            }

            _lastAmmo = Detail::WeaponDpsPlayerModAmmoUa(shooter);
            HoldShooter(shooter);
            IncrementInt32Unchecked(_firingFrames);
            return;
        }

        if (static_cast<std::int32_t>(shooter.ShockCoilTimer()) > _worstShockCoilTimer)
        {
            _worstShockCoilTimer
                = static_cast<std::int32_t>(shooter.ShockCoilTimer());
        }

        Detail::WeaponDpsPlayerModArmWeapon(shooter, _beam);

        const Vector3 toVictim = Subtract(
            AddY(static_cast<Vector3>(victim.Position), 0.5F),
            AddY(static_cast<Vector3>(shooter.Position), 0.5F));
        if (LengthSquared(toVictim) > 0.001F)
        {
            Detail::WeaponDpsPlayerModSetAim(shooter, Normalize(toVictim));
        }

        NetTestScript::HoldFire(shooterReference, true);
        NetTestScript::Rest(victimReference, true);

        auto enumerator = _scene->Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            const std::shared_ptr<Entities::EntityBase> entityReference
                = enumerator.Current();
            Entities::EntityBase& entity = RequireReference(entityReference);
            if (entity.Type == EntityType::BeamProjectile)
            {
                const std::shared_ptr<Entities::BeamProjectileEntity> shot
                    = std::dynamic_pointer_cast<Entities::BeamProjectileEntity>(
                        entityReference);
                if (shot && shot->Owner() == shooterReference)
                {
                    IncrementInt32Unchecked(_beamFrames);
                    break;
                }
            }
        }

        _lastAmmo = Detail::WeaponDpsPlayerModAmmoUa(shooter);
        HoldShooter(shooter);
        IncrementInt32Unchecked(_firingFrames);
    }

    void WeaponDps::HoldShooter(Entities::PlayerEntity& shooter)
    {
        const std::int32_t floor = std::max<std::int32_t>(
            1,
            SubtractInt32Unchecked(FullHealth(shooter), HealHeadroom));
        if (shooter.Health() > floor)
        {
            _healed = AddInt32Unchecked(
                _healed,
                SubtractInt32Unchecked(shooter.Health(), floor));
        }
        shooter.SetHealth(floor);
    }

    std::int32_t WeaponDps::Report()
    {
        if (!_placed || _firingFrames == 0)
        {
            Detail::WeaponDpsConsoleWriteLine(
                "DPSFAIL " + _room + " | "
                + HunterName(_hunter) + " " + BeamName(_beam)
                + " | never got set up");
            return 1;
        }

        const double seconds = static_cast<double>(_firingFrames) / 60.0;
        const double window = _killFrames > 0
            ? static_cast<double>(_killFrames) / 60.0
            : seconds;
        const std::string kill = _killFrames > 0
            ? "killed " + Detail::WeaponDpsFormatInt32(_startHealth) + " hp in "
                + Detail::WeaponDpsFormatFixed(
                    static_cast<double>(_killFrames) / 60.0, 2)
                + " s"
            : "did not kill " + Detail::WeaponDpsFormatInt32(_startHealth) + " hp in "
                + Detail::WeaponDpsFormatFixed(seconds, 1)
                + " s";
        const std::string action = _bombs
            ? std::string("laying bombs")
            : "holding " + BeamName(_beam);

        Detail::WeaponDpsConsoleWriteLine(
            "DPS " + _room
            + " | " + HunterName(_hunter) + " " + action
            + " at "
            + Detail::WeaponDpsFormatFixed(
                static_cast<double>(_bombs ? 0.6F : _distance), 1)
            + " units | " + kill
            + " | damage " + Detail::WeaponDpsFormatInt32(_damage)
            + " | hits " + Detail::WeaponDpsFormatInt32(_hits)
            + " | "
            + Detail::WeaponDpsFormatFixed(
                static_cast<double>(_damage) / window, 1)
            + " per second | "
            + Detail::WeaponDpsFormatFixed(
                _hits > 0
                    ? static_cast<double>(_damage) / static_cast<double>(_hits)
                    : 0.0,
                1)
            + " per hit | "
            + Detail::WeaponDpsFormatFixed(
                static_cast<double>(_hits) / window, 1)
            + " hits per second | beam alive on "
            + Detail::WeaponDpsFormatInt32(_beamFrames) + " of "
            + Detail::WeaponDpsFormatInt32(_firingFrames) + " frame(s)"
            + " | shockCoilTimer " + Detail::WeaponDpsFormatInt32(_worstShockCoilTimer)
            + " (ramp needs 60 for +1, 240 for +4)"
            + " | victim ended on " + Detail::WeaponDpsFormatInt32(_lastHealth)
            + " hp | healed shooter " + Detail::WeaponDpsFormatInt32(_healed)
            + " hp | shooter ammo " + Detail::WeaponDpsFormatInt32(_lastAmmo)
            + " | last hit on firing frame " + Detail::WeaponDpsFormatInt32(_lastHitFrame)
            + " of " + Detail::WeaponDpsFormatInt32(_firingFrames));
        return 0;
    }

    std::int32_t WeaponDps::Run(
        std::string room,
        Hunter hunter,
        BeamType beam,
        double seconds,
        float distance,
        bool bombs)
    {
        std::unique_ptr<WeaponDps> window;
        std::int32_t result = 1;
        try
        {
            try
            {
                window = std::unique_ptr<WeaponDps>(new WeaponDps(
                    room,
                    hunter,
                    beam,
                    seconds,
                    ClampDistance(distance),
                    bombs));
                window->Run();
                result = window->Report();
            }
            catch (const std::exception& ex)
            {
                Detail::WeaponDpsConsoleWriteLine(
                    "DPSCRASH " + room + " | "
                    + Detail::WeaponDpsExceptionTypeName(ex) + ": "
                    + Detail::WeaponDpsExceptionMessage(ex));
                Detail::WeaponDpsConsoleWriteLine(
                    Detail::WeaponDpsExceptionStackTrace(ex));
                result = 1;
            }
        }
        catch (...)
        {
            if (window)
            {
                Detail::WeaponDpsDispose(*window);
            }
            throw;
        }

        if (window)
        {
            Detail::WeaponDpsDispose(*window);
        }
        return result;
    }
}

#include "PlayerInput.hpp"

#include "PlayerEntity.hpp"
#include "PlayerHud.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../BombEntity.hpp"
#include "../EnemyInstanceEntity.hpp"
#include "../ObjectEntity.hpp"
#include "../../Features.hpp"
#include "../../GameState.hpp"
#include "../../Program.hpp"
#include "../../Scene.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/Player.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Mods/Input/InputSettings.hpp"
#include "../../Mods/Input/PointerInput.hpp"
#include "../../Mods/Input/StylusZone.hpp"
#include "../../Mods/Network/NetDamage.hpp"
#include "../../Mods/Network/NetHooks.hpp"
#include "../../Mods/Network/NetPlayerBridge.hpp"
#include "../../Mods/Network/NetUnlagged.hpp"
#include "../../Mods/SpectatorMode.hpp"
#include "../../Utility/Rng.hpp"

#include <algorithm>
#include <any>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using MphRead::Fixed;
    using MphRead::Matrix;
    using MphRead::Entities::ButtonType;
    using MphRead::Entities::Keybind;
    using MphRead::Entities::PlayerEntity;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;
    using OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState;
    using OpenTK::Windowing::GraphicsLibraryFramework::Keys;
    using OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;
    using OpenTK::Windowing::GraphicsLibraryFramework::MouseState;

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestAny(TEnum value, TEnum flags) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flags)) != 0;
    }

    [[nodiscard]] constexpr Vector3 Add(Vector3 a, Vector3 b) noexcept
    {
        return Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    }

    [[nodiscard]] constexpr Vector3 Subtract(Vector3 a, Vector3 b) noexcept
    {
        return Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    }

    [[nodiscard]] constexpr Vector3 Multiply(Vector3 v, float s) noexcept
    {
        return Vector3(v.X * s, v.Y * s, v.Z * s);
    }

    [[nodiscard]] constexpr Vector3 Divide(Vector3 v, float s) noexcept
    {
        return Vector3(v.X / s, v.Y / s, v.Z / s);
    }

    [[nodiscard]] constexpr Vector3 WithX(Vector3 v, float x) noexcept
    {
        v.X = x;
        return v;
    }

    [[nodiscard]] constexpr Vector3 WithY(Vector3 v, float y) noexcept
    {
        v.Y = y;
        return v;
    }

    [[nodiscard]] constexpr Vector3 WithZ(Vector3 v, float z) noexcept
    {
        v.Z = z;
        return v;
    }

    [[nodiscard]] constexpr Vector3 AddX(Vector3 v, float x) noexcept
    {
        v.X += x;
        return v;
    }

    [[nodiscard]] constexpr Vector3 AddY(Vector3 v, float y) noexcept
    {
        v.Y += y;
        return v;
    }

    [[nodiscard]] constexpr Vector3 AddZ(Vector3 v, float z) noexcept
    {
        v.Z += z;
        return v;
    }

    [[nodiscard]] constexpr bool VectorEquals(Vector3 a, Vector3 b) noexcept
    {
        return a.X == b.X && a.Y == b.Y && a.Z == b.Z;
    }

    [[nodiscard]] float Length(Vector3 v) noexcept
    {
        return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
    }

    [[nodiscard]] float DegreesToRadians(float value) noexcept
    {
        return value * 0.01745329251994329576923690768489F;
    }

    [[nodiscard]] float RadiansToDegrees(float value) noexcept
    {
        return value * 57.295779513082320876798154814105F;
    }

    [[nodiscard]] float DotNetRound(float value) noexcept
    {
        if (!std::isfinite(value) || value == 0.0F)
        {
            return value;
        }
        const float floorValue = std::floor(value);
        const float fraction = value - floorValue;
        float rounded;
        if (fraction < 0.5F)
        {
            rounded = floorValue;
        }
        else if (fraction > 0.5F)
        {
            rounded = floorValue + 1.0F;
        }
        else
        {
            const float half = floorValue * 0.5F;
            rounded = half == std::floor(half) ? floorValue : floorValue + 1.0F;
        }
        return std::copysign(rounded, value);
    }

    [[nodiscard]] Matrix4 IdentityMatrix() noexcept
    {
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;
    constexpr std::uint32_t Prime4 = 668265263U;
    constexpr std::uint32_t Prime5 = 374761393U;

    [[nodiscard]] std::uint32_t GlobalHashSeed()
    {
        static const std::uint32_t seed = []
        {
            std::random_device device;
            std::uniform_int_distribution<std::uint32_t> distribution;
            return distribution(device);
        }();
        return seed;
    }

    [[nodiscard]] std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queuedValue) noexcept
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    [[nodiscard]] std::uint32_t MixFinal(std::uint32_t hash) noexcept
    {
        hash ^= hash >> 15;
        hash *= Prime2;
        hash ^= hash >> 13;
        hash *= Prime3;
        hash ^= hash >> 16;
        return hash;
    }

    [[nodiscard]] std::int32_t CombineHashCodes(
        std::int32_t value1, std::int32_t value2, std::int32_t value3)
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 12U;
        hash = QueueRound(hash, static_cast<std::uint32_t>(value1));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value2));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value3));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T, std::size_t Size>
    [[nodiscard]] T& ManagedAt(std::array<T, Size>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= Size)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T, std::size_t Size>
    [[nodiscard]] const T& ManagedAt(const std::array<T, Size>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= Size)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] T& ManagedAt(std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ManagedAt(const std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] T& ManagedAt(MphRead::ManagedArray<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.Length())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ManagedAt(const MphRead::ManagedArray<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.Length())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> ManagedCast(const std::shared_ptr<MphRead::Entities::EntityBase>& value)
    {
        if (!value)
        {
            return nullptr;
        }
        std::shared_ptr<T> cast = std::dynamic_pointer_cast<T>(value);
        if (!cast)
        {
            throw MphRead::SceneDetail::InvalidCastException();
        }
        return cast;
    }

    [[nodiscard]] std::size_t BeamIndex(MphRead::BeamType beam)
    {
        const auto value = static_cast<std::int32_t>(beam);
        if (value < 0 || value >= 9)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(value);
    }
}

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    bool KeyboardState::IsKeyDown(Keys key) const noexcept
    {
        const std::int32_t value = static_cast<std::int32_t>(key);
        return value >= 0 && static_cast<std::size_t>(value) < _down.size()
            && _down[static_cast<std::size_t>(value)];
    }

    void KeyboardState::SetKeyDown(Keys key, bool down) noexcept
    {
        const std::int32_t value = static_cast<std::int32_t>(key);
        if (value >= 0 && static_cast<std::size_t>(value) < _down.size())
        {
            _down[static_cast<std::size_t>(value)] = down;
        }
    }

    bool MouseState::IsButtonDown(MouseButton button) const noexcept
    {
        const std::int32_t value = static_cast<std::int32_t>(button);
        return value >= 0 && static_cast<std::size_t>(value) < _down.size()
            && _down[static_cast<std::size_t>(value)];
    }

    void MouseState::SetButtonDown(MouseButton button, bool down) noexcept
    {
        const std::int32_t value = static_cast<std::int32_t>(button);
        if (value >= 0 && static_cast<std::size_t>(value) < _down.size())
        {
            _down[static_cast<std::size_t>(value)] = down;
        }
    }
}

namespace MphRead::Entities
{
    Keybind::Keybind(Keys key) noexcept
        : _type(ButtonType::Key), _key(key)
    {
    }

    Keybind::Keybind(MouseButtonType mouseButton) noexcept
        : _type(ButtonType::Mouse), _mouseButton(mouseButton)
    {
    }

    Keybind::Keybind(ButtonType scrollType)
    {
        if (scrollType != ButtonType::ScrollUp && scrollType != ButtonType::ScrollDown)
        {
            throw MphRead::ProgramException("Unexpected control type.");
        }
        _type = scrollType;
    }

    bool operator==(const Keybind& lhs, const Keybind& rhs) noexcept
    {
        return lhs._type == rhs._type && lhs._key == rhs._key
            && lhs._mouseButton == rhs._mouseButton;
    }

    bool operator!=(const Keybind& lhs, const Keybind& rhs) noexcept
    {
        return lhs._type != rhs._type || lhs._key != rhs._key
            || lhs._mouseButton != rhs._mouseButton;
    }

    bool Keybind::Equals(const std::any& obj) const noexcept
    {
        const Keybind* other = std::any_cast<Keybind>(&obj);
        return other != nullptr && *this == *other;
    }

    std::int32_t Keybind::GetHashCode() const
    {
        return CombineHashCodes(
            static_cast<std::int32_t>(_type),
            static_cast<std::int32_t>(_key),
            static_cast<std::int32_t>(_mouseButton));
    }

    PlayerControls::PlayerControls(
        std::shared_ptr<Keybind> moveLeft, std::shared_ptr<Keybind> moveRight,
        std::shared_ptr<Keybind> moveUp, std::shared_ptr<Keybind> moveDown,
        std::shared_ptr<Keybind> rollLeft, std::shared_ptr<Keybind> rollRight,
        std::shared_ptr<Keybind> rollUp, std::shared_ptr<Keybind> rollDown,
        std::shared_ptr<Keybind> aimLeft, std::shared_ptr<Keybind> aimRight,
        std::shared_ptr<Keybind> aimUp, std::shared_ptr<Keybind> aimDown,
        std::shared_ptr<Keybind> shoot, std::shared_ptr<Keybind> zoom,
        std::shared_ptr<Keybind> jump, std::shared_ptr<Keybind> morph,
        std::shared_ptr<Keybind> boost, std::shared_ptr<Keybind> altAttack,
        std::shared_ptr<Keybind> scanVisor, std::shared_ptr<Keybind> scan,
        std::shared_ptr<Keybind> nextWeapon, std::shared_ptr<Keybind> prevWeapon,
        std::shared_ptr<Keybind> weaponMenu, std::shared_ptr<Keybind> powerBeam,
        std::shared_ptr<Keybind> missile, std::shared_ptr<Keybind> voltDriver,
        std::shared_ptr<Keybind> battlehammer, std::shared_ptr<Keybind> imperialist,
        std::shared_ptr<Keybind> judicator, std::shared_ptr<Keybind> magmaul,
        std::shared_ptr<Keybind> shockCoil, std::shared_ptr<Keybind> omegaCannon,
        std::shared_ptr<Keybind> affinitySlot, std::shared_ptr<Keybind> pause,
        std::shared_ptr<Keybind> hudOverlay)
        : _moveLeft(std::move(moveLeft)), _moveRight(std::move(moveRight)),
          _moveUp(std::move(moveUp)), _moveDown(std::move(moveDown)),
          _rollLeft(std::move(rollLeft)), _rollRight(std::move(rollRight)),
          _rollUp(std::move(rollUp)), _rollDown(std::move(rollDown)),
          _aimLeft(std::move(aimLeft)), _aimRight(std::move(aimRight)),
          _aimUp(std::move(aimUp)), _aimDown(std::move(aimDown)),
          _shoot(std::move(shoot)), _zoom(std::move(zoom)),
          _jump(std::move(jump)), _morph(std::move(morph)),
          _boost(std::move(boost)), _altAttack(std::move(altAttack)),
          _scanVisor(std::move(scanVisor)), _scan(std::move(scan)),
          _nextWeapon(std::move(nextWeapon)), _prevWeapon(std::move(prevWeapon)),
          _weaponMenu(std::move(weaponMenu)), _powerBeam(std::move(powerBeam)),
          _missile(std::move(missile)), _voltDriver(std::move(voltDriver)),
          _battlehammer(std::move(battlehammer)), _imperialist(std::move(imperialist)),
          _judicator(std::move(judicator)), _magmaul(std::move(magmaul)),
          _shockCoil(std::move(shockCoil)), _omegaCannon(std::move(omegaCannon)),
          _affinitySlot(std::move(affinitySlot)), _pause(std::move(pause)),
          _hudOverlay(std::move(hudOverlay)),
          _all{_moveLeft, _moveRight, _moveUp, _moveDown, _rollLeft, _rollRight,
              _rollUp, _rollDown, _aimLeft, _aimRight, _aimUp, _aimDown,
              _shoot, _zoom, _jump, _morph, _boost, _altAttack, _scanVisor, _scan,
              _nextWeapon, _prevWeapon, _weaponMenu, _powerBeam, _missile, _voltDriver,
              _battlehammer, _imperialist, _judicator, _magmaul, _shockCoil,
              _omegaCannon, _affinitySlot, _pause, _hudOverlay}
    {
        _mouseAim = true;
        _keyboardAim = true;
        _scrollAllWeapons = true;
    }

    Keybind& PlayerControls::RequireControl(std::shared_ptr<Keybind>& control)
    {
        return RequireReference(control);
    }

    const Keybind& PlayerControls::RequireControl(const std::shared_ptr<Keybind>& control) const
    {
        return RequireReference(control);
    }

    void PlayerControls::ClearAll()
    {
        for (const auto& bind : _all)
        {
            Keybind& control = RequireReference(bind);
            control.SetIsDown(false);
            control.SetIsPressed(false);
            control.SetIsReleased(false);
        }
    }

    void PlayerControls::ClearPressed()
    {
        for (const auto& bind : _all)
        {
            RequireReference(bind).SetIsPressed(false);
        }
    }

    PlayerControls PlayerControls::GetDefault()
    {
        PlayerControls controls = CreateDefault();
        MphRead::Mods::InputSettings::Apply(controls);
        return controls;
    }

    PlayerControls PlayerControls::CreateDefault()
    {
        const auto key = [](Keys value) { return std::make_shared<Keybind>(value); };
        const auto mouse = [](MouseButton value) { return std::make_shared<Keybind>(value); };
        const auto scroll = [](ButtonType value) { return std::make_shared<Keybind>(value); };

        std::shared_ptr<Keybind> moveLeft = key(Keys::A);
        std::shared_ptr<Keybind> moveRight = key(Keys::D);
        std::shared_ptr<Keybind> moveUp = key(Keys::W);
        std::shared_ptr<Keybind> moveDown = key(Keys::S);
        std::shared_ptr<Keybind> rollLeft = key(Keys::A);
        std::shared_ptr<Keybind> rollRight = key(Keys::D);
        std::shared_ptr<Keybind> rollUp = key(Keys::W);
        std::shared_ptr<Keybind> rollDown = key(Keys::S);
        std::shared_ptr<Keybind> aimLeft = key(Keys::Left);
        std::shared_ptr<Keybind> aimRight = key(Keys::Right);
        std::shared_ptr<Keybind> aimUp = key(Keys::Up);
        std::shared_ptr<Keybind> aimDown = key(Keys::Down);
        std::shared_ptr<Keybind> shoot = mouse(MouseButton::Left);
        std::shared_ptr<Keybind> zoom = mouse(MouseButton::Right);
        std::shared_ptr<Keybind> jump = key(Keys::Space);
        std::shared_ptr<Keybind> morph = key(Keys::C);
        std::shared_ptr<Keybind> boost = key(Keys::Space);
        std::shared_ptr<Keybind> altAttack = mouse(MouseButton::Left);
        std::shared_ptr<Keybind> scanVisor = key(Keys::E);
        std::shared_ptr<Keybind> scan = key(Keys::Q);
        std::shared_ptr<Keybind> nextWeapon = scroll(ButtonType::ScrollDown);
        std::shared_ptr<Keybind> prevWeapon = scroll(ButtonType::ScrollUp);
        std::shared_ptr<Keybind> weaponMenu = mouse(MouseButton::Middle);
        std::shared_ptr<Keybind> powerBeam = key(Keys::D1);
        std::shared_ptr<Keybind> missile = key(Keys::D2);
        std::shared_ptr<Keybind> voltDriver = key(Keys::D3);
        std::shared_ptr<Keybind> battlehammer = key(Keys::D4);
        std::shared_ptr<Keybind> imperialist = key(Keys::D5);
        std::shared_ptr<Keybind> judicator = key(Keys::D6);
        std::shared_ptr<Keybind> magmaul = key(Keys::D7);
        std::shared_ptr<Keybind> shockCoil = key(Keys::D8);
        std::shared_ptr<Keybind> omegaCannon = key(Keys::D9);
        std::shared_ptr<Keybind> affinitySlot = key(Keys::Unknown);
        std::shared_ptr<Keybind> pause = key(Keys::Tab);
        std::shared_ptr<Keybind> hudOverlay = key(Keys::LeftShift);

        return PlayerControls(
            std::move(moveLeft), std::move(moveRight), std::move(moveUp), std::move(moveDown),
            std::move(rollLeft), std::move(rollRight), std::move(rollUp), std::move(rollDown),
            std::move(aimLeft), std::move(aimRight), std::move(aimUp), std::move(aimDown),
            std::move(shoot), std::move(zoom), std::move(jump), std::move(morph),
            std::move(boost), std::move(altAttack), std::move(scanVisor), std::move(scan),
            std::move(nextWeapon), std::move(prevWeapon), std::move(weaponMenu),
            std::move(powerBeam), std::move(missile), std::move(voltDriver),
            std::move(battlehammer), std::move(imperialist), std::move(judicator),
            std::move(magmaul), std::move(shockCoil), std::move(omegaCannon),
            std::move(affinitySlot), std::move(pause), std::move(hudOverlay));
    }

    float PlayerEntity::PlayerInput::MouseDeltaX() const
    {
        if (MphRead::Mods::Input::StylusZone::OnButton())
        {
            return 0.0F;
        }
        const float current = MouseState.has_value() ? MouseState->X : 0.0F;
        const float previous = PrevMouseState.has_value() ? PrevMouseState->X : 0.0F;
        if (!MouseState.has_value() || !PrevMouseState.has_value())
        {
            return MphRead::Mods::Input::PointerInput::Filter(0.0F);
        }
        return MphRead::Mods::Input::PointerInput::Filter(current - previous);
    }

    float PlayerEntity::PlayerInput::MouseDeltaY() const
    {
        if (MphRead::Mods::Input::StylusZone::OnButton())
        {
            return 0.0F;
        }
        const float current = MouseState.has_value() ? MouseState->Y : 0.0F;
        const float previous = PrevMouseState.has_value() ? PrevMouseState->Y : 0.0F;
        if (!MouseState.has_value() || !PrevMouseState.has_value())
        {
            return MphRead::Mods::Input::PointerInput::Filter(0.0F);
        }
        return MphRead::Mods::Input::PointerInput::Filter(current - previous);
    }

    void PlayerEntity::ProcessInput()
    {
        if (_health > 0)
        {
            if (TestFlag(_flags1, PlayerFlags1::FreeLook))
            {
                _flags1 |= PlayerFlags1::FreeLookPrevious;
            }
            else
            {
                _flags1 &= ~PlayerFlags1::FreeLookPrevious;
            }
            _flags1 &= ~PlayerFlags1::FreeLook;
            _flags1 &= ~PlayerFlags1::Walking;
            _flags1 &= ~PlayerFlags1::Strafing;
            if (!_isBot)
            {
                ProcessTouchInput();
                if (GameState::Multiplayer && !TestFlag(_flags1, PlayerFlags1::WeaponMenuOpen)
                    && _controls.Pause().IsDown())
                {
                    _showScoreboard = true;
                }
                else
                {
                    _showScoreboard = false;
                }
            }
            if (_frozenTimer > 0)
            {
                --_frozenTimer;
                _timeSinceFrozen = 0;
                if (_frozenTimer == 0)
                {
                    _soundSource.PlaySfx(SfxId::SHOTGUN_BREAK_FREEZE);
                    if (IsAltForm())
                    {
                        CreateIceBreakEffectAlt();
                    }
                    else if (IsMainPlayer())
                    {
                        CreateIceBreakEffectGun();
                    }
                    else if (TestFlag(_flags2, PlayerFlags2::DrawnThirdPerson))
                    {
                        const std::int32_t lod = TestFlag(_flags2, PlayerFlags2::Lod1) ? 1 : 0;
                        CreateIceBreakEffectBiped(RequireReference(ManagedAt(_bipedModelLods, lod)).Model());
                    }
                }
            }
            if (_frozenGfxTimer > 0)
            {
                --_frozenGfxTimer;
                if (IsMainPlayer() && _frozenGfxTimer == 0)
                {
                    _drawIceLayer = false;
                }
            }
            if (_timeSinceFrozen != std::numeric_limits<std::uint16_t>::max())
            {
                ++_timeSinceFrozen;
            }
        }
        else
        {
            _showScoreboard = GameState::Multiplayer && _controls.Pause().IsDown();
        }
        if (IsAltForm() || IsMorphing())
        {
            ProcessAlt();
        }
        else
        {
            ProcessBiped();
        }
    }

    void PlayerEntity::ApplyStylusZone(PlayerEntity& player)
    {
        if (!Mods::Input::StylusZone::OnButton())
        {
            return;
        }
        PlayerControls& controls = player._controls;
        controls.Shoot().SetIsDown(false);
        controls.Shoot().SetIsPressed(false);
        controls.Shoot().SetIsReleased(false);
        const Mods::Input::StylusRegion pressed = Mods::Input::StylusZone::Pressed();
        if (pressed == Mods::Input::StylusRegion::None)
        {
            return;
        }
        Keybind* bind = nullptr;
        switch (pressed)
        {
        case Mods::Input::StylusRegion::PowerBeam: bind = controls._powerBeam.get(); break;
        case Mods::Input::StylusRegion::Missile: bind = controls._missile.get(); break;
        case Mods::Input::StylusRegion::Weapons: bind = controls._nextWeapon.get(); break;
        case Mods::Input::StylusRegion::WeaponSelect: bind = controls._weaponMenu.get(); break;
        case Mods::Input::StylusRegion::AltForm: bind = controls._morph.get(); break;
        default: break;
        }
        if (bind != nullptr)
        {
            bind->SetIsDown(true);
            bind->SetIsPressed(true);
            bind->SetIsReleased(false);
            player._input.HasInput = true;
        }
    }

    void PlayerEntity::ProcessTouchInput()
    {
        if (GameState::SinglePlayer && _controls.ScanVisor().IsPressed()
            && !TestFlag(_flags1, PlayerFlags1::WeaponMenuOpen)
            && !IsAltForm() && !IsMorphing())
        {
            if (_scanVisor)
            {
                SwitchVisors(false);
            }
            else
            {
                SwitchVisors(false);
                UpdateZoom(false);
            }
        }
        if ((GameState::Multiplayer || _weaponSlots[2] != BeamType::OmegaCannon)
            && _controls.WeaponMenu().IsDown())
        {
            _flags1 |= PlayerFlags1::NoAimInput;
            _flags1 |= PlayerFlags1::WeaponMenuOpen;
            _showScoreboard = false;
        }
        bool selected = false;
        if (!_controls.WeaponMenu().IsDown())
        {
            selected = EndWeaponMenu();
        }
        if (!selected)
        {
            const auto equipIfDifferent = [this](Keybind& bind, BeamType beam)
            {
                if (bind.IsPressed() && _currentWeapon != beam)
                {
                    (void)TryEquipWeapon(beam, false, true);
                    return true;
                }
                return bind.IsPressed();
            };
            if (equipIfDifferent(_controls.PowerBeam(), BeamType::PowerBeam)) {}
            else if (equipIfDifferent(_controls.Missile(), BeamType::Missile)) {}
            else if (equipIfDifferent(_controls.VoltDriver(), BeamType::VoltDriver)) {}
            else if (equipIfDifferent(_controls.Battlehammer(), BeamType::Battlehammer)) {}
            else if (equipIfDifferent(_controls.Imperialist(), BeamType::Imperialist)) {}
            else if (equipIfDifferent(_controls.Judicator(), BeamType::Judicator)) {}
            else if (equipIfDifferent(_controls.Magmaul(), BeamType::Magmaul)) {}
            else if (equipIfDifferent(_controls.ShockCoil(), BeamType::ShockCoil)) {}
            else if (equipIfDifferent(_controls.OmegaCannon(), BeamType::OmegaCannon)) {}
            else if (_controls.AffinitySlot().IsPressed())
            {
                const BeamType weapon = _weaponSlots[2];
                if (weapon != BeamType::None && _currentWeapon != weapon)
                {
                    (void)TryEquipWeapon(weapon);
                }
            }
            else if (_controls.ScrollAllWeapons()
                || (_currentWeapon != BeamType::PowerBeam && _currentWeapon != BeamType::Missile))
            {
                std::int32_t currentIndex = -1;
                for (std::int32_t i = 0; i < static_cast<std::int32_t>(_weaponOrder.size()); ++i)
                {
                    if (_weaponOrder[static_cast<std::size_t>(i)] == _currentWeapon)
                    {
                        currentIndex = i;
                    }
                }
                std::int32_t nextIndex = currentIndex;
                BeamType nextBeam = _currentWeapon;
                std::int32_t steps = static_cast<std::int32_t>(_weaponOrder.size());
                if (_controls.NextWeapon().IsPressed())
                {
                    do
                    {
                        ++nextIndex;
                        if (_controls.ScrollAllWeapons() && nextIndex > 8)
                        {
                            nextIndex = 0;
                        }
                        else if (!_controls.ScrollAllWeapons() && nextIndex > 7)
                        {
                            nextIndex = 2;
                        }
                        nextBeam = _weaponOrder.at(static_cast<std::size_t>(nextIndex));
                    }
                    while (nextIndex != currentIndex && --steps > 0 && !CanCycleToWeapon(nextBeam));
                }
                else if (_controls.PrevWeapon().IsPressed())
                {
                    do
                    {
                        --nextIndex;
                        if (_controls.ScrollAllWeapons() && nextIndex < 0)
                        {
                            nextIndex = 8;
                        }
                        else if (!_controls.ScrollAllWeapons() && nextIndex < 2)
                        {
                            nextIndex = 7;
                        }
                        nextBeam = _weaponOrder.at(static_cast<std::size_t>(nextIndex));
                    }
                    while (nextIndex != currentIndex && --steps > 0 && !CanCycleToWeapon(nextBeam));
                }
                if (nextBeam != _currentWeapon)
                {
                    (void)TryEquipWeapon(nextBeam);
                }
            }
        }
    }

    bool PlayerEntity::CanCycleToWeapon(BeamType beam)
    {
        if (!_availableWeapons[beam])
        {
            return false;
        }
        const WeaponInfo& info = Weapons::Current[BeamIndex(beam)];
        const std::int32_t ammo = ManagedAt(_ammo, info.AmmoType);
        return beam == BeamType::PowerBeam || ammo == -1 || ammo >= info.AmmoCost;
    }

    bool PlayerEntity::EndWeaponMenu()
    {
        bool selected = false;
        if (_weaponSelection != BeamType::None)
        {
            if (_weaponSelection != _currentWeapon)
            {
                (void)TryEquipWeapon(_weaponSelection);
                selected = true;
            }
            else if (IsMainPlayer() && TestFlag(_flags1, PlayerFlags1::WeaponMenuOpen))
            {
                _soundSource.PlayFreeSfx(SfxId::BEAM_SWITCH_FAIL);
            }
            _weaponSelection = _currentWeapon;
        }
        _flags1 &= ~PlayerFlags1::NoAimInput;
        _flags1 &= ~PlayerFlags1::WeaponMenuOpen;
        return selected;
    }

    void PlayerEntity::UpdateAimFacing()
    {
        if (Features::FixedCrosshair)
        {
            _facingVector = _gunVec1;
            return;
        }
        const float dot = Vector3::Dot(_gunVec1, _facingVector);
        if (dot < Fixed::ToFloat(3956))
        {
            const Vector3 temp1 = Subtract(_facingVector, Multiply(_gunVec1, dot)).Normalized();
            _facingVector = Add(Multiply(_gunVec1, Fixed::ToFloat(3956)),
                Multiply(temp1, Fixed::ToFloat(1060)));
        }
        _facingVector = Add(_facingVector, Multiply(Subtract(_gunVec1, _facingVector), 0.1F)).Normalized();
    }

    void PlayerEntity::UpdateAimY(float amount)
    {
        if (_controls.InvertAimY())
        {
            amount *= -1.0F;
        }
        float sensitivity = 1.0F;
        if (_equipInfo->Zoomed)
        {
            const float normalFov = Fixed::ToFloat(_values.NormalFov) * 2.0F;
            if (normalFov != 0.0F)
            {
                sensitivity = _cameraInfo->Fov / normalFov;
            }
        }
        amount *= sensitivity;
        const float prevAim = _aimY;
        _aimY += amount;
        _aimY = IsAltForm() ? std::clamp(_aimY, -25.0F, 5.0F) : std::clamp(_aimY, -85.0F, 85.0F);
        const float diff = DegreesToRadians(_aimY - prevAim);
        const Matrix4 transform = GetTransformMatrix(_gunVec1, Vector3(0.0F, 1.0F, 0.0F));
        Vector3 vector;
        if (diff <= 0.0F)
        {
            vector = Vector3(0.0F, std::sin(diff), std::cos(diff));
        }
        else
        {
            vector = Vector3(0.0F, -std::sin(-diff), std::cos(-diff));
        }
        _gunVec1 = Matrix::Vec3MultMtx3(vector, transform).Normalized();
        _aimPosition = Add(_cameraInfo->Position, Multiply(_gunVec1, Fixed::ToFloat(_values.AimDistance)));
        UpdateAimFacing();
    }

    void PlayerEntity::UpdateAimX(float amount)
    {
        if (_controls.InvertAimX())
        {
            amount *= -1.0F;
        }
        float sensitivity = 1.0F;
        if (_equipInfo->Zoomed)
        {
            const float normalFov = Fixed::ToFloat(_values.NormalFov) * 2.0F;
            if (normalFov != 0.0F)
            {
                sensitivity = _cameraInfo->Fov / normalFov;
            }
        }
        amount *= sensitivity;
        const float angle = DegreesToRadians(amount);
        float sinValue;
        float cosValue;
        if (amount <= 0.0F)
        {
            sinValue = std::sin(angle);
            cosValue = std::cos(angle);
        }
        else
        {
            sinValue = -std::sin(-angle);
            cosValue = std::cos(-angle);
        }
        const float x = _gunVec1.X;
        const float z = _gunVec1.Z;
        _gunVec1.X = x * cosValue + z * sinValue;
        _gunVec1.Z = x * -sinValue + z * cosValue;
        _gunVec1 = _gunVec1.Normalized();
        _aimPosition = Add(_cameraInfo->Position, Multiply(_gunVec1, Fixed::ToFloat(_values.AimDistance)));
        if (_equipInfo->Zoomed)
        {
            _facingVector = _gunVec1;
        }
        else
        {
            UpdateAimFacing();
        }
    }

    void PlayerEntity::UpdateHudShiftY(float amount)
    {
        if (IsMainPlayer())
        {
            float sum = 0.0F;
            for (std::int32_t i = 6; i >= 0; --i)
            {
                const float past = _pastAimY[static_cast<std::size_t>(i)];
                sum += past;
                _pastAimY[static_cast<std::size_t>(i + 1)] = past;
            }
            _pastAimY[0] = amount;
            if (Features::HudSway && !Features::FixedWeapon)
            {
                const float average = (sum + amount) / 8.0F;
                _hudShiftY = std::clamp(-DotNetRound(average), -8.0F, 8.0F);
            }
            else
            {
                _hudShiftY = 0.0F;
            }
            _objShiftY = -_hudShiftY / 2.0F;
        }
    }

    void PlayerEntity::UpdateHudShiftX(float amount)
    {
        if (IsMainPlayer())
        {
            float sum = 0.0F;
            for (std::int32_t i = 6; i >= 0; --i)
            {
                const float past = _pastAimX[static_cast<std::size_t>(i)];
                sum += past;
                _pastAimX[static_cast<std::size_t>(i + 1)] = past;
            }
            _pastAimX[0] = amount;
            if (Features::HudSway && !Features::FixedWeapon)
            {
                const float average = (sum + amount) / 8.0F;
                _hudShiftX = std::clamp(DotNetRound(average), -8.0F, 8.0F);
            }
            else
            {
                _hudShiftX = 0.0F;
            }
            _objShiftX = _hudShiftX / 2.0F;
        }
    }

    void PlayerEntity::ProcessBiped()
    {
        if (IsMainPlayer() && GameState::SinglePlayer && CameraSequence::Current != nullptr)
        {
            _timeIdle = 0;
        }
        if (_equipInfo->SmokeLevel < _equipInfo->Weapon.SmokeDrain)
        {
            _equipInfo->SmokeLevel = 0;
        }
        else
        {
            _equipInfo->SmokeLevel -= _equipInfo->Weapon.SmokeDrain;
        }
        Vector3 speedDelta{};
        PlayerAnimation anim1 = PlayerAnimation::None;
        PlayerAnimation anim2 = PlayerAnimation::None;
        AnimFlags animFlags1 = AnimFlags::None;
        AnimFlags animFlags2 = AnimFlags::None;
        if (_frozenTimer == 0 && _health > 0 && !_field6D0)
        {
            if (Biped1Anim() == PlayerAnimation::Turn)
            {
                AnimFlags flags = Biped1Flags();
                if (Biped1Frame() <= Biped1FrameCount() / 2)
                {
                    flags |= AnimFlags::Reverse;
                }
                else
                {
                    flags &= ~AnimFlags::Reverse;
                }
                flags |= AnimFlags::NoLoop;
                SetBiped1Flags(flags);
            }
            if (Biped2Anim() == PlayerAnimation::Turn)
            {
                AnimFlags flags = Biped2Flags();
                if (Biped2Frame() <= Biped2FrameCount() / 2)
                {
                    flags |= AnimFlags::Reverse;
                }
                else
                {
                    flags &= ~AnimFlags::Reverse;
                }
                flags |= AnimFlags::NoLoop;
                SetBiped2Flags(flags);
            }
            ApplyModAim();
            if (_controls.MouseAim() && !TestFlag(_flags1, PlayerFlags1::NoAimInput) && !_isBot)
            {
                float aimY = -_input.MouseDeltaY() / 4.0F * Mods::InputSettings::MouseSensitivity()
                    * (Mods::InputSettings::InvertMouseY() ? -1.0F : 1.0F);
                float aimX = -_input.MouseDeltaX() / 4.0F * Mods::InputSettings::MouseSensitivity()
                    * (Mods::InputSettings::InvertMouseX() ? -1.0F : 1.0F);
                if ((CameraSequence::Current != nullptr
                        && TestFlag(CameraSequence::Current->Flags, CamSeqFlags::BlockInput))
                    || RequireReference(_scene).FrameAdvance || RequireReference(_scene).FrameAdvanceLastFrame)
                {
                    aimX = aimY = 0.0F;
                }
                if (_controls.KeyboardAim()
                    && (_controls.AimLeft().IsDown() || _controls.AimRight().IsDown()
                        || _controls.AimUp().IsDown() || _controls.AimDown().IsDown()))
                {
                    aimX = aimY = 0.0F;
                }
                if (aimX != 0.0F || aimY != 0.0F)
                {
                    _input.HasInput = true;
                }
                UpdateHudShiftY(aimY);
                UpdateHudShiftX(aimX);
                UpdateAimY(aimY);
                UpdateAimX(aimX);
                if (TestFlag(_flags1, PlayerFlags1::Grounded))
                {
                    if (aimX > 3.0F)
                    {
                        _timeIdle = 0;
                        anim1 = PlayerAnimation::Turn;
                        animFlags1 = AnimFlags::Reverse;
                        if (Biped2Anim() == PlayerAnimation::Turn)
                        {
                            ManagedAt(RequireReference(RequireReference(_bipedModel2).AnimInfo->Flags), 0) &= ~AnimFlags::NoLoop;
                            ManagedAt(RequireReference(RequireReference(_bipedModel2).AnimInfo->Flags), 0) |= AnimFlags::Reverse;
                        }
                        if (Biped1Anim() == PlayerAnimation::Turn)
                        {
                            ManagedAt(RequireReference(RequireReference(_bipedModel1).AnimInfo->Flags), 0) &= ~AnimFlags::NoLoop;
                            ManagedAt(RequireReference(RequireReference(_bipedModel1).AnimInfo->Flags), 0) |= AnimFlags::Reverse;
                        }
                    }
                    else if (aimX < -3.0F)
                    {
                        _timeIdle = 0;
                        anim1 = PlayerAnimation::Turn;
                        if (Biped2Anim() == PlayerAnimation::Turn)
                        {
                            ManagedAt(RequireReference(RequireReference(_bipedModel2).AnimInfo->Flags), 0) &= ~AnimFlags::NoLoop;
                            ManagedAt(RequireReference(RequireReference(_bipedModel2).AnimInfo->Flags), 0) &= ~AnimFlags::Reverse;
                        }
                        if (Biped1Anim() == PlayerAnimation::Turn)
                        {
                            ManagedAt(RequireReference(RequireReference(_bipedModel1).AnimInfo->Flags), 0) &= ~AnimFlags::NoLoop;
                            ManagedAt(RequireReference(RequireReference(_bipedModel1).AnimInfo->Flags), 0) &= ~AnimFlags::Reverse;
                        }
                    }
                }
            }
            if (_controls.KeyboardAim() || _isBot)
            {
                UpdateAimX(_buttonAimX);
                UpdateAimY(_buttonAimY);
            }
            bool jumping = false;
            if (!TestAny(_flags2, PlayerFlags2::BipedLock | PlayerFlags2::BipedStuck))
            {
                const auto moveRightLeft = [&](PlayerAnimation walkAnim, std::int32_t sign)
                {
                    _flags1 |= PlayerFlags1::Strafing;
                    _flags1 |= PlayerFlags1::MovingBiped;
                    if (TestFlag(_flags1, PlayerFlags1::Standing))
                    {
                        _flags1 |= PlayerFlags1::Walking;
                    }
                    else
                    {
                        _flags1 &= ~PlayerFlags1::Walking;
                    }
                    float traction = Fixed::ToFloat(_values.StrafeBipedTraction);
                    if (_jumpPadControlLockMin > 0)
                    {
                        traction *= Fixed::ToFloat(_values.JumpPadSlideFactor);
                    }
                    else if (TestFlag(_flags1, PlayerFlags1::Standing) && _slipperiness != 0)
                    {
                        traction *= ManagedAt(Metadata::TractionFactors, _slipperiness);
                    }
                    speedDelta.X -= _field78 * traction * static_cast<float>(sign);
                    speedDelta.Z -= _field7C * traction * static_cast<float>(sign);
                    if (!_controls.MoveUp().IsDown() && !_controls.MoveDown().IsDown()
                        && TestFlag(_flags1, PlayerFlags1::Grounded) && _timeSinceJumpPad > 14)
                    {
                        anim1 = walkAnim;
                    }
                    if (!_equipInfo->Zoomed)
                    {
                        _viewTiltAngleH += Fixed::ToFloat(_values.ViewTiltIncrement)
                            * static_cast<float>(sign) / 2.0F;
                        _viewTiltAngleH = std::clamp(_viewTiltAngleH, -180.0F, 180.0F);
                    }
                };

                const auto moveForwardBack = [&](PlayerAnimation walkAnim, std::int32_t sign)
                {
                    _flags1 |= PlayerFlags1::MovingBiped;
                    if (TestFlag(_flags1, PlayerFlags1::Standing))
                    {
                        _flags1 |= PlayerFlags1::Walking;
                    }
                    else
                    {
                        _flags1 &= ~PlayerFlags1::Walking;
                    }
                    float traction = Fixed::ToFloat(_values.WalkBipedTraction);
                    if (_jumpPadControlLockMin > 0)
                    {
                        traction *= Fixed::ToFloat(_values.JumpPadSlideFactor);
                    }
                    else if (TestFlag(_flags1, PlayerFlags1::Standing) && _slipperiness != 0)
                    {
                        traction *= ManagedAt(Metadata::TractionFactors, _slipperiness);
                    }
                    speedDelta.X += _field70 * traction * static_cast<float>(sign);
                    speedDelta.Z += _field74 * traction * static_cast<float>(sign);
                    if (TestFlag(_flags1, PlayerFlags1::Grounded) && _timeSinceJumpPad > 14)
                    {
                        anim1 = walkAnim;
                    }
                    if (!_equipInfo->Zoomed)
                    {
                        _viewTiltAngleV += Fixed::ToFloat(_values.ViewTiltIncrement)
                            * static_cast<float>(sign) / 2.0F;
                        _viewTiltAngleV = std::clamp(_viewTiltAngleV, -180.0F, 180.0F);
                    }
                };

                if (_controls.MoveRight().IsDown())
                {
                    moveRightLeft(PlayerAnimation::WalkRight, 1);
                }
                else if (_controls.MoveLeft().IsDown())
                {
                    moveRightLeft(PlayerAnimation::WalkLeft, -1);
                }
                if (_viewTiltAngleH < Fixed::ToFloat(500) && _viewTiltAngleH > Fixed::ToFloat(-500))
                {
                    _viewTiltAngleH = 0.0F;
                }
                else
                {
                    _viewTiltAngleH *= 0.9F;
                }
                if (_controls.MoveUp().IsDown())
                {
                    moveForwardBack(PlayerAnimation::WalkForward, 1);
                }
                else if (_controls.MoveDown().IsDown())
                {
                    moveForwardBack(PlayerAnimation::WalkBackward, -1);
                }
                if (_viewTiltAngleV < Fixed::ToFloat(500) && _viewTiltAngleV > Fixed::ToFloat(-500))
                {
                    _viewTiltAngleV = 0.0F;
                }
                else
                {
                    _viewTiltAngleV *= 0.9F;
                }
                if (Cheats::UnlimitedJumps)
                {
                    _flags1 &= ~PlayerFlags1::UsedJump;
                }
                if (_jumpPadControlLockMin == 0 && _controls.Jump().IsPressed()
                    && !TestFlag(_flags1, PlayerFlags1::UsedJump))
                {
                    jumping = true;
                    if (!TestFlag(_flags1, PlayerFlags1::Standing)
                        || !TestFlag(_abilities, AbilityFlags::SpaceJump))
                    {
                        _flags1 |= PlayerFlags1::UsedJump;
                    }
                    _speed = WithY(_speed, IsPrimeHunter() ? 0.35F : Fixed::ToFloat(_values.JumpSpeed));
                    _timeSinceGrounded = 16;
                    PlayHunterSfx(HunterSfx::Jump);
                }
            }
            if (jumping || _timeSinceJumpPad == 1)
            {
                animFlags1 = AnimFlags::NoLoop;
                if (_controls.MoveUp().IsDown()) anim1 = PlayerAnimation::JumpForward;
                else if (_controls.MoveDown().IsDown()) anim1 = PlayerAnimation::JumpBack;
                else if (_controls.MoveLeft().IsDown()) anim1 = PlayerAnimation::JumpLeft;
                else if (_controls.MoveRight().IsDown()) anim1 = PlayerAnimation::JumpRight;
                else anim1 = PlayerAnimation::JumpNeutral;
            }
        }

        ProcessMovement();
        Mods::Network::NetHooks::AfterRemoteMovement(*this);
        UpdateCamera();
        ModRefreshNetworkAim();
        UpdateAimVecs();

        if (_frozenTimer == 0 && _health > 0 && !_field6D0)
        {
            bool scanInput = false;
            if (_scanVisor)
            {
                scanInput = true;
                if ((!_scanning && _controls.Scan().IsPressed())
                    || (_scanning && _controls.Scan().IsDown()))
                {
                    UpdateScanning(true);
                }
                else
                {
                    UpdateScanning(false);
                    if (_controls.Scan() != _controls.Shoot()
                        && (_controls.Shoot().IsPressed() || _controls.Morph().IsPressed()))
                    {
                        SwitchVisors(false);
                        scanInput = false;
                    }
                }
                if (_equipInfo->ChargeLevel > 0)
                {
                    _equipInfo->ChargeLevel = 0;
                    StopBeamChargeSfx(_currentWeapon);
                }
            }
            if (!scanInput && !IsUnmorphing())
            {
                if (!_controls.Shoot().IsDown())
                {
                    _flags2 &= ~PlayerFlags2::Shooting;
                }
                else if (_controls.Shoot().IsPressed() || !TestFlag(_flags2, PlayerFlags2::NoShotsFired))
                {
                    _flags2 |= PlayerFlags2::Shooting;
                    _flags2 &= ~PlayerFlags2::NoShotsFired;
                }
                const WeaponInfo& equipWeapon = EquipWeapon();
                if (!_availableCharges[_currentWeapon] || !TestFlag(equipWeapon.Flags, WeaponFlags::CanCharge))
                {
                    _equipInfo->ChargeLevel = 0;
                }
                else
                {
                    bool releaseCharge = false;
                    if (!TestFlag(_flags2, PlayerFlags2::Shooting)
                        || _equipInfo->Ammo < equipWeapon.ChargeCost)
                    {
                        releaseCharge = true;
                    }
                    else
                    {
                        if (_equipInfo->ChargeLevel > 0 && _gunAnimation != GunAnimation::MissileClose)
                        {
                            if (_currentWeapon != BeamType::PowerBeam
                                || _equipInfo->ChargeLevel >= _equipInfo->Weapon.MinCharge * 2)
                            {
                                PlayBeamChargeSfx(_currentWeapon);
                            }
                            if (TestFlag(Biped2Flags(), AnimFlags::Ended)
                                || Biped2Anim() == PlayerAnimation::Charge
                                || (Biped2Anim() == PlayerAnimation::Shoot && Biped2Frame() > 8))
                            {
                                anim2 = PlayerAnimation::Charge;
                            }
                        }
                        if (_equipInfo->ChargeLevel >= equipWeapon.FullCharge * 2)
                        {
                            _equipInfo->SmokeLevel = static_cast<std::uint16_t>(
                                _equipInfo->SmokeLevel + equipWeapon.SmokeChargeAmount);
                            _equipInfo->SmokeLevel = static_cast<std::uint16_t>(std::min<std::int32_t>(
                                _equipInfo->SmokeLevel, equipWeapon.SmokeStart * 2));
                        }
                        else
                        {
                            ++_equipInfo->ChargeLevel;
                            const std::int32_t minCharge = equipWeapon.MinCharge * 2;
                            if (_equipInfo->ChargeLevel > minCharge)
                            {
                                const std::int32_t fullCharge = equipWeapon.FullCharge * 2;
                                const std::int32_t chargeCost = equipWeapon.ChargeCost * 2;
                                const std::int32_t minCost = equipWeapon.MinChargeCost * 2;
                                const std::int32_t cost = minCost + (chargeCost - minCost)
                                    * (_equipInfo->ChargeLevel - minCharge) / (fullCharge - minCharge);
                                if (_equipInfo->Ammo < cost / 2)
                                {
                                    --_equipInfo->ChargeLevel;
                                }
                            }
                        }
                    }
                    if (releaseCharge)
                    {
                        StopBeamChargeSfx(_currentWeapon);
                        if (_equipInfo->ChargeLevel >= equipWeapon.MinCharge * 2)
                        {
                            (void)TryFireWeapon();
                            anim2 = PlayerAnimation::ChargeShoot;
                            animFlags2 = AnimFlags::NoLoop;
                        }
                        _equipInfo->ChargeLevel = 0;
                    }
                }

                if (TestFlag(equipWeapon.Flags, WeaponFlags::CanZoom))
                {
                    if (_controls.Zoom().IsPressed())
                    {
                        UpdateZoom(!_equipInfo->Zoomed);
                    }
                    if (_equipInfo->Zoomed && CameraSequence::Current == nullptr)
                    {
                        float zoomFov = Fixed::ToFloat(_equipInfo->Weapon.ZoomFov);
                        const Vector3 facing = _facingVector;
                        const auto checkZoomTargets = [&](EntityType type)
                        {
                            for (const auto& entity : RequireReference(_scene).Entities())
                            {
                                EntityBase& target = RequireReference(entity);
                                if (target.Type != type || entity.get() == this || !target.GetTargetable())
                                {
                                    continue;
                                }
                                if (target.Type == EntityType::Object)
                                {
                                    const auto object = ManagedCast<ObjectEntity>(entity);
                                    if (!TestFlag(object->Data.EffectFlags, ObjEffFlags::WeaponZoom))
                                    {
                                        continue;
                                    }
                                }
                                Vector3 position{};
                                target.GetPosition(position);
                                const Vector3 between = Subtract(position, static_cast<Vector3>(Position));
                                const float dot = Vector3::Dot(between, facing);
                                if (dot > 1.0F && dot / Length(between) >= Fixed::ToFloat(4074))
                                {
                                    const float angle = RadiansToDegrees(std::atan2(3.0F, dot));
                                    if (angle < zoomFov)
                                    {
                                        zoomFov = angle;
                                    }
                                }
                            }
                        };
                        checkZoomTargets(EntityType::Player);
                        checkZoomTargets(EntityType::EnemyInstance);
                        checkZoomTargets(EntityType::Object);
                        zoomFov *= 2.0F;
                        float currentFov = _cameraInfo->Fov;
                        if (zoomFov > currentFov)
                        {
                            currentFov += 4.0F;
                            if (currentFov > zoomFov) currentFov = zoomFov;
                        }
                        else if (zoomFov < currentFov)
                        {
                            currentFov -= 4.0F;
                            if (currentFov < zoomFov) currentFov = zoomFov;
                        }
                        _cameraInfo->Fov = currentFov;
                    }
                }

                if ((_controls.Shoot().IsPressed() && _equipInfo->ChargeLevel <= 2)
                    || (TestFlag(equipWeapon.Flags, WeaponFlags::RepeatFire)
                        && TestFlag(_flags2, PlayerFlags2::Shooting)
                        && (!TestFlag(equipWeapon.Flags, WeaponFlags::CanCharge)
                            || _equipInfo->ChargeLevel < equipWeapon.MinCharge * 2)))
                {
                    if (TryFireWeapon())
                    {
                        anim2 = PlayerAnimation::Shoot;
                        animFlags2 |= AnimFlags::NoLoop;
                        if (Biped2Anim() == PlayerAnimation::Shoot)
                        {
                            SetBiped2Animation(PlayerAnimation::Shoot, Biped2Flags());
                        }
                    }
                }

                if ((!TestFlag(_flags2, PlayerFlags2::BipedStuck)
                        && TestFlag(_abilities, AbilityFlags::AltForm) && _controls.Morph().IsPressed())
                    || (IsMainPlayer() && CameraSequence::Current != nullptr
                        && CameraSequence::Current->ForceAlt))
                {
                    if (TrySwitchForms() && IsMainPlayer() && IsMorphing())
                    {
                        HudOnMorphStart();
                    }
                    anim1 = PlayerAnimation::Morph;
                    anim2 = PlayerAnimation::Morph;
                }
            }

            const float magBefore = std::sqrt(_speed.X * _speed.X + _speed.Z * _speed.Z);
            _speed = Add(_speed, speedDelta);
            const float magAfter = std::sqrt(_speed.X * _speed.X + _speed.Z * _speed.Z);
            if (magAfter > magBefore && magAfter > _hSpeedCap)
            {
                const float factor = magBefore <= _hSpeedCap ? _hSpeedCap / magAfter : magBefore / magAfter;
                _speed = WithZ(WithX(_speed, _speed.X * factor), _speed.Z * factor);
            }
            if (_equipInfo->Zoomed)
            {
                _facingVector = Add(_facingVector, Multiply(Subtract(_gunVec1, _facingVector), 0.3F / 2.0F)).Normalized();
            }
            if (anim1 == PlayerAnimation::None)
            {
                if (TestFlag(_flags1, PlayerFlags1::Grounded))
                {
                    if (Biped1Anim() == PlayerAnimation::Idle)
                    {
                        ++_timeIdle;
                        if (_timeIdle > 600 && _timeSinceInput > 600)
                        {
                            SetBiped1Animation(PlayerAnimation::Flourish, AnimFlags::NoLoop);
                        }
                    }
                    else if (!TestFlag(Biped1Flags(), AnimFlags::NoLoop)
                        || TestFlag(Biped1Flags(), AnimFlags::Ended))
                    {
                        _timeIdle = 0;
                        SetBiped1Animation(PlayerAnimation::Idle, AnimFlags::None);
                    }
                }
            }
            else if (anim1 != Biped1Anim() || anim1 == PlayerAnimation::JumpForward
                || anim1 == PlayerAnimation::JumpBack || anim1 == PlayerAnimation::JumpLeft
                || anim1 == PlayerAnimation::JumpRight || anim1 == PlayerAnimation::JumpNeutral)
            {
                SetBiped1Animation(anim1, animFlags1);
            }
            if (anim2 == PlayerAnimation::None)
            {
                if ((!TestFlag(Biped2Flags(), AnimFlags::NoLoop) || TestFlag(Biped2Flags(), AnimFlags::Ended))
                    && Biped2Anim() != Biped1Anim())
                {
                    SetBiped2Animation(Biped1Anim(), Biped1Flags());
                    ManagedAt(RequireReference(RequireReference(_bipedModel2).AnimInfo->Frame), 0) = Biped1Frame();
                }
            }
            else if (anim2 != Biped2Anim())
            {
                SetBiped2Animation(anim2, animFlags2);
            }
        }
    }

    bool PlayerEntity::TryFireWeapon()
    {
        if (!TestFlag(_flags2, PlayerFlags2::Cloaking))
        {
            _cloakTimer = 0;
        }
        if (_attachedEnemy != nullptr)
        {
            return false;
        }
        const bool pressed = _controls.Shoot().IsPressed();
        const WeaponInfo& equipWeapon = EquipWeapon();
        if (pressed || _currentWeapon != BeamType::PowerBeam)
        {
            _autofireCooldown = static_cast<std::uint16_t>(equipWeapon.AutofireCooldown * 2);
            _powerBeamAutofire = 0;
        }
        else
        {
            if (_powerBeamAutofire < std::numeric_limits<std::uint16_t>::max())
            {
                ++_powerBeamAutofire;
            }
            std::int32_t pbAuto = std::min<std::int32_t>(_powerBeamAutofire / 2, 90);
            pbAuto = static_cast<std::int32_t>(pbAuto * 15 / 90.0F);
            _autofireCooldown = static_cast<std::uint16_t>((pbAuto + equipWeapon.AutofireCooldown) * 2);
        }
        if ((_timeSinceShot < equipWeapon.ShotCooldown * 2
                || (!pressed && _timeSinceShot < _autofireCooldown))
            && (!_isBot || !TestFlag(RequireReference(AiData).Flags2, AiFlags2::Bit20)))
        {
            return false;
        }
        if (_gunAnimation == GunAnimation::UpDown)
        {
            return false;
        }
        Vector3 shotOrigin = Mods::Network::NetHooks::RemoteShotOrigin(*this, _muzzlePos);
        Vector3 shotVec = Mods::Network::NetHooks::RemoteShotDirection(*this,
            Subtract(_aimPosition, _muzzlePos));
        if (!VectorEquals(shotOrigin, _muzzlePos))
        {
            shotVec = Mods::Network::NetHooks::RemoteShotDirection(*this, shotVec);
        }
        if (_disruptedTimer > 0)
        {
            shotVec.X += Fixed::ToFloat(static_cast<std::int32_t>(Rng::GetRandomInt2(24576)) - 12288);
            shotVec.Y += Fixed::ToFloat(static_cast<std::int32_t>(Rng::GetRandomInt2(24576)) - 12288);
            shotVec.Z += Fixed::ToFloat(static_cast<std::int32_t>(Rng::GetRandomInt2(24576)) - 12288);
        }
        shotVec = shotVec.Normalized();
        const WeaponInfo curWeapon = _equipInfo->Weapon;
        if (IsPrimeHunter())
        {
            _equipInfo->Weapon = ManagedAt(Weapons::Current, static_cast<std::int32_t>(_currentWeapon) + 9);
        }
        if (_isBot && GameState::SinglePlayer)
        {
            UpdateAdventureModeBotWeapon();
        }
        BeamSpawnFlags flags = BeamSpawnFlags::NoMuzzle;
        if (_doubleDmgTimer > 0)
        {
            flags |= BeamSpawnFlags::DoubleDamage;
        }
        else if (IsPrimeHunter())
        {
            flags |= BeamSpawnFlags::PrimeHunter;
        }
        Mods::Network::NetUnlagged::BeginShot(*this);
        const BeamResultFlags result = BeamProjectileEntity::Spawn(
            shared_from_this(), _equipInfo, shotOrigin, shotVec, flags, NodeRef, _scene);
        Mods::Network::NetUnlagged::EndShot(*this);
        Mods::Network::NetDamage::NoteFired(*this, shotVec, _gunVec1);
        if (result == BeamResultFlags::NoSpawn)
        {
            _equipInfo->Weapon = curWeapon;
            PlayBeamEmptySfx(_equipInfo->Weapon.Beam);
            return false;
        }
        _timeSinceShot = 0;
        if (IsMainPlayer())
        {
            HudOnFiredShot();
        }
        if (_currentWeapon == BeamType::Missile)
        {
            _flags1 |= PlayerFlags1::ShotMissile;
        }
        if (_equipInfo->ChargeLevel < _equipInfo->Weapon.MinCharge * 2)
        {
            _flags1 |= PlayerFlags1::ShotUncharged;
        }
        else
        {
            _flags1 |= PlayerFlags1::ShotCharged;
        }
        if (_muzzleEffect == nullptr || !TestFlag(_equipInfo->Weapon.Flags, WeaponFlags::Continuous))
        {
            if (_muzzleEffect != nullptr)
            {
                RequireReference(_scene).UnlinkEffectEntry(_muzzleEffect);
                _muzzleEffect.reset();
            }
            const std::int32_t effectId = Metadata::MuzzleEffectIds[BeamIndex(_currentWeapon)];
            _muzzleEffect = RequireReference(_scene).SpawnEffectGetEntry(effectId, _gunVec2, _gunVec1, _muzzlePos);
            if (_muzzleEffect != nullptr && !IsMainPlayer())
            {
                _muzzleEffect->SetDrawEnabled(false);
            }
        }
        const bool charged = TestFlag(_equipInfo->Weapon.Flags, WeaponFlags::PartialCharge)
            ? TestFlag(_flags1, PlayerFlags1::ShotCharged)
            : _equipInfo->ChargeLevel >= _equipInfo->Weapon.FullCharge * 2;
        const bool continuous = TestFlag(_equipInfo->Weapon.Flags, WeaponFlags::Continuous);
        const bool homing = TestFlag(result, BeamResultFlags::Homing);
        const float amountA = 0x3FFF * _shockCoilTimer / (30.0F * 2.0F);
        PlayBeamShotSfx(_equipInfo->Weapon.Beam, charged, continuous, homing, amountA);
        if (_equipInfo->Weapon.Beam == BeamType::Imperialist
            && _equipInfo->Ammo >= _equipInfo->Weapon.AmmoCost)
        {
            _soundSource.PlaySfx(SfxId::SNIPER_RELOAD);
        }
        _equipInfo->Weapon = curWeapon;
        UnequipOmegaCannon();
        return true;
    }

    void PlayerEntity::ResetAdventureModeBotWeapon()
    {
        _equipInfo->DrawFuncIds[0] = 255;
        _equipInfo->DrawFuncIds[1] = 255;
        _equipInfo->DmgDirTypes[0] = 255;
        _equipInfo->DmgDirTypes[1] = 255;
        _equipInfo->UnchargedDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->HeadshotDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->MinChargeDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->ChargedDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->MinChargeHeadshotDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->ChargedHeadshotDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->SplashDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->MinChargeSplashDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->ChargedSplashDamage = std::numeric_limits<std::uint16_t>::max();
        _equipInfo->HomingTolerance = std::numeric_limits<std::int32_t>::max();
        _equipInfo->InfiniteAmmo = false;
    }

    void PlayerEntity::UpdateAdventureModeBotWeapon()
    {
        const std::int32_t encounter = ManagedAt(GameState::EncounterState, _slotIndex);
        if (encounter == 1 || encounter == 3 || encounter == 4)
        {
            if (_hunter == Hunter::Kanden)
            {
                _equipInfo->HomingTolerance = 4006;
            }
            else if (_hunter == Hunter::Spire || _hunter == Hunter::Weavel)
            {
                _equipInfo->DmgDirTypes[0] = 0;
                _equipInfo->DmgDirTypes[1] = 0;
            }
        }
        std::int32_t index;
        if (_hunter == Hunter::Guardian)
        {
            index = 0;
            if (_equipInfo->Weapon.Beam == BeamType::Magmaul)
            {
                _equipInfo->DrawFuncIds[0] = 22;
                _equipInfo->DrawFuncIds[1] = 22;
            }
        }
        else if (encounter == 1 || encounter == 3 || encounter == 4)
        {
            index = 1;
        }
        else if (BotLevel() == 0)
        {
            index = 2;
        }
        else if (BotLevel() == 1)
        {
            index = 3;
        }
        else
        {
            index = 4;
        }
        if (encounter == 3 && _hunter == Hunter::Trace)
        {
            _equipInfo->UnchargedDamage = 50;
            _equipInfo->HeadshotDamage = 50;
        }
        else if (_equipInfo->Weapon.Beam != BeamType::OmegaCannon)
        {
            const Weapons::BotWeaponValues values = ManagedAt(Weapons::BotWeapons, index)[BeamIndex(_equipInfo->Weapon.Beam)];
            _equipInfo->UnchargedDamage = values.UnchargedDamage;
            _equipInfo->HeadshotDamage = values.UnchargedDamage;
            _equipInfo->MinChargeDamage = values.ChargedDamage;
            _equipInfo->ChargedDamage = values.ChargedDamage;
            _equipInfo->MinChargeHeadshotDamage = values.ChargedDamage;
            _equipInfo->ChargedHeadshotDamage = values.ChargedDamage;
            _equipInfo->SplashDamage = values.SplashDamage;
            _equipInfo->MinChargeSplashDamage = values.ChargedSplashDamage;
            _equipInfo->ChargedSplashDamage = values.ChargedSplashDamage;
        }
        _equipInfo->InfiniteAmmo = true;
    }

    void PlayerEntity::ProcessAlt()
    {
        Vector3 speedDelta{};
        std::int32_t animId = -1;
        AnimFlags animFlags = AnimFlags::None;
        _flags1 |= PlayerFlags1::UsedJump;
        if (_frozenTimer == 0 && _health > 0)
        {
            if ((!_controls.RollRight().IsDown() && !_controls.RolltLeft().IsDown()
                    && !_controls.RollUp().IsDown() && !_controls.RollDown().IsDown())
                || _controls.RollRight().IsPressed() || _controls.RolltLeft().IsPressed()
                || _controls.RollUp().IsPressed() || _controls.RollDown().IsPressed())
            {
                _flags1 &= ~PlayerFlags1::AltDirOverride;
            }
            if (_timeSinceMorphCamera > 20 && !TestFlag(_flags1, PlayerFlags1::AltDirOverride)
                && (std::fabs(_cameraInfo->Field48) >= 1.0F / 4096.0F
                    || std::fabs(_cameraInfo->Field4C) >= 1.0F / 4096.0F))
            {
                _altRollFbX = _cameraInfo->Field48;
                _altRollFbZ = _cameraInfo->Field4C;
                _altRollLrX = _cameraInfo->Field50;
                _altRollLrZ = _cameraInfo->Field54;
            }

            const auto updateAnimation = [&](float aimX, float /*aimY*/)
            {
                if ((_hunter == Hunter::Trace || _hunter == Hunter::Weavel)
                    && TestFlag(_flags1, PlayerFlags1::Grounded))
                {
                    if (aimX > 3.0F)
                    {
                        _timeIdle = 0;
                        animId = static_cast<std::int32_t>(WeavelAltAnim::Turn);
                        animFlags = AnimFlags::Reverse;
                        if (ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Index), 0) == animId)
                        {
                            ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Flags), 0) &= ~AnimFlags::NoLoop;
                            ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Flags), 0) |= AnimFlags::Reverse;
                        }
                    }
                    else if (aimX < -3.0F)
                    {
                        _timeIdle = 0;
                        animId = static_cast<std::int32_t>(WeavelAltAnim::Turn);
                        if (ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Index), 0) == animId)
                        {
                            ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Flags), 0) &= ~AnimFlags::NoLoop;
                            ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Flags), 0) &= ~AnimFlags::Reverse;
                        }
                    }
                }
            };

            if (_values.AltFormStrafe != 0)
            {
                ApplyModAim();
                if (_controls.MouseAim() && !TestFlag(_flags1, PlayerFlags1::NoAimInput) && !_isBot)
                {
                    float aimY = -_input.MouseDeltaY() / 4.0F * Mods::InputSettings::MouseSensitivity()
                        * (Mods::InputSettings::InvertMouseY() ? -1.0F : 1.0F);
                    float aimX = -_input.MouseDeltaX() / 4.0F * Mods::InputSettings::MouseSensitivity()
                        * (Mods::InputSettings::InvertMouseX() ? -1.0F : 1.0F);
                    if ((CameraSequence::Current != nullptr
                            && TestFlag(CameraSequence::Current->Flags, CamSeqFlags::BlockInput))
                        || RequireReference(_scene).FrameAdvance || RequireReference(_scene).FrameAdvanceLastFrame)
                    {
                        aimX = aimY = 0.0F;
                    }
                    if (aimX != 0.0F || aimY != 0.0F)
                    {
                        _input.HasInput = true;
                    }
                    UpdateHudShiftY(aimY);
                    UpdateHudShiftX(aimX);
                    UpdateAimY(aimY);
                    UpdateAimX(aimX);
                    updateAnimation(aimX, aimY);
                }
                if (_controls.KeyboardAim() || _isBot)
                {
                    UpdateAimX(_buttonAimX);
                    UpdateAimY(_buttonAimY);
                    updateAnimation(_buttonAimX, _buttonAimY);
                }
                if (!TestFlag(_flags2, PlayerFlags2::BipedLock)
                    && (_hunter != Hunter::Trace || !TestFlag(_flags2, PlayerFlags2::AltAttack)))
                {
                    const auto moveRightLeft = [&](std::int32_t walkAnim, std::int32_t sign)
                    {
                        _flags1 |= PlayerFlags1::Strafing;
                        _flags1 |= PlayerFlags1::MovingBiped;
                        if (TestFlag(_flags1, PlayerFlags1::Standing)) _flags1 |= PlayerFlags1::Walking;
                        else _flags1 &= ~PlayerFlags1::Walking;
                        float traction = Fixed::ToFloat(_values.StrafeBipedTraction);
                        if (_jumpPadControlLockMin > 0)
                        {
                            traction *= Fixed::ToFloat(_values.JumpPadSlideFactor);
                        }
                        speedDelta.X -= _field78 * traction * static_cast<float>(sign);
                        speedDelta.Z -= _field7C * traction * static_cast<float>(sign);
                        if (_hunter == Hunter::Trace || _hunter == Hunter::Weavel)
                        {
                            animId = walkAnim;
                            animFlags = AnimFlags::None;
                        }
                    };
                    const auto moveForwardBack = [&](std::int32_t walkAnim, std::int32_t sign)
                    {
                        _flags1 |= PlayerFlags1::MovingBiped;
                        if (TestFlag(_flags1, PlayerFlags1::Standing)) _flags1 |= PlayerFlags1::Walking;
                        else _flags1 &= ~PlayerFlags1::Walking;
                        float traction = Fixed::ToFloat(_values.WalkBipedTraction);
                        if (_jumpPadControlLockMin > 0)
                        {
                            traction *= Fixed::ToFloat(_values.JumpPadSlideFactor);
                        }
                        else if (TestFlag(_flags1, PlayerFlags1::Standing) && _slipperiness != 0)
                        {
                            traction *= ManagedAt(Metadata::TractionFactors, _slipperiness);
                        }
                        speedDelta.X += _field70 * traction * static_cast<float>(sign);
                        speedDelta.Z += _field74 * traction * static_cast<float>(sign);
                        if (_hunter == Hunter::Trace || _hunter == Hunter::Weavel)
                        {
                            animId = walkAnim;
                            animFlags = AnimFlags::None;
                        }
                    };
                    if (_controls.MoveRight().IsDown()) moveRightLeft(4, 1);
                    else if (_controls.MoveLeft().IsDown()) moveRightLeft(2, -1);
                    if (_controls.MoveUp().IsDown()) moveForwardBack(3, 1);
                    else if (_controls.MoveDown().IsDown()) moveForwardBack(5, -1);
                }
            }
            else
            {
                float traction = Fixed::ToFloat(_values.RollAltTraction);
                if (_jumpPadControlLockMin > 0)
                {
                    traction *= Fixed::ToFloat(_values.JumpPadSlideFactor);
                }
                if (_controls.RollUp().IsDown())
                {
                    speedDelta.X += _altRollFbX * traction;
                    speedDelta.Z += _altRollFbZ * traction;
                }
                else if (_controls.RollDown().IsDown())
                {
                    speedDelta.X -= _altRollFbX * traction;
                    speedDelta.Z -= _altRollFbZ * traction;
                }
                if (_controls.RolltLeft().IsDown())
                {
                    speedDelta.X += _altRollLrX * traction;
                    speedDelta.Z += _altRollLrZ * traction;
                }
                else if (_controls.RollRight().IsDown())
                {
                    speedDelta.X -= _altRollLrX * traction;
                    speedDelta.Z -= _altRollLrZ * traction;
                }
            }

            if (!IsMorphing())
            {
                if (TestFlag(_abilities, AbilityFlags::Bombs) && _controls.AltAttack().IsPressed()
                    && _bombAmmo > 0 && _bombCooldown == 0 && _field35C == nullptr)
                {
                    SpawnBomb();
                }
                if (TestFlag(_abilities, AbilityFlags::NoxusAltAttack))
                {
                    if (_controls.AltAttack().IsDown())
                    {
                        if (_controls.AltAttack().IsPressed())
                        {
                            _altAttackTime = 1;
                            RequireReference(_altModel).SetAnimation(static_cast<std::int32_t>(NoxusAltAnim::Extend), AnimFlags::NoLoop);
                        }
                        else if (_altAttackTime > 0)
                        {
                            ++_altAttackTime;
                            if (_altAttackTime == 14)
                            {
                                _soundSource.PlaySfx(SfxId::NOX_TOP_ATTACK1);
                            }
                            else
                            {
                                const std::int32_t startupTime = _values.AltAttackStartup * 2;
                                if (_altAttackTime == startupTime / 2)
                                {
                                    _soundSource.PlaySfx(SfxId::NOX_TOP_ATTACK2, true);
                                }
                                else if (_altAttackTime >= startupTime)
                                {
                                    _altAttackTime = static_cast<std::uint16_t>(startupTime);
                                    _flags2 |= PlayerFlags2::AltAttack;
                                }
                            }
                        }
                        ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Frame), 0) = (_altAttackTime / 2
                            * ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->FrameCount), 0) - 1) / _values.AltAttackStartup;
                    }
                    else
                    {
                        EndAltAttack();
                    }
                }
                if (TestFlag(_abilities, AbilityFlags::SpireAltAttack))
                {
                    if (TestFlag(_flags2, PlayerFlags2::AltAttack))
                    {
                        if (TestFlag(ManagedAt(RequireReference(RequireReference(_altModel).AnimInfo->Flags), 0), AnimFlags::Ended))
                        {
                            EndAltAttack();
                        }
                    }
                    else if (_controls.AltAttack().IsPressed())
                    {
                        _flags2 |= PlayerFlags2::AltAttack;
                        RequireReference(_altModel).SetAnimation(static_cast<std::int32_t>(SpireAltAnim::Attack), AnimFlags::NoLoop);
                        _soundSource.PlaySfx(SfxId::SPIRE_ALT_ATTACK);
                        _spireRockPosR = static_cast<Vector3>(Position);
                        _spireRockPosL = static_cast<Vector3>(Position);
                        _spireAltUp = _fieldC0;
                        const Vector3 cross = Vector3::Cross(_facingVector, _spireAltUp);
                        _spireAltFacing = Vector3::Cross(_spireAltUp, cross).Normalized();
                    }
                }
                if (TestFlag(_abilities, AbilityFlags::TraceAltAttack))
                {
                    if (TestFlag(_flags2, PlayerFlags2::AltAttack) || _altAttackCooldown > 0)
                    {
                        if (TestFlag(_flags1, PlayerFlags1::Standing)) EndAltAttack();
                    }
                    else if (_controls.AltAttack().IsPressed())
                    {
                        _flags2 |= PlayerFlags2::AltAttack;
                        const float attackHSpeed = Fixed::ToFloat(_values.LungeHSpeed);
                        const float attackVSpeed = Fixed::ToFloat(_values.LungeVSpeed);
                        const float accelX = _field70 * attackHSpeed;
                        const float accelZ = _field74 * attackHSpeed;
                        if (_field70 * _speed.X + _field74 * _speed.Z < attackHSpeed)
                        {
                            _speed = WithZ(WithX(_speed, accelX), accelZ);
                        }
                        _accelerationTimer = 12;
                        _acceleration = Vector3(accelX, 0.0F, accelZ);
                        if (_speed.Y < attackVSpeed)
                        {
                            float newYSpeed = _speed.Y + attackVSpeed;
                            if (newYSpeed > attackVSpeed) newYSpeed = attackVSpeed;
                            _speed = WithY(_speed, newYSpeed);
                        }
                        animId = static_cast<std::int32_t>(TraceAltAnim::Attack);
                        animFlags = AnimFlags::NoLoop;
                        _soundSource.PlaySfx(SfxId::TRACE_ALT_ATTACK);
                    }
                }
                if (TestFlag(_abilities, AbilityFlags::WeavelAltAttack))
                {
                    if (TestFlag(_flags2, PlayerFlags2::AltAttack) || _altAttackCooldown > 0)
                    {
                        if (TestFlag(_flags1, PlayerFlags1::Standing)) EndAltAttack();
                    }
                    else if (_controls.AltAttack().IsPressed())
                    {
                        _flags2 |= PlayerFlags2::AltAttack;
                        float attackHSpeed = Fixed::ToFloat(_values.LungeHSpeed);
                        float attackVSpeed = Fixed::ToFloat(_values.LungeVSpeed);
                        if (_isBot && GameState::SinglePlayer
                            && ManagedAt(GameState::EncounterState, _slotIndex) == 1)
                        {
                            attackHSpeed = 0.3F;
                            attackVSpeed = 0.45F;
                        }
                        if (_field70 * _speed.X + _field74 * _speed.Z < attackHSpeed)
                        {
                            _speed = WithZ(WithX(_speed, _field70 * attackHSpeed), _field74 * attackHSpeed);
                        }
                        if (_speed.Y < attackVSpeed)
                        {
                            float newYSpeed = _speed.Y + attackVSpeed;
                            if (newYSpeed > attackVSpeed) newYSpeed = attackVSpeed;
                            _speed = WithY(_speed, newYSpeed);
                        }
                        animId = static_cast<std::int32_t>(WeavelAltAnim::Attack);
                        animFlags = AnimFlags::NoLoop;
                        _soundSource.PlaySfx(SfxId::WEAVEL_ALT_ATTACK);
                    }
                }
                if (TestFlag(_abilities, AbilityFlags::Boost) && _attachedEnemy == nullptr)
                {
                    const bool swipeBoost = _swipeBoostRequested;
                    _swipeBoostRequested = false;
                    float boostDirX = _field70;
                    float boostDirZ = _field74;
                    bool boostAimed = false;
                    if (swipeBoost && (_swipeBoostX != 0.0F || _swipeBoostY != 0.0F))
                    {
                        const float forward = -_swipeBoostY;
                        const float left = -_swipeBoostX;
                        const float dirX = _altRollFbX * forward + _altRollLrX * left;
                        const float dirZ = _altRollFbZ * forward + _altRollLrZ * left;
                        const float dirMag = std::sqrt(dirX * dirX + dirZ * dirZ);
                        if (dirMag > 1.0F / 4096.0F)
                        {
                            boostDirX = dirX / dirMag;
                            boostDirZ = dirZ / dirMag;
                            boostAimed = true;
                        }
                    }
                    _swipeBoostX = 0.0F;
                    _swipeBoostY = 0.0F;
                    if (_controls.Boost().IsDown() && !swipeBoost)
                    {
                        if (_boostCharge < _values.BoostChargeMax * 2)
                        {
                            ++_boostCharge;
                        }
                    }
                    else
                    {
                        if (swipeBoost)
                        {
                            _boostCharge = static_cast<std::uint16_t>(_values.BoostChargeMax * 2);
                        }
                        if (_boostCharge > _values.BoostChargeMin * 2)
                        {
                            if (Features::FullBoostCharge)
                            {
                                _boostCharge = static_cast<std::uint16_t>(_values.BoostChargeMax * 2);
                            }
                            if (_boostCharge > 0)
                            {
                                const auto hunterSfx = Metadata::HunterSfx();
                                const auto& hunterSounds = ManagedAt(
                                    RequireReference(hunterSfx), static_cast<std::int32_t>(_hunter));
                                const std::int32_t sfx = ManagedAt(
                                    hunterSounds, static_cast<std::int32_t>(HunterSfx::Boost));
                                _soundSource.PlaySfx(sfx);
                            }
                            const float boostHCap = Fixed::ToFloat(_values.BoostSpeedCap) * _boostCharge
                                / static_cast<float>(_values.BoostChargeMax * 2);
                            if (_hSpeedCap < boostHCap) _hSpeedCap = boostHCap;
                            const float factor = Fixed::ToFloat(_values.BoostSpeedMin)
                                + _boostCharge * (Fixed::ToFloat(_values.BoostSpeedMax)
                                    - Fixed::ToFloat(_values.BoostSpeedMin))
                                / static_cast<float>(_values.BoostChargeMax * 2);
                            speedDelta = AddZ(AddX(speedDelta, boostDirX * factor), boostDirZ * factor);
                            _altAttackCooldown = static_cast<std::uint16_t>(_values.AltAttackCooldown * 2);
                            _flags1 |= PlayerFlags1::Boosting;
                            _boostDamage = static_cast<std::uint16_t>(
                                _values.AltAttackDamage * _boostCharge / (_values.BoostChargeMax * 2));
                            if (IsMainPlayer())
                            {
                                RequireReference(_boostInst).SetAnimation(0, 10, 11, 0);
                            }
                            if (_boostEffect != nullptr)
                            {
                                RequireReference(_scene).UnlinkEffectEntry(_boostEffect);
                                _boostEffect.reset();
                            }
                            const Vector3 boostVec1 = boostAimed
                                ? Vector3(boostDirZ, 0.0F, -boostDirX) : _gunVec2;
                            const Vector3 boostVec2 = boostAimed
                                ? Vector3(boostDirX, 0.0F, boostDirZ) : _facingVector;
                            _boostEffect = RequireReference(_scene).SpawnEffectGetEntry(136, boostVec1, boostVec2, static_cast<Vector3>(Position));
                            if (_boostEffect != nullptr)
                            {
                                _boostEffect->SetElementExtension(true);
                            }
                        }
                        _boostCharge = 0;
                    }
                }
            }

            const float magBefore = std::sqrt(_speed.X * _speed.X + _speed.Z * _speed.Z);
            _speed = Add(_speed, speedDelta);
            const float magAfter = std::sqrt(_speed.X * _speed.X + _speed.Z * _speed.Z);
            if (magAfter > magBefore && magAfter > _hSpeedCap)
            {
                const float factor = magBefore <= _hSpeedCap ? _hSpeedCap / magAfter : magBefore / magAfter;
                _speed = WithZ(WithX(_speed, _speed.X * factor), _speed.Z * factor);
            }
            if (_field35C != nullptr)
            {
                _speed = WithZ(WithX(_speed, 0.0F), 0.0F);
            }
            else if (_attachedEnemy != nullptr)
            {
                _speed = WithZ(WithX(_speed, _speed.X / 2.0F), _speed.Z / 2.0F);
            }
            if ((TestFlag(_abilities, AbilityFlags::AltForm) && _controls.Morph().IsPressed())
                || (IsMainPlayer() && CameraSequence::Current != nullptr
                    && CameraSequence::Current->ForceBiped))
            {
                (void)TrySwitchForms();
            }
            if (_hunter == Hunter::Trace || _hunter == Hunter::Weavel)
            {
                AnimationInfo& info = RequireReference(RequireReference(_altModel).AnimInfo);
                const std::int32_t infoIndex = ManagedAt(RequireReference(info.Index), 0);
                const AnimFlags infoFlags = ManagedAt(RequireReference(info.Flags), 0);
                if (animId != -1)
                {
                    if ((infoIndex != 1 || TestFlag(infoFlags, AnimFlags::Ended))
                        && animId != infoIndex)
                    {
                        RequireReference(_altModel).SetAnimation(animId, animFlags);
                    }
                }
                else if (infoIndex != 0
                    && (!TestFlag(infoFlags, AnimFlags::NoLoop) || TestFlag(infoFlags, AnimFlags::Ended)))
                {
                    RequireReference(_altModel).SetAnimation(0);
                }
            }
        }
        ProcessMovement();
        Mods::Network::NetHooks::AfterRemoteMovement(*this);
        UpdateCamera();
    }

    void PlayerEntity::SpawnBomb()
    {
        ++Mods::Network::NetDamage::BombSpawnCalls;
        Matrix4 transform = IdentityMatrix();
        if (_hunter == Hunter::Kanden)
        {
            const Matrix4 segMtx = _kandenSegMtx[4];
            transform = GetTransformMatrix(segMtx.Row2().Xyz(), segMtx.Row1().Xyz(), _kandenSegPos[4]);
        }
        else
        {
            if (_hunter == Hunter::Sylux && _syluxBombCount >= 3)
            {
                bool detonated = false;
                for (std::int32_t i = static_cast<std::int32_t>(_syluxBombs.size()) - 1; i >= 0; --i)
                {
                    const auto& placed = _syluxBombs[static_cast<std::size_t>(i)];
                    if (placed != nullptr)
                    {
                        placed->SetCountdown(0);
                        detonated = true;
                    }
                }
                if (detonated)
                {
                    ++Mods::Network::NetDamage::BombSpawnDetonated;
                    return;
                }
                ++Mods::Network::NetDamage::BombSpawnStaleCount;
                _syluxBombCount = 0;
            }
            transform = GetTransformMatrix(Vector3(0.0F, 0.0F, 1.0F),
                Vector3(0.0F, 1.0F, 0.0F), AddY(Position, Fixed::ToFloat(-1000)));
        }
        const auto bomb = BombEntity::Spawn(this, transform, _scene);
        if (bomb == nullptr)
        {
            ++Mods::Network::NetDamage::BombSpawnPoolEmpty;
        }
        if (bomb != nullptr)
        {
            ++Mods::Network::NetDamage::BombSpawnMade;
            if (_hunter == Hunter::Sylux)
            {
                _syluxBombs.at(_syluxBombCount) = bomb;
                bomb->SetBombIndex(_syluxBombCount++);
            }
            bomb->NodeRef = NodeRef;
            bomb->SetRadius(Fixed::ToFloat(_values.BombRadius));
            bomb->SetSelfRadius(Fixed::ToFloat(_values.BombSelfRadius));
            bomb->SetDamage(static_cast<std::uint16_t>(_values.BombDamage));
            bomb->SetEnemyDamage(static_cast<std::uint16_t>(_values.BombEnemyDamage));
            if (_isBot && GameState::SinglePlayer
                && (_hunter == Hunter::Kanden || _hunter == Hunter::Sylux))
            {
                const std::int32_t encounter = ManagedAt(GameState::EncounterState, _slotIndex);
                std::uint16_t damage;
                if (encounter == 1 || encounter == 3 || encounter == 4
                    || (encounter == 0 && BotLevel() == 0))
                {
                    damage = static_cast<std::uint16_t>(_hunter == Hunter::Kanden ? 2 : 6);
                }
                else if (encounter != 0 || BotLevel() < 2)
                {
                    damage = static_cast<std::uint16_t>(_hunter == Hunter::Kanden ? 4 : 3);
                }
                else
                {
                    damage = static_cast<std::uint16_t>(_hunter == Hunter::Kanden ? 8 : 6);
                }
                bomb->SetDamage(damage);
                bomb->SetEnemyDamage(damage);
            }
            if (_doubleDmgTimer > 0)
            {
                bomb->SetDamage(static_cast<std::uint16_t>(bomb->Damage() * 2));
                bomb->SetEnemyDamage(static_cast<std::uint16_t>(bomb->EnemyDamage() * 2));
            }
            if (_bombAmmo >= 2)
            {
                _bombRefillTimer = static_cast<std::uint16_t>(_values.BombRefillTime * 2);
            }
            --_bombAmmo;
            _bombCooldown = static_cast<std::uint16_t>(_values.BombCooldown * 2);
            if (_hunter == Hunter::Kanden)
            {
                RequireReference(_altModel).SetAnimation(static_cast<std::int32_t>(KandenAltAnim::TailOut), AnimFlags::NoLoop);
            }
            else if (_hunter == Hunter::Sylux && _syluxBombCount == 3)
            {
                _bombOveruse += 54;
                if (_bombOveruse >= 200)
                {
                    _bombCooldown = 300;
                }
            }
            bomb->PlaySpawnSfx();
        }
    }

    void PlayerEntity::EndAltAttack()
    {
        if (_hunter == Hunter::Samus)
        {
            _flags1 &= ~PlayerFlags1::Boosting;
        }
        else if (_hunter == Hunter::Trace || _hunter == Hunter::Weavel)
        {
            if (TestFlag(_flags2, PlayerFlags2::AltAttack))
            {
                if (_isBot && GameState::SinglePlayer
                    && ManagedAt(GameState::EncounterState, _slotIndex) == 1)
                {
                    _altAttackCooldown = 20;
                }
                else
                {
                    _altAttackCooldown = static_cast<std::uint16_t>(_values.AltAttackCooldown * 2);
                }
            }
        }
        else if (_hunter == Hunter::Noxus)
        {
            if (_altAttackTime > 0)
            {
                _soundSource.StopSfx(SfxId::NOX_TOP_ATTACK1);
                _soundSource.StopSfx(SfxId::NOX_TOP_ATTACK2);
                if (_altAttackTime >= _values.AltAttackStartup / 2 * 2)
                {
                    _soundSource.PlaySfx(SfxId::NOX_TOP_ATTACK3);
                }
                RequireReference(_altModel).SetAnimation(static_cast<std::int32_t>(NoxusAltAnim::Extend), AnimFlags::Paused);
                _altAttackTime = 0;
            }
        }
        _flags2 &= ~PlayerFlags2::AltAttack;
    }

    void PlayerEntity::ProcessMovement()
    {
        if (_accelerationTimer > 0)
        {
            --_accelerationTimer;
            _speed = Add(_speed, Divide(_acceleration, 2.0F));
        }
        Vector3 hSpeed(_speed.X, 0.0F, _speed.Z);
        const float hSpeedMag = Length(hSpeed);
        if (hSpeedMag == 0.0F)
        {
            _hSpeedMag = 0.0F;
        }
        else
        {
            hSpeed = Divide(hSpeed, hSpeedMag);
            if (_values.AltFormStrafe == 0)
            {
                if (hSpeedMag > Fixed::ToFloat(_values.Field5C))
                {
                    _field80 = hSpeed.X;
                    _field84 = hSpeed.Z;
                }
                if ((IsAltForm() || IsMorphing()) && hSpeedMag > Fixed::ToFloat(_values.Field58))
                {
                    _field70 = hSpeed.X;
                    _field74 = hSpeed.Z;
                    _facingVector = Vector3(_field70, 0.0F, _field74);
                    _gunVec1 = _facingVector;
                    const float add = _gunVec1.X * Fixed::ToFloat(_values.AimDistance);
                    _aimPosition = AddZ(AddX(Position, add), add);
                }
            }
            if (IsAltForm())
            {
                const float altMin = Fixed::ToFloat(_values.AltMinHSpeed);
                if (_hSpeedCap <= altMin)
                {
                    _hSpeedCap = altMin;
                }
                else if (hSpeedMag >= _hSpeedCap)
                {
                    _hSpeedCap -= Fixed::ToFloat(_values.AltHSpeedCapIncrement) / 2.0F;
                }
                else
                {
                    _hSpeedCap = hSpeedMag;
                }
            }
            else
            {
                const bool strafing = TestFlag(_flags1, PlayerFlags1::Strafing);
                _hSpeedCap = Fixed::ToFloat(strafing ? _values.StrafeSpeedCap : _values.WalkSpeedCap);
            }
            if (IsPrimeHunter() && !IsAltForm())
            {
                _hSpeedCap = 0.4F;
            }
            _hSpeedMag = hSpeedMag;
        }

        float hMag = std::sqrt(_facingVector.X * _facingVector.X + _facingVector.Z * _facingVector.Z);
        _field70 = _facingVector.X / hMag;
        _field74 = _facingVector.Z / hMag;
        _gunVec2 = Vector3(_field74, 0.0F, -_field70);
        _field78 = _gunVec2.X;
        _field7C = _gunVec2.Z;
        _upVector = Vector3::Cross(_facingVector, _gunVec2).Normalized();
        if (_values.AltFormStrafe != 0)
        {
            _field80 = _field70;
            _field84 = _field74;
        }
        _aimPosition = Add(Multiply(_gunVec1, Fixed::ToFloat(_values.AimDistance)), _cameraInfo->Position);
        hMag = std::sqrt(_gunVec1.X * _gunVec1.X + _gunVec1.Z * _gunVec1.Z);
        _aimY = RadiansToDegrees(std::atan2(_gunVec1.Y, hMag));
        if (_aimY > 75.0F || _aimY < -75.0F)
        {
            UpdateAimY(0.0F);
        }
        if (TestFlag(_flags1, PlayerFlags1::UsedJumpPad))
        {
            const float prevX = _speed.X;
            _speed.X -= _jumpPadAccel.X;
            if ((prevX <= 0.0F && _speed.X > 0.0F) || (prevX > 0.0F && _speed.X < 0.0F))
            {
                _jumpPadAccel.X += _speed.X / 2.0F;
                _speed.X = 0.0F;
            }
            const float prevZ = _speed.Z;
            _speed.Z -= _jumpPadAccel.Z;
            if ((prevZ <= 0.0F && _speed.Z > 0.0F) || (prevZ > 0.0F && _speed.Z < 0.0F))
            {
                _jumpPadAccel.Z += _speed.Z / 2.0F;
                _speed.Z = 0.0F;
            }
        }
        float slideSfxAmount = 0.0F;
        float speedFactor;
        if (IsAltForm() || IsMorphing())
        {
            if (TestFlag(_flags2, PlayerFlags2::AltAttack)
                && (_hunter == Hunter::Trace || _hunter == Hunter::Weavel))
            {
                speedFactor = 0.96F;
            }
            else if (TestFlag(_flags1, PlayerFlags1::Standing))
            {
                speedFactor = Fixed::ToFloat(_values.AltGroundSpeedFactor);
            }
            else
            {
                speedFactor = Fixed::ToFloat(_values.AirSpeedFactor);
            }
        }
        else if (TestFlag(_flags1, PlayerFlags1::Standing))
        {
            if (TestFlag(_flags1, PlayerFlags1::Strafing))
            {
                speedFactor = Fixed::ToFloat(_values.StrafeSpeedFactor);
            }
            else if (TestFlag(_flags1, PlayerFlags1::Walking))
            {
                speedFactor = Fixed::ToFloat(_values.WalkSpeedFactor);
            }
            else
            {
                speedFactor = Fixed::ToFloat(_values.StandSpeedFactor);
            }
        }
        else
        {
            speedFactor = Fixed::ToFloat(_values.AirSpeedFactor);
        }
        if (TestFlag(_flags1, PlayerFlags1::Standing) && _slipperiness != 0)
        {
            speedFactor += (1.0F - speedFactor)
                * ManagedAt(Metadata::SlipSpeedFactors, _slipperiness);
            if (!TestFlag(_flags1, PlayerFlags1::MovingBiped))
            {
                slideSfxAmount = 0xFFFF * _hSpeedMag / Fixed::ToFloat(_values.WalkSpeedCap);
            }
        }
        UpdateSlidingSfx(slideSfxAmount);
        const Vector3 speedMul(_speed.X * speedFactor, _speed.Y, _speed.Z * speedFactor);
        _speed = Add(_speed, Divide(Subtract(speedMul, _speed), 2.0F));
        if (TestFlag(_flags1, PlayerFlags1::UsedJumpPad))
        {
            _speed.X += _jumpPadAccel.X;
            _speed.Z += _jumpPadAccel.Z;
        }
        if (TestFlag(_flags1, PlayerFlags1::Standing) && _timeSinceJumpPad > 10)
        {
            _lastJumpPad = nullptr;
            _flags1 &= ~PlayerFlags1::UsedJumpPad;
            _jumpPadControlLock = 0;
            _jumpPadControlLockMin = 0;
        }
        if (IsAltForm())
        {
            _flags2 |= PlayerFlags2::AltFormGravity;
        }
        else if (_speed.Y <= 0.01F)
        {
            _flags2 &= ~PlayerFlags2::AltFormGravity;
        }
        if (_health > 0)
        {
            if (_jumpPadControlLock == 0 && !TestFlag(_flags2, PlayerFlags2::BipedStuck))
            {
                if (TestFlag(_flags2, PlayerFlags2::GravityOverride))
                {
                    _flags2 &= ~PlayerFlags2::GravityOverride;
                }
                else
                {
                    if (IsAltForm() || TestFlag(_flags2, PlayerFlags2::AltFormGravity))
                    {
                        if (TestFlag(_flags1, PlayerFlags1::Standing) && _slipperiness == 0
                            && _values.AltFormStrafe != 0)
                        {
                            _gravity = 0.0F;
                        }
                        else if (TestFlag(_flags1, PlayerFlags1::Standing))
                        {
                            _gravity = Fixed::ToFloat(_values.AltGroundGravity);
                        }
                        else
                        {
                            _gravity = Fixed::ToFloat(_values.AltAirGravity);
                        }
                    }
                    else if (TestFlag(_flags1, PlayerFlags1::Standing) && _slipperiness == 0)
                    {
                        _gravity = 0.0F;
                    }
                    else
                    {
                        _gravity = Fixed::ToFloat(_values.BipedGravity);
                    }
                }
                _speed.Y += _gravity / 2.0F;
            }
            Vector3 position = Add(static_cast<Vector3>(Position), Divide(_speed, 2.0F));
            if (_attachedEnemy != nullptr && _attachedEnemy->EnemyType() == EnemyType::Quadtroid)
            {
                position.X = Position.X;
                position.Z = Position.Z;
            }
            Position = position;
            CheckPlayerCollision();
        }
        if (_hunter == Hunter::Kanden && IsAltForm() && TestFlag(_flags1, PlayerFlags1::Standing))
        {
            for (std::size_t i = 1; i < _kandenSegPos.size(); ++i)
            {
                _kandenSegPos[i].Y -= 0.1F / 2.0F;
            }
        }
        if (_standingEntCol != nullptr)
        {
            Vector3 position = Matrix::Vec3MultMtx4(static_cast<Vector3>(Position), _standingEntCol->Inverse2);
            Position = Matrix::Vec3MultMtx4(position, _standingEntCol->Transform);
        }
        if (!TestFlag(_flags1, PlayerFlags1::CollidingLateral))
        {
            _horizColTimer = 0;
        }
        else if (_horizColTimer != std::numeric_limits<std::uint16_t>::max())
        {
            ++_horizColTimer;
        }
        const Terrain prevTerrain = _standTerrain;
        const bool standingPrev = TestFlag(_flags1, PlayerFlags1::Standing);
        const bool noUnmorphPrev = TestFlag(_flags1, PlayerFlags1::NoUnmorph);
        _flags1 &= ~PlayerFlags1::Standing;
        _flags1 &= ~PlayerFlags1::StandingPrevious;
        _flags1 &= ~PlayerFlags1::NoUnmorph;
        _flags1 &= ~PlayerFlags1::NoUnmorphPrevious;
        _flags1 &= ~PlayerFlags1::OnLava;
        _flags1 &= ~PlayerFlags1::OnAcid;
        _flags2 &= ~PlayerFlags2::SpireClimbing;
        _flags1 &= ~PlayerFlags1::CollidingLateral;
        _flags1 &= ~PlayerFlags1::NoUnmorph;
        _flags1 &= ~PlayerFlags1::CollidingEntity;
        _flags1 &= ~PlayerFlags1::Standing;
        if (standingPrev)
        {
            _flags1 |= PlayerFlags1::StandingPrevious;
        }
        if (noUnmorphPrev)
        {
            _flags1 |= PlayerFlags1::NoUnmorphPrevious;
        }
        const Vector3 prevC0 = _fieldC0;
        _fieldC0 = Vector3::Zero;
        CheckCollision();
        if (_field449 > 0 && _field449 < 60)
        {
            _fieldC0 = prevC0;
        }
        else if (!VectorEquals(_fieldC0, Vector3::Zero))
        {
            _fieldC0 = _fieldC0.Normalized();
        }
        else
        {
            _fieldC0 = Vector3(0.0F, 1.0F, 0.0F);
        }
        if (_standTerrain != prevTerrain)
        {
            StopTerrainSfx(prevTerrain);
        }
        if (TestFlag(_flags1, PlayerFlags1::Standing)
            && !TestFlag(_flags1, PlayerFlags1::StandingPrevious))
        {
            _timeStanding = 0;
            if (_prevSpeed.Y >= 0.0F)
            {
                _field44C = 0.0F;
            }
            else
            {
                _field44C = -_prevSpeed.Y * 0.35F;
                if (_field44C > Fixed::ToFloat(800))
                {
                    _field44C = Fixed::ToFloat(800);
                }
                if (_prevSpeed.Y < -0.65F)
                {
                    _cameraInfo->SetShake(Fixed::ToFloat(204));
                }
            }
        }
        else if (_timeStanding != std::numeric_limits<std::uint16_t>::max())
        {
            ++_timeStanding;
        }
        if (IsAltForm())
        {
            UpdateAltTransform();
        }
        if (TestFlag(_flags1, PlayerFlags1::Grounded))
        {
            _flags1 |= PlayerFlags1::GroundedPrevious;
        }
        else
        {
            _flags1 &= ~PlayerFlags1::GroundedPrevious;
        }
        if (TestFlag(_flags1, PlayerFlags1::Standing) || TestFlag(_flags2, PlayerFlags2::SpireClimbing))
        {
            _timeBeforeLanding = _timeSinceGrounded;
            _timeSinceGrounded = 0;
            _flags1 |= PlayerFlags1::Grounded;
        }
        else if (_timeSinceGrounded < 180)
        {
            ++_timeSinceGrounded;
            if (_timeSinceGrounded >= 16)
            {
                _flags1 &= ~PlayerFlags1::Grounded;
                _walkSfxTimer = 0;
                _walkSfxIndex = 0;
            }
        }
        bool burning = false;
        if (_health > 0 && (_burnTimer > 0 || (_hunter != Hunter::Spire
            && TestFlag(_flags1, PlayerFlags1::OnLava) && TestFlag(_flags1, PlayerFlags1::Grounded))))
        {
            burning = true;
        }
        UpdateBurningSfx(burning);
        if ((!IsAltForm() || _hunter == Hunter::Weavel) && TestFlag(_flags1, PlayerFlags1::Grounded))
        {
            UpdateWalkingSfx();
        }
    }

    bool PlayerEntity::IsDown(const Keybind& control, KeyboardState keyboard,
        MouseState mouse)
    {
        if (control.Type() == ButtonType::Key)
        {
            return control.Key() != Keys::Unknown && keyboard.IsKeyDown(control.Key());
        }
        return control.Type() == ButtonType::Mouse && mouse.IsButtonDown(control.MouseButton());
    }

    void PlayerEntity::ProcessInput(KeyboardState keyboardState,
        MouseState mouseState, bool noPlayerInput)
    {
        const KeyboardState keyboardSnap = keyboardState.GetSnapshot();
        const MouseState mouseSnap = mouseState.GetSnapshot();
        if (Mods::SpectatorMode::IsSpectating())
        {
            Mods::SpectatorMode::NoteScoreboard(
                IsDown(Mods::InputSettings::Current().Pause(), keyboardSnap, mouseSnap));
        }
        const auto& players = Players();
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(players.size()); ++i)
        {
            const std::shared_ptr<PlayerEntity>& playerPtr = players[static_cast<std::size_t>(i)];
            if (!playerPtr)
            {
                throw System::NullReferenceException();
            }
            PlayerEntity& player = *playerPtr;
            if (player._isBot)
            {
                if (TestFlag(player._loadFlags, LoadFlags::Active))
                {
                    if (player.AiData == nullptr)
                    {
                        throw System::NullReferenceException();
                    }
                    player.AiData->ProcessInput();
                }
                continue;
            }
            if (Mods::Network::NetHooks::TryApplyRemoteInput(player, i))
            {
                continue;
            }
            if (noPlayerInput || i != Mods::Network::NetHooks::LocalSlot()
                || Mods::SpectatorMode::IsSpectating())
            {
                continue;
            }
            player._input.HasInput = false;
            const std::optional<KeyboardState> prevKeyboardSnap = player._input.KeyboardState;
            const std::optional<MouseState> prevMouseSnap = player._input.MouseState;
            player._input.PrevKeyboardState = prevKeyboardSnap;
            player._input.PrevMouseState = prevMouseSnap;
            player._input.KeyboardState = keyboardSnap;
            player._input.MouseState = mouseSnap;
            _isScrollingUp = false;
            _isScrollingDown = false;
            const float curScrollY = mouseSnap.Scroll.Y;
            const float prevScrollY = prevMouseSnap.has_value() ? prevMouseSnap->Scroll.Y : 0.0F;
            if (curScrollY > prevScrollY)
            {
                _isScrollingUp = true;
            }
            else if (curScrollY < prevScrollY)
            {
                _isScrollingDown = true;
            }
            if (TestFlag(player._loadFlags, LoadFlags::Active))
            {
                for (const std::shared_ptr<Keybind>& controlPtr : player._controls.All())
                {
                    if (!controlPtr)
                    {
                        throw System::NullReferenceException();
                    }
                    Keybind& control = *controlPtr;
                    if (control.Type() == ButtonType::Key)
                    {
                        if (control.Key() != Keys::Unknown)
                        {
                            const bool prevDown = prevKeyboardSnap.has_value()
                                ? prevKeyboardSnap->IsKeyDown(control.Key()) : false;
                            control.SetIsDown(keyboardSnap.IsKeyDown(control.Key()));
                            control.SetIsPressed(control.IsDown() && !prevDown);
                            control.SetIsReleased(!control.IsDown() && prevDown);
                            if (control.IsDown() || control.IsPressed() || control.IsReleased())
                            {
                                player._input.HasInput = true;
                            }
                        }
                    }
                    else if (control.Type() == ButtonType::Mouse)
                    {
                        if (GameState::DialogPause)
                        {
                            continue;
                        }
                        if (control.MouseButton() == MouseButton::Left && player._ignoreClick)
                        {
                            control.SetNeedsRepress(true);
                        }
                        const bool down = mouseSnap.IsButtonDown(control.MouseButton());
                        const bool prevDown = prevMouseSnap.has_value()
                            ? prevMouseSnap->IsButtonDown(control.MouseButton()) : false;
                        if (control.NeedsRepress() && !player._ignoreClick)
                        {
                            if (!down || !prevDown)
                            {
                                control.SetNeedsRepress(false);
                            }
                        }
                        if (!control.NeedsRepress())
                        {
                            control.SetIsDown(down);
                            control.SetIsPressed(control.IsDown() && !prevDown);
                            control.SetIsReleased(!control.IsDown() && prevDown);
                            if (control.IsDown() || control.IsPressed() || control.IsReleased())
                            {
                                player._input.HasInput = true;
                            }
                        }
                    }
                    else
                    {
                        control.SetIsDown(
                            (control.Type() == ButtonType::ScrollUp && _isScrollingUp)
                            || (control.Type() == ButtonType::ScrollDown && _isScrollingDown));
                        control.SetIsPressed(control.IsDown());
                        control.SetIsReleased(false);
                        if (control.IsDown())
                        {
                            player._input.HasInput = true;
                        }
                    }
                }
            }
            if (TestFlag(player._loadFlags, LoadFlags::Active))
            {
                ApplyStylusZone(player);
            }
            player._ignoreClick = false;
            if (mouseSnap.IsButtonDown(MouseButton::Left)
                && (!prevMouseSnap.has_value() || !prevMouseSnap->IsButtonDown(MouseButton::Left)))
            {
                player._input.ClickX = mouseSnap.X;
                player._input.ClickY = mouseSnap.Y;
                player._input.HasInput = true;
            }
            else
            {
                player._input.ClickX = -1.0F;
                player._input.ClickY = -1.0F;
            }
            if (i == 0 && RequireReference(player._scene).MoviePlaying)
            {
                bool skipMovie = false;
                Keybind& skipControl = player._controls.Shoot();
                if (skipControl.Type() == ButtonType::Key)
                {
                    if (skipControl.Key() != Keys::Unknown)
                    {
                        const bool prevDown = prevKeyboardSnap.has_value()
                            ? prevKeyboardSnap->IsKeyDown(skipControl.Key()) : false;
                        const bool isDown = keyboardSnap.IsKeyDown(skipControl.Key());
                        skipMovie = isDown && !prevDown;
                    }
                }
                else if (skipControl.Type() == ButtonType::Mouse)
                {
                    const bool prevDown = prevMouseSnap.has_value()
                        ? prevMouseSnap->IsButtonDown(skipControl.MouseButton()) : false;
                    const bool isDown = mouseSnap.IsButtonDown(skipControl.MouseButton());
                    skipMovie = isDown && !prevDown;
                }
                else
                {
                    skipMovie = (skipControl.Type() == ButtonType::ScrollUp && _isScrollingUp)
                        || (skipControl.Type() == ButtonType::ScrollDown && _isScrollingDown);
                }
                if (skipMovie)
                {
                    RequireReference(player._scene).SkipMovie();
                }
            }
        }
    }
}

namespace MphRead::Mods::Network::Detail
{
    Entities::PlayerControls& NetPlayerBridgeControls(Entities::PlayerEntity& player)
    {
        return player.Controls();
    }

    Entities::Keybind& NetPlayerBridgeControl(
        Entities::PlayerControls& controls, std::int32_t index)
    {
        switch (index)
        {
        case 0: return controls.MoveLeft();
        case 1: return controls.MoveRight();
        case 2: return controls.MoveUp();
        case 3: return controls.MoveDown();
        case 4: return controls.Shoot();
        case 5: return controls.Zoom();
        case 6: return controls.Jump();
        case 7: return controls.Morph();
        case 8: return controls.Boost();
        case 9: return controls.AltAttack();
        case 10: return controls.ScanVisor();
        case 11: return controls.NextWeapon();
        case 12: return controls.PrevWeapon();
        case 13: return controls.RolltLeft();
        case 14: return controls.RollRight();
        case 15: return controls.RollUp();
        case 16: return controls.RollDown();
        default: throw SceneDetail::IndexOutOfRangeException();
        }
    }

    bool NetPlayerBridgeKeybindIsDown(const Entities::Keybind& bind)
    {
        return bind.IsDown();
    }

    bool NetPlayerBridgeKeybindIsPressed(const Entities::Keybind& bind)
    {
        return bind.IsPressed();
    }

    void NetPlayerBridgeSetKeybindDown(Entities::Keybind& bind, bool value)
    {
        bind.SetIsDown(value);
    }

    void NetPlayerBridgeSetKeybindPressed(Entities::Keybind& bind, bool value)
    {
        bind.SetIsPressed(value);
    }

    void NetPlayerBridgeSetKeybindReleased(Entities::Keybind& bind, bool value)
    {
        bind.SetIsReleased(value);
    }

    void NetPlayerBridgeNoteInput(Entities::PlayerEntity& player)
    {
        player.ModNoteInput();
    }
}

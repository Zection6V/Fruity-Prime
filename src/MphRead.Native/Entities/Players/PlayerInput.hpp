#pragma once

#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"

#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    enum class Keys : std::int32_t
    {
        Unknown = -1,
        Space = 32,
        D0 = 48, D1 = 49, D2 = 50, D3 = 51, D4 = 52,
        D5 = 53, D6 = 54, D7 = 55, D8 = 56, D9 = 57,
        A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72,
        I = 73, J = 74, K = 75, L = 76, M = 77, N = 78, O = 79, P = 80,
        Q = 81, R = 82, S = 83, T = 84, U = 85, V = 86, W = 87, X = 88,
        Y = 89, Z = 90,
        Right = 262, Left = 263, Down = 264, Up = 265,
        Tab = 258,
        LeftShift = 340
    };

    enum class MouseButton : std::int32_t
    {
        Button1 = 0, Button2 = 1, Button3 = 2, Button4 = 3,
        Button5 = 4, Button6 = 5, Button7 = 6, Button8 = 7,
        Left = Button1, Right = Button2, Middle = Button3, Last = Button8
    };

    class KeyboardState final
    {
    public:
        KeyboardState() = default;
        [[nodiscard]] KeyboardState GetSnapshot() const { return *this; }
        [[nodiscard]] bool IsKeyDown(Keys key) const noexcept;
        void SetKeyDown(Keys key, bool down) noexcept;

    private:
        std::array<bool, 512> _down{};
    };

    class MouseState final
    {
    public:
        float X = 0.0F;
        float Y = 0.0F;
        ::OpenTK::Mathematics::Vector2 Scroll{};

        MouseState() = default;
        [[nodiscard]] MouseState GetSnapshot() const { return *this; }
        [[nodiscard]] bool IsButtonDown(MouseButton button) const noexcept;
        void SetButtonDown(MouseButton button, bool down) noexcept;

    private:
        std::array<bool, 8> _down{};
    };
}

namespace MphRead::Entities
{
    class PlayerEntity;

    enum class ButtonType : std::int32_t
    {
        Key = 0,
        Mouse = 1,
        ScrollUp = 2,
        ScrollDown = 3
    };

    class Keybind
    {
    public:
        using Keys = ::OpenTK::Windowing::GraphicsLibraryFramework::Keys;
        using MouseButtonType = ::OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;

        explicit Keybind(Keys key) noexcept;
        explicit Keybind(MouseButtonType mouseButton) noexcept;
        explicit Keybind(ButtonType scrollType);

        [[nodiscard]] ButtonType Type() const noexcept { return _type; }
        void SetType(ButtonType value) noexcept { _type = value; }
        [[nodiscard]] Keys Key() const noexcept { return _key; }
        void SetKey(Keys value) noexcept { _key = value; }
        [[nodiscard]] MouseButtonType MouseButton() const noexcept { return _mouseButton; }
        void SetMouseButton(MouseButtonType value) noexcept { _mouseButton = value; }

        [[nodiscard]] bool IsPressed() const noexcept { return _isPressed; }
        void SetIsPressed(bool value) noexcept { _isPressed = value; }
        [[nodiscard]] bool IsDown() const noexcept { return _isDown; }
        void SetIsDown(bool value) noexcept { _isDown = value; }
        [[nodiscard]] bool IsReleased() const noexcept { return _isReleased; }
        void SetIsReleased(bool value) noexcept { _isReleased = value; }
        [[nodiscard]] bool NeedsRepress() const noexcept { return _needsRepress; }
        void SetNeedsRepress(bool value) noexcept { _needsRepress = value; }

        [[nodiscard]] bool Equals(const std::any& obj) const noexcept;
        [[nodiscard]] std::int32_t GetHashCode() const;

        friend bool operator==(const Keybind& lhs, const Keybind& rhs) noexcept;
        friend bool operator!=(const Keybind& lhs, const Keybind& rhs) noexcept;

    private:
        ButtonType _type = ButtonType::Key;
        Keys _key = static_cast<Keys>(0);
        MouseButtonType _mouseButton = static_cast<MouseButtonType>(0);
        bool _isPressed = false;
        bool _isDown = false;
        bool _isReleased = false;
        bool _needsRepress = false;
    };

    class PlayerControls
    {
        friend class PlayerEntity;

    public:
        PlayerControls(std::shared_ptr<Keybind> moveLeft, std::shared_ptr<Keybind> moveRight,
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
            std::shared_ptr<Keybind> hudOverlay);

        [[nodiscard]] bool MouseAim() const noexcept { return _mouseAim; }
        void SetMouseAim(bool value) noexcept { _mouseAim = value; }
        [[nodiscard]] bool KeyboardAim() const noexcept { return _keyboardAim; }
        void SetKeyboardAim(bool value) noexcept { _keyboardAim = value; }
        [[nodiscard]] bool InvertAimY() const noexcept { return _invertAimY; }
        [[nodiscard]] bool InvertAimX() const noexcept { return _invertAimX; }
        [[nodiscard]] bool ScrollAllWeapons() const noexcept { return _scrollAllWeapons; }
        void SetScrollAllWeapons(bool value) noexcept { _scrollAllWeapons = value; }

#define MPHREAD_CONTROL_ACCESSOR(name, member) \
        [[nodiscard]] Keybind& name() { return RequireControl(member); } \
        [[nodiscard]] const Keybind& name() const { return RequireControl(member); }
        MPHREAD_CONTROL_ACCESSOR(MoveLeft, _moveLeft)
        MPHREAD_CONTROL_ACCESSOR(MoveRight, _moveRight)
        MPHREAD_CONTROL_ACCESSOR(MoveUp, _moveUp)
        MPHREAD_CONTROL_ACCESSOR(MoveDown, _moveDown)
        MPHREAD_CONTROL_ACCESSOR(RolltLeft, _rollLeft)
        MPHREAD_CONTROL_ACCESSOR(RollRight, _rollRight)
        MPHREAD_CONTROL_ACCESSOR(RollUp, _rollUp)
        MPHREAD_CONTROL_ACCESSOR(RollDown, _rollDown)
        MPHREAD_CONTROL_ACCESSOR(AimLeft, _aimLeft)
        MPHREAD_CONTROL_ACCESSOR(AimRight, _aimRight)
        MPHREAD_CONTROL_ACCESSOR(AimUp, _aimUp)
        MPHREAD_CONTROL_ACCESSOR(AimDown, _aimDown)
        MPHREAD_CONTROL_ACCESSOR(Shoot, _shoot)
        MPHREAD_CONTROL_ACCESSOR(Zoom, _zoom)
        MPHREAD_CONTROL_ACCESSOR(Jump, _jump)
        MPHREAD_CONTROL_ACCESSOR(Morph, _morph)
        MPHREAD_CONTROL_ACCESSOR(Boost, _boost)
        MPHREAD_CONTROL_ACCESSOR(AltAttack, _altAttack)
        MPHREAD_CONTROL_ACCESSOR(ScanVisor, _scanVisor)
        MPHREAD_CONTROL_ACCESSOR(Scan, _scan)
        MPHREAD_CONTROL_ACCESSOR(NextWeapon, _nextWeapon)
        MPHREAD_CONTROL_ACCESSOR(PrevWeapon, _prevWeapon)
        MPHREAD_CONTROL_ACCESSOR(WeaponMenu, _weaponMenu)
        MPHREAD_CONTROL_ACCESSOR(PowerBeam, _powerBeam)
        MPHREAD_CONTROL_ACCESSOR(Missile, _missile)
        MPHREAD_CONTROL_ACCESSOR(VoltDriver, _voltDriver)
        MPHREAD_CONTROL_ACCESSOR(Battlehammer, _battlehammer)
        MPHREAD_CONTROL_ACCESSOR(Imperialist, _imperialist)
        MPHREAD_CONTROL_ACCESSOR(Judicator, _judicator)
        MPHREAD_CONTROL_ACCESSOR(Magmaul, _magmaul)
        MPHREAD_CONTROL_ACCESSOR(ShockCoil, _shockCoil)
        MPHREAD_CONTROL_ACCESSOR(OmegaCannon, _omegaCannon)
        MPHREAD_CONTROL_ACCESSOR(AffinitySlot, _affinitySlot)
        MPHREAD_CONTROL_ACCESSOR(Pause, _pause)
        MPHREAD_CONTROL_ACCESSOR(HudOverlay, _hudOverlay)
#undef MPHREAD_CONTROL_ACCESSOR

        [[nodiscard]] std::array<std::shared_ptr<Keybind>, 35>& All() noexcept { return _all; }
        [[nodiscard]] const std::array<std::shared_ptr<Keybind>, 35>& All() const noexcept { return _all; }

        void ClearAll();
        void ClearPressed();
        [[nodiscard]] static PlayerControls GetDefault();

    private:
        [[nodiscard]] static PlayerControls CreateDefault();
        [[nodiscard]] Keybind& RequireControl(std::shared_ptr<Keybind>& control);
        [[nodiscard]] const Keybind& RequireControl(const std::shared_ptr<Keybind>& control) const;

        bool _mouseAim = true;
        bool _keyboardAim = true;
        bool _invertAimY = false;
        bool _invertAimX = false;
        bool _scrollAllWeapons = true;
        std::shared_ptr<Keybind> _moveLeft{};
        std::shared_ptr<Keybind> _moveRight{};
        std::shared_ptr<Keybind> _moveUp{};
        std::shared_ptr<Keybind> _moveDown{};
        std::shared_ptr<Keybind> _rollLeft{};
        std::shared_ptr<Keybind> _rollRight{};
        std::shared_ptr<Keybind> _rollUp{};
        std::shared_ptr<Keybind> _rollDown{};
        std::shared_ptr<Keybind> _aimLeft{};
        std::shared_ptr<Keybind> _aimRight{};
        std::shared_ptr<Keybind> _aimUp{};
        std::shared_ptr<Keybind> _aimDown{};
        std::shared_ptr<Keybind> _shoot{};
        std::shared_ptr<Keybind> _zoom{};
        std::shared_ptr<Keybind> _jump{};
        std::shared_ptr<Keybind> _morph{};
        std::shared_ptr<Keybind> _boost{};
        std::shared_ptr<Keybind> _altAttack{};
        std::shared_ptr<Keybind> _scanVisor{};
        std::shared_ptr<Keybind> _scan{};
        std::shared_ptr<Keybind> _nextWeapon{};
        std::shared_ptr<Keybind> _prevWeapon{};
        std::shared_ptr<Keybind> _weaponMenu{};
        std::shared_ptr<Keybind> _powerBeam{};
        std::shared_ptr<Keybind> _missile{};
        std::shared_ptr<Keybind> _voltDriver{};
        std::shared_ptr<Keybind> _battlehammer{};
        std::shared_ptr<Keybind> _imperialist{};
        std::shared_ptr<Keybind> _judicator{};
        std::shared_ptr<Keybind> _magmaul{};
        std::shared_ptr<Keybind> _shockCoil{};
        std::shared_ptr<Keybind> _omegaCannon{};
        std::shared_ptr<Keybind> _affinitySlot{};
        std::shared_ptr<Keybind> _pause{};
        std::shared_ptr<Keybind> _hudOverlay{};
        std::array<std::shared_ptr<Keybind>, 35> _all{};
    };


}

#define MPHREAD_PLAYER_INPUT_MEMBERS                                                           \
public:                                                                                        \
    [[nodiscard]] ::MphRead::Entities::PlayerControls& Controls() noexcept { return _controls; } \
    [[nodiscard]] const ::MphRead::Entities::PlayerControls& Controls() const noexcept { return _controls; } \
    static void ProcessInput(                                                                  \
        ::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState keyboardState,            \
        ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState mouseState,                  \
        bool noPlayerInput);                                                                   \
    void ResetAdventureModeBotWeapon();                                                        \
private:                                                                                       \
    void ProcessInput();                                                                       \
    static void ApplyStylusZone(::MphRead::Entities::PlayerEntity& player);                    \
    void ProcessTouchInput();                                                                  \
    [[nodiscard]] bool CanCycleToWeapon(::MphRead::BeamType beam);                            \
    [[nodiscard]] bool EndWeaponMenu();                                                        \
    void UpdateAimFacing();                                                                    \
    void UpdateAimY(float amount);                                                             \
    void UpdateAimX(float amount);                                                             \
    void UpdateHudShiftY(float amount);                                                        \
    void UpdateHudShiftX(float amount);                                                        \
    void ProcessBiped();                                                                       \
    [[nodiscard]] bool TryFireWeapon();                                                        \
    void UpdateAdventureModeBotWeapon();                                                       \
    void ProcessAlt();                                                                         \
    void SpawnBomb();                                                                          \
    void EndAltAttack();                                                                       \
    void ProcessMovement();                                                                    \
    [[nodiscard]] static bool IsDown(                                                         \
        const ::MphRead::Entities::Keybind& control,                                          \
        ::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState keyboard,                 \
        ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState mouse);               \
    std::array<float, 8> _pastAimX{};                                                          \
    std::array<float, 8> _pastAimY{};                                                          \
    float _buttonAimX = 0.0F;                                                                  \
    float _buttonAimY = 0.0F;                                                                  \
    static constexpr float _maxButtonAimX = 8.0F;                                              \
    static constexpr float _maxButtonAimY = 8.0F;                                              \
    class PlayerInput final                                                                    \
    {                                                                                           \
    public:                                                                                     \
        std::optional<::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState> PrevKeyboardState{}; \
        std::optional<::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState> KeyboardState{}; \
        std::optional<::OpenTK::Windowing::GraphicsLibraryFramework::MouseState> PrevMouseState{}; \
        std::optional<::OpenTK::Windowing::GraphicsLibraryFramework::MouseState> MouseState{}; \
        float ClickX = -1.0F;                                                                   \
        float ClickY = -1.0F;                                                                   \
        bool HasInput = false;                                                                  \
        [[nodiscard]] float MouseDeltaX() const;                                                \
        [[nodiscard]] float MouseDeltaY() const;                                                \
    };                                                                                          \
    ::MphRead::Entities::PlayerControls _controls = ::MphRead::Entities::PlayerControls::GetDefault(); \
    PlayerInput _input{};                                                                       \
    inline static const std::array<::MphRead::BeamType, 9> _weaponOrder =                     \
    {                                                                                          \
        ::MphRead::BeamType::PowerBeam, ::MphRead::BeamType::Missile,                         \
        ::MphRead::BeamType::VoltDriver, ::MphRead::BeamType::Battlehammer,                   \
        ::MphRead::BeamType::Imperialist, ::MphRead::BeamType::Judicator,                     \
        ::MphRead::BeamType::Magmaul, ::MphRead::BeamType::ShockCoil,                         \
        ::MphRead::BeamType::OmegaCannon                                                       \
    };                                                                                         \
    inline static bool _isScrollingUp = false;                                                 \
    inline static bool _isScrollingDown = false;

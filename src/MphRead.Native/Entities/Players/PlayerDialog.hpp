#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Entities
{
    enum class DialogType : std::int32_t
    {
        None = -1,
        Overlay = 0,
        Hud = 1,
        Okay = 3,
        Event = 4,
        YesNo = 5,
        Scan = 6
    };

    enum class ConfirmState : std::int32_t
    {
        No = 0,
        Yes = 1,
        Okay = 2
    };

    enum class PromptType : std::int32_t
    {
        Any = 0,
        ShipHatch = 1,
        GameOver = 2
    };

    enum class EventType : std::int32_t
    {
        EnergyTank = 0,
        VoltDriver = 1,
        MissileTank = 2,
        Battlehammer = 3,
        Imperialist = 4,
        Judicator = 5,
        Magmaul = 6,
        ShockCoil = 7,
        OmegaCannon = 8,
        Artifact = 15,
        Octolith = 16,
        UATank = 17
    };
}

#define MPHREAD_PLAYER_DIALOG_MEMBERS                                                        \
public:                                                                                       \
    [[nodiscard]] ::MphRead::Entities::DialogType DialogType() const noexcept                 \
    {                                                                                          \
        return _dialogType;                                                                    \
    }                                                                                          \
    void DialogType(::MphRead::Entities::DialogType value) noexcept                           \
    {                                                                                          \
        _dialogType = value;                                                                   \
    }                                                                                          \
    [[nodiscard]] ::MphRead::Entities::ConfirmState DialogConfirmState() const noexcept        \
    {                                                                                          \
        return _dialogConfirmState;                                                            \
    }                                                                                          \
    void DialogConfirmState(::MphRead::Entities::ConfirmState value) noexcept                  \
    {                                                                                          \
        _dialogConfirmState = value;                                                           \
    }                                                                                          \
    [[nodiscard]] ::MphRead::Entities::PromptType DialogPromptType() const noexcept            \
    {                                                                                          \
        return _dialogPromptType;                                                              \
    }                                                                                          \
    void DialogPromptType(::MphRead::Entities::PromptType value) noexcept                      \
    {                                                                                          \
        _dialogPromptType = value;                                                             \
    }                                                                                          \
    void ShowDialog(::MphRead::Entities::DialogType type, std::int32_t messageId,              \
        std::int32_t param1 = 0, std::int32_t param2 = 0,                                      \
        std::optional<std::string> value1 = std::nullopt,                                      \
        std::optional<std::string> value2 = std::nullopt);                                     \
    void CloseDialogs();                                                                       \
    void UpdateDialogs();                                                                      \
private:                                                                                       \
    struct ButtonInfo final                                                                    \
    {                                                                                          \
        float Left;                                                                            \
        float Right;                                                                           \
        float Top;                                                                             \
        float Bottom;                                                                          \
        constexpr ButtonInfo(float left, float right, float top, float bottom) noexcept        \
            : Left(left / 256.0F), Right(right / 256.0F),                                      \
              Top(top / 192.0F), Bottom(bottom / 192.0F)                                      \
        {                                                                                      \
        }                                                                                      \
    };                                                                                         \
    enum class DialogButton : std::int32_t                                                     \
    {                                                                                          \
        Okay = 0,                                                                              \
        Yes = 1,                                                                               \
        No = 2,                                                                                \
        Left = 3,                                                                              \
        Right = 4,                                                                             \
        Quit = 5                                                                               \
    };                                                                                         \
    inline static const std::array<ButtonInfo, 6> _buttonInfo =                                \
    {                                                                                          \
        ButtonInfo(111, 145, 173, 191),                                                        \
        ButtonInfo(79, 113, 173, 191),                                                         \
        ButtonInfo(143, 177, 173, 191),                                                        \
        ButtonInfo(55, 88, 172, 190),                                                          \
        ButtonInfo(168, 201, 172, 190),                                                        \
        ButtonInfo(3, 48, 157, 188)                                                            \
    };                                                                                         \
    ::MphRead::Entities::DialogType _dialogType = ::MphRead::Entities::DialogType::None;       \
    std::optional<std::string> _overlayMessage1{};                                             \
    std::optional<std::string> _overlayMessage2{};                                             \
    std::array<char16_t, 128> _overlayBuffer1{};                                               \
    std::array<char16_t, 512> _overlayBuffer2{};                                               \
    std::optional<std::string> _dialogValue1{};                                                \
    std::optional<std::string> _dialogValue2{};                                                \
    float _overlayTimer = 0.0F;                                                                \
    float _dialogCharTimer = 0.0F;                                                             \
    std::int32_t _dialogPalette = 0;                                                           \
    float _overlayTextOffsetY = 0.0F;                                                          \
    std::int32_t _prevOverlayCharacters = 0;                                                   \
    bool _showDialogConfirm = false;                                                           \
    float _dialogConfirmTimer = 0.0F;                                                          \
    bool _lastDialogPageSeen = false;                                                          \
    std::int32_t _dialogPageCount = 0;                                                         \
    std::int32_t _dialogPageIndex = 0;                                                         \
    std::array<std::int32_t, 10> _dialogPageLengths{};                                         \
    ::MphRead::Entities::ConfirmState _dialogConfirmState =                                    \
        ::MphRead::Entities::ConfirmState::Okay;                                               \
    ::MphRead::Entities::PromptType _dialogPromptType = ::MphRead::Entities::PromptType::Any;  \
    ::MphRead::Entities::EventType _eventType = ::MphRead::Entities::EventType::EnergyTank;    \
    bool _ignoreClick = false;                                                                 \
    void BufferDialogPages();                                                                  \
    void ShowDialogOverlay(std::int32_t messageId, std::int32_t duration, bool warning);        \
    void ShowDialogHud(std::int32_t messageId, std::int32_t duration, bool unpause);            \
    void ShowDialogPrompt(std::int32_t messageId);                                              \
    void ShowDialogScan();                                                                     \
    void ShowDialogEvent(std::int32_t messageId, ::MphRead::Entities::EventType eventType);     \
    void DrawDialogs();                                                                         \
    void DrawDialogConfirmButtons(::MphRead::Entities::DialogType dialogType);                  \
    [[nodiscard]] bool CheckButtonPressed(DialogButton type);

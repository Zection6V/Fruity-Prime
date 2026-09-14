#pragma once

#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"
#include "../../HUD/HudInfo.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace MphRead::Entities
{

enum class DialogType : std::int32_t
{
    None = -1,
    Okay,
    YesNo
};

enum class DialogButton : std::int32_t
{
    None,
    Okay,
    Yes,
    No
};

#define MPHREAD_PLAYER_DIALOG_MEMBERS                                                            \
private:                                                                                          \
    DialogType _dialogType = DialogType::None;                                                    \
    std::int32_t _dialogId = -1;                                                                  \
    DialogButton _dialogButton = DialogButton::None;                                              \
    bool _dialogConfirm = false;                                                                  \
    std::int32_t _dialogConfirmTimer = 0;                                                         \
    std::string _dialogText = "";                                                                 \
    std::string _dialogBuffer = "";                                                               \
    std::int32_t _dialogTextSpacing = 10;                                                         \
    std::int32_t _dialogTextChars = 0;                                                            \
    std::int32_t _dialogPage = 0;                                                                 \
    std::int32_t _dialogPageCount = 0;                                                            \
    std::int32_t _dialogPageLength = 0;                                                           \
    std::int32_t _dialogPageLines = 0;                                                            \
    std::int32_t _dialogPageTimer = 0;                                                            \
                                                                                                  \
    void SetPlayerSpawning(bool respawn);                                                         \
                                                                                                  \
    static constexpr float DialogConfirmTime = 90 / 30.0F;                                       \
    static constexpr float DialogPageTime = 15 / 30.0F;                                          \
                                                                                                  \
    void ProcessDialog();                                                                         \
    void ExecuteDialogAction(std::int32_t dialogId);                                              \
    void GiveAllOctoliths();                                                                      \
    void LeaveOtherShip();                                                                        \
    void AddCloakingDevice();                                                                     \
    void UnlockAltForm();                                                                         \
    void UnlockAllWeapons();                                                                      \
    void DrawDialog();                                                                            \
                                                                                                  \
    std::vector<std::tuple<DialogButton,                                                          \
        std::shared_ptr<MphRead::Hud::HudObjectInstance>,                                         \
        OpenTK::Mathematics::Vector2, std::string>> _dialogButtons{};                              \
                                                                                                  \
public:                                                                                           \
    void SetDialogTemplates(                                                                      \
        MphRead::Hud::ReadOnlyList<                                                               \
            std::shared_ptr<MphRead::Hud::HudObjectInstance>> messageBoxInst,                     \
        MphRead::Hud::ReadOnlyList<                                                               \
            std::shared_ptr<MphRead::Hud::HudObjectInstance>> buttonInst);                        \
    void ShowDialog(DialogType type, std::int32_t messageId,                                      \
        std::int32_t value1 = 0, std::int32_t value2 = 0);                                       \
                                                                                                  \
private:                                                                                          \
    void SetOverlayMessage(std::int32_t index, std::int32_t param1);                              \
    void SetHudMessage(std::int32_t index, std::int32_t param1);                                  \
    std::string WrapText(                                                                         \
        std::string text, std::int32_t maxWidth, std::int32_t spacing, std::int32_t fontSpacing); \
                                                                                                  \
    static const std::vector<std::pair<std::int32_t, MphRead::Hunter>> PlayerIndexMessages

} // namespace MphRead::Entities

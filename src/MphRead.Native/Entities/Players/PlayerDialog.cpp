#include "PlayerDialog.hpp"

#include "../../GameState.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Program.hpp"
#include "../../Scene.hpp"
#include "../../Text/Strings.hpp"
#include "../EnemyInstanceEntity.hpp"
#include "PlayerEntity.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using MphRead::Hud::HudObjectInstance;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;

    struct Utf8Character
    {
        char32_t Value;
        std::size_t Length;
        bool Valid;
    };

    [[nodiscard]] Utf8Character DecodeUtf8(
        std::string_view text, std::size_t offset) noexcept
    {
        const auto byte = static_cast<unsigned char>(text[offset]);
        if (byte < 0x80)
        {
            return {byte, 1, true};
        }

        std::size_t length = 0;
        char32_t value = 0;
        char32_t minimum = 0;
        if ((byte & 0xE0) == 0xC0)
        {
            length = 2;
            value = byte & 0x1F;
            minimum = 0x80;
        }
        else if ((byte & 0xF0) == 0xE0)
        {
            length = 3;
            value = byte & 0x0F;
            minimum = 0x800;
        }
        else if ((byte & 0xF8) == 0xF0)
        {
            length = 4;
            value = byte & 0x07;
            minimum = 0x10000;
        }
        else
        {
            return {0xFFFD, 1, false};
        }

        if (offset + length > text.size())
        {
            return {0xFFFD, 1, false};
        }
        for (std::size_t i = 1; i < length; ++i)
        {
            const auto continuation = static_cast<unsigned char>(text[offset + i]);
            if ((continuation & 0xC0) != 0x80)
            {
                return {0xFFFD, 1, false};
            }
            value = (value << 6) | (continuation & 0x3F);
        }
        if (value < minimum || value > 0x10FFFF
            || (value >= 0xD800 && value <= 0xDFFF))
        {
            return {0xFFFD, 1, false};
        }
        return {value, length, true};
    }

    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view text)
    {
        std::u16string result;
        std::size_t offset = 0;
        while (offset < text.size())
        {
            const Utf8Character character = DecodeUtf8(text, offset);
            const char32_t value = character.Value;
            if (value <= 0xFFFF)
            {
                result.push_back(static_cast<char16_t>(value));
            }
            else
            {
                const char32_t scalar = value - 0x10000;
                result.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FF)));
            }
            offset += character.Length;
        }
        return result;
    }

    void AppendUtf8Scalar(std::string& result, char32_t value)
    {
        if (value <= 0x7F)
        {
            result.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FF)
        {
            result.push_back(static_cast<char>(0xC0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
        else if (value <= 0xFFFF)
        {
            result.push_back(static_cast<char>(0xE0 | (value >> 12)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
        else
        {
            result.push_back(static_cast<char>(0xF0 | (value >> 18)));
            result.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
    }

    [[nodiscard]] std::string Utf16ToUtf8(std::u16string_view text)
    {
        std::string result;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char16_t unit = text[i];
            char32_t value = unit;
            if (unit >= 0xD800 && unit <= 0xDBFF)
            {
                if (i + 1 < text.size()
                    && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
                {
                    const char32_t high = static_cast<char32_t>(unit) - 0xD800;
                    const char32_t low
                        = static_cast<char32_t>(text[++i]) - 0xDC00;
                    value = 0x10000 + (high << 10) + low;
                }
                else
                {
                    value = 0xFFFD;
                }
            }
            else if (unit >= 0xDC00 && unit <= 0xDFFF)
            {
                value = 0xFFFD;
            }
            AppendUtf8Scalar(result, value);
        }
        return result;
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

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    template <typename TEnum>
    [[nodiscard]] constexpr TEnum SetFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return static_cast<TEnum>(static_cast<U>(value) | static_cast<U>(flag));
    }

    template <typename TEnum>
    [[nodiscard]] constexpr TEnum ClearFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return static_cast<TEnum>(
            static_cast<U>(value) & static_cast<U>(~static_cast<U>(flag)));
    }

    [[nodiscard]] constexpr std::int32_t ManagedAdd(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t value
            = static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t ManagedSubtract(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t value
            = static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t ManagedMultiply(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t value
            = static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t ManagedIncrement(std::int32_t value) noexcept
    {
        return ManagedAdd(value, 1);
    }

    [[nodiscard]] constexpr std::int32_t ManagedDecrement(std::int32_t value) noexcept
    {
        return ManagedSubtract(value, 1);
    }

    [[nodiscard]] std::int32_t ConvertToInt32Net9(float value) noexcept
    {
        if (value != value)
        {
            return 0;
        }
        const double wide = static_cast<double>(value);
        if (wide < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (wide > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(wide);
    }

    [[nodiscard]] std::int32_t ManagedStringLength(const std::string& value)
    {
        return static_cast<std::int32_t>(Utf8ToUtf16(value).size());
    }

    [[nodiscard]] char16_t ManagedCharAt(
        const std::string& value, std::int32_t index)
    {
        const std::u16string utf16 = Utf8ToUtf16(value);
        if (index < 0 || static_cast<std::size_t>(index) >= utf16.size())
        {
            throw std::out_of_range("Index was outside the bounds of the string.");
        }
        return utf16[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] std::string ManagedSubstring(
        const std::string& value, std::int32_t startIndex)
    {
        const std::u16string utf16 = Utf8ToUtf16(value);
        if (startIndex < 0 || static_cast<std::size_t>(startIndex) > utf16.size())
        {
            throw std::out_of_range("startIndex");
        }
        return Utf16ToUtf8(utf16.substr(static_cast<std::size_t>(startIndex)));
    }

    [[nodiscard]] std::string ManagedSubstring(
        const std::string& value, std::int32_t startIndex, std::int32_t length)
    {
        const std::u16string utf16 = Utf8ToUtf16(value);
        if (startIndex < 0 || length < 0
            || static_cast<std::size_t>(startIndex) > utf16.size()
            || static_cast<std::size_t>(length)
                > utf16.size() - static_cast<std::size_t>(startIndex))
        {
            throw std::out_of_range("Substring range");
        }
        return Utf16ToUtf8(utf16.substr(
            static_cast<std::size_t>(startIndex), static_cast<std::size_t>(length)));
    }

    void ReplaceAll(std::string& value, const std::string& oldValue, const std::string& newValue)
    {
        std::size_t position = 0;
        while ((position = value.find(oldValue, position)) != std::string::npos)
        {
            value.replace(position, oldValue.size(), newValue);
            position += newValue.size();
        }
    }

    [[nodiscard]] std::string FormatDecimal2(std::int32_t value)
    {
        std::ostringstream stream;
        if (value < 0)
        {
            stream << '-';
            const std::uint32_t magnitude
                = 0U - static_cast<std::uint32_t>(value);
            stream << std::setfill('0') << std::setw(2) << magnitude;
        }
        else
        {
            stream << std::setfill('0') << std::setw(2) << value;
        }
        return stream.str();
    }

    [[nodiscard]] std::string FormatHex8(std::int32_t value)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
               << static_cast<std::uint32_t>(value);
        return stream.str();
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return values[static_cast<std::size_t>(index)];
    }
}

namespace MphRead::Entities
{
    const std::vector<std::pair<std::int32_t, MphRead::Hunter>> PlayerEntity::PlayerIndexMessages
    {
        {100, MphRead::Hunter::Guardian},
        {101, MphRead::Hunter::Samus},
        {102, MphRead::Hunter::Kanden},
        {103, MphRead::Hunter::Trace},
        {104, MphRead::Hunter::Sylux},
        {105, MphRead::Hunter::Noxus},
        {106, MphRead::Hunter::Spire},
        {107, MphRead::Hunter::Weavel}
    };

    void PlayerEntity::SetPlayerSpawning(bool respawn)
    {
        if (TestFlag(LoadFlags(), MphRead::Entities::LoadFlags::Spawning) && !respawn)
        {
            Respawn();
            return;
        }
        if (respawn)
        {
            LoadFlags = SetFlag(LoadFlags(), MphRead::Entities::LoadFlags::Spawning);
        }
        else
        {
            LoadFlags = ClearFlag(LoadFlags(), MphRead::Entities::LoadFlags::Spawning);
        }
        _controlLockTime = 0;
        SetCollision(CollisionType::None);
        SetVisible(false);
        LoadFlags = ClearFlag(LoadFlags(), MphRead::Entities::LoadFlags::Active);
        PlayHunterSound(HunterSfx::NoHealth);
    }

    void PlayerEntity::ProcessDialog()
    {
        if (_dialogType == DialogType::None
            || !TestFlag(LoadFlags(), MphRead::Entities::LoadFlags::Active))
        {
            return;
        }
        if (_dialogId == Metadata::DialogIds::GameOver
            && TestFlag(LoadFlags(), MphRead::Entities::LoadFlags::Spawning))
        {
            if (!RequireReference(_scene).Multiplayer)
            {
                if (RequireReference(_scene).GameMode != MphRead::GameMode::Adventure)
                {
                    CloseDialog();
                }
                return;
            }
            SetPlayerSpawning(false);
        }
        _controlLockTime = 0;
        if (_dialogConfirmTimer > 0)
        {
            _dialogConfirmTimer = ManagedDecrement(_dialogConfirmTimer);
        }
        if (_dialogConfirm)
        {
            if (_dialogConfirmTimer == 0)
            {
                if (RequireReference(_scene).GameMode != MphRead::GameMode::Adventure)
                {
                    if (Controls().StartPressed)
                    {
                        CloseDialog();
                    }
                }
                else if (Controls().ControlUpPressed || Controls().ControlDownPressed)
                {
                    _dialogButton = _dialogButton == DialogButton::Yes
                        ? DialogButton::No
                        : DialogButton::Yes;
                }
                if (_dialogType == DialogType::Okay)
                {
                    if (Controls().ControlUpPressed || Controls().ControlDownPressed)
                    {
                        _dialogButton = DialogButton::Okay;
                    }
                    if (_dialogButton == DialogButton::Okay
                        && Controls().ControlUpReleased && Controls().ControlDownReleased)
                    {
                        _dialogButton = DialogButton::None;
                    }
                    else if (_dialogButton != DialogButton::Okay
                        && (Controls().ControlUpReleased || Controls().ControlDownReleased))
                    {
                        CloseDialog();
                    }
                }
                else if (_dialogType == DialogType::YesNo)
                {
                    if (Controls().ControlUpReleased || Controls().ControlDownReleased)
                    {
                        if (_dialogId == Metadata::DialogIds::GameOver)
                        {
                            SetPlayerSpawning(true);
                            return;
                        }
                        if (_dialogId == Metadata::DialogIds::ShipHatch)
                        {
                            if (_dialogButton == DialogButton::Yes)
                            {
                                RequireReference(_scene).ActivateCameraSequence(0);
                                RequireReference(_scene).SetFade(FadeType::FadeOutWhite, 20, true);
                            }
                            else
                            {
                                CloseDialog();
                            }
                            return;
                        }
                        if (_dialogButton == DialogButton::Yes)
                        {
                            ExecuteDialogAction(_dialogId);
                        }
                        CloseDialog();
                        return;
                    }
                }
            }
            return;
        }
        if (!_dialogBuffer.empty())
        {
            const std::int32_t prevLength = _dialogPageLength;
            _dialogPageLength = 0;
            _dialogPageLines = 0;
            const std::int32_t length = ManagedStringLength(_dialogBuffer);
            for (std::int32_t i = 0; i < length; i = ManagedIncrement(i))
            {
                if (ManagedCharAt(_dialogBuffer, i) == u'\n')
                {
                    if (_dialogPageLines == 2)
                    {
                        break;
                    }
                    _dialogPageLines = ManagedIncrement(_dialogPageLines);
                }
                _dialogPageLength = ManagedIncrement(_dialogPageLength);
            }
            _dialogPageLength = ManagedAdd(_dialogPageLength, prevLength);
            _dialogBuffer = ManagedSubstring(
                _dialogBuffer, ManagedSubtract(_dialogPageLength, prevLength));
            _dialogPage = ManagedIncrement(_dialogPage);
            _dialogPageTimer = ConvertToInt32Net9(
                static_cast<float>(_dialogPageLength) / 30.0F * 3.0F);
        }
        _dialogTextChars = ManagedIncrement(_dialogTextChars);
        if (_dialogBuffer.empty() && _dialogTextChars > _dialogPageLength)
        {
            _dialogPageTimer = ManagedDecrement(_dialogPageTimer);
            if (_dialogPageTimer == 0)
            {
                _dialogType = DialogType::None;
                _dialogConfirm = true;
                _dialogConfirmTimer = ConvertToInt32Net9(DialogConfirmTime * 30.0F);
                if (RequireReference(_scene).GameMode == MphRead::GameMode::Adventure)
                {
                    _dialogButton = _dialogType == DialogType::Okay
                        ? DialogButton::Okay
                        : DialogButton::Yes;
                }
            }
        }
        else if (_dialogTextChars > _dialogPageLength)
        {
            _dialogPageTimer = ManagedDecrement(_dialogPageTimer);
            if (_dialogPageTimer == 0)
            {
                _dialogTextChars = _dialogPageLength;
                _dialogType = DialogType::None;
                _dialogConfirm = true;
                _dialogConfirmTimer = ConvertToInt32Net9(DialogPageTime * 30.0F);
                _dialogButton = DialogButton::Okay;
            }
        }
    }

    void PlayerEntity::ExecuteDialogAction(std::int32_t dialogId)
    {
        if (dialogId == Metadata::DialogIds::OtherShip)
        {
            LeaveOtherShip();
        }
        else if (dialogId == Metadata::DialogIds::CloakingDevice)
        {
            AddCloakingDevice();
        }
        else if (dialogId == Metadata::DialogIds::AlternateForm)
        {
            UnlockAltForm();
        }
        else if (dialogId == Metadata::DialogIds::OctoLith)
        {
            GiveAllOctoliths();
        }
        else if (dialogId == Metadata::DialogIds::Weapons)
        {
            UnlockAllWeapons();
        }
        else if (dialogId == Metadata::DialogIds::Ammo)
        {
            CurrentMissileAmmo = MaxMissileAmmo();
            CurrentAmmo[0] = Ammo[0] = MaxAmmo[0];
            CurrentAmmo[1] = Ammo[1] = MaxAmmo[1];
            PlayHunterSound(HunterSfx::EnergyPickup);
            UpdateDoubleDamageSfx(false);
        }
        else if (dialogId == Metadata::DialogIds::Energy)
        {
            SetHealth(MaxHealth());
            PlayHunterSound(HunterSfx::EnergyPickup);
            UpdateDoubleDamageSfx(false);
        }
    }

    void PlayerEntity::GiveAllOctoliths()
    {
        if (OctolithCount() != 8)
        {
            PlayHunterSound(HunterSfx::OctolithFlagTaken);
        }
        OctolithCount = 8;
        GameState::Octoliths = 255;
    }

    void PlayerEntity::LeaveOtherShip()
    {
        if (OctolithCount() <= 8)
        {
            OctolithCount = std::max(0, ManagedSubtract(OctolithCount(), 1));
            GameState::Octoliths = static_cast<std::uint8_t>(GameState::Octoliths >> 1);
        }
        CloseDialog();
    }

    void PlayerEntity::AddCloakingDevice()
    {
        const std::int32_t health = Health();
        ProcessSubroutine(Enemies::SubroutinePowers::CloakingDevice);
        GameState::CloakingTime = 900;
        SetHealth(health);
        CloseDialog();
    }

    void PlayerEntity::UnlockAltForm()
    {
        if (!TestFlag(LoadFlags(), MphRead::Entities::LoadFlags::AltForm))
        {
            PlayHunterSound(HunterSfx::WeaponSelect);
            LoadFlags = SetFlag(LoadFlags(), MphRead::Entities::LoadFlags::AltForm);
            RequireReference(
                RequireReference(_scene).HudObjects.at("AltFormLocked")).Enabled = false;
            RequireReference(RequireReference(_scene).HudObjects.at("AltFormIcon")).Enabled = true;
            UpdateDoubleDamageSfx(false);
        }
    }

    void PlayerEntity::UnlockAllWeapons()
    {
        PlayHunterSound(HunterSfx::WeaponSelect);
        using U = std::underlying_type_t<BeamType>;
        const BeamType value = static_cast<BeamType>(
            static_cast<U>(BeamType::VoltDriver)
            | static_cast<U>(BeamType::Battlehammer)
            | static_cast<U>(BeamType::Imperialist)
            | static_cast<U>(BeamType::Judicator)
            | static_cast<U>(BeamType::Magmaul)
            | static_cast<U>(BeamType::ShockCoil));
        BeamAbility = static_cast<BeamType>(static_cast<U>(BeamAbility()) | static_cast<U>(value));
        UpdateDoubleDamageSfx(false);
    }

    void PlayerEntity::DrawDialog()
    {
        if (!TestFlag(LoadFlags(), MphRead::Entities::LoadFlags::Active))
        {
            return;
        }
        Scene& scene = RequireReference(_scene);
        if (scene.GameMode == MphRead::GameMode::Adventure || scene.HudObjects.empty())
        {
            return;
        }
        if (_dialogId != Metadata::DialogIds::Overlay
            && _dialogId != Metadata::DialogIds::HudMessage)
        {
            return;
        }
        if (!_dialogBuffer.empty() && _dialogPageLength < _dialogTextChars)
        {
            const std::int32_t pageLength = ManagedAdd(_dialogPageLength, 1);
            std::string page = ManagedSubstring(_dialogText, 0, pageLength);
            page += "\n";
            page += ManagedSubstring(
                _dialogText, pageLength, ManagedSubtract(_dialogTextChars, pageLength));
            scene.Renderer.DrawText(page, _messageBoxInst, _messageBoxPosX, _messageBoxPosY,
                Palette::White, _dialogTextSpacing, 0);
        }
        else
        {
            scene.Renderer.DrawText(
                ManagedSubstring(_dialogText, 0, _dialogTextChars),
                _messageBoxInst, _messageBoxPosX, _messageBoxPosY,
                Palette::White, _dialogTextSpacing, 0);
        }
        if (_dialogConfirm && _dialogConfirmTimer == 0)
        {
            if (_dialogType == DialogType::Okay || _dialogType == DialogType::YesNo)
            {
                const std::int32_t count = _dialogType == DialogType::YesNo ? 2 : 1;
                for (std::int32_t i = 0; i < count; i = ManagedIncrement(i))
                {
                    const std::int32_t x = i == 0 ? 0 : 132;
                    const std::int32_t index
                        = ManagedAdd(i, _dialogType == DialogType::YesNo ? 1 : 0);
                    const auto& item = _dialogButtons.at(static_cast<std::size_t>(index));
                    const DialogButton button = std::get<0>(item);
                    const std::shared_ptr<HudObjectInstance> inst = std::get<1>(item);
                    const Vector2 position = std::get<2>(item);
                    if (button == _dialogButton)
                    {
                        scene.Renderer.DrawHudObject(inst, x, 0);
                    }
                    scene.Renderer.DrawHudText(std::get<3>(item),
                        ManagedAdd(x, ConvertToInt32Net9(position.X)),
                        ConvertToInt32Net9(position.Y), Palette::White, 10, 0);
                }
            }
            else
            {
                scene.Renderer.DrawText("Press Start", _messageBoxInst,
                    ManagedAdd(_messageBoxPosX, 24), ManagedAdd(_messageBoxPosY, 90),
                    Palette::White, _dialogTextSpacing, 0);
            }
        }
    }

    void PlayerEntity::SetDialogTemplates(
        Hud::ReadOnlyList<std::shared_ptr<HudObjectInstance>> messageBoxInst,
        Hud::ReadOnlyList<std::shared_ptr<HudObjectInstance>> buttonInst)
    {
        if (!buttonInst)
        {
            throw System::NullReferenceException();
        }
        if (buttonInst->size() != 3)
        {
            throw ProgramException();
        }
        _dialogButtons.clear();
        _dialogButtons.emplace_back(DialogButton::Okay, buttonInst->at(0),
            Vector2(128 - 12, 154 + 9), "Okay");
        _dialogButtons.emplace_back(DialogButton::No, buttonInst->at(1),
            Vector2(84 - 8, 154 + 9), "No");
        _dialogButtons.emplace_back(DialogButton::Yes, buttonInst->at(2),
            Vector2(84 - 12, 154 + 9), "Yes");
        _messageBoxInst = std::move(messageBoxInst);
    }

    void PlayerEntity::ShowDialog(
        DialogType type, std::int32_t messageId, std::int32_t value1, std::int32_t value2)
    {
        if (_dialogConfirm)
        {
            return;
        }
        _dialogType = type;
        _dialogId = messageId;
        _dialogButton = DialogButton::None;
        _dialogConfirm = false;
        _dialogConfirmTimer = 0;
        _dialogText = "";
        _dialogBuffer = "";

        if (messageId == Metadata::DialogIds::Overlay)
        {
            if (value1 < 0)
            {
                value1 = std::max(0, ManagedMultiply(-1, value1));
                _dialogTextSpacing = 10;
            }
            else
            {
                _dialogTextSpacing = 12;
            }
            SetOverlayMessage(value1, value2);
        }
        else if (messageId == Metadata::DialogIds::HudMessage)
        {
            if (value1 < 0)
            {
                value1 = std::max(0, ManagedMultiply(-1, value1));
                _dialogTextSpacing = 10;
            }
            else
            {
                _dialogTextSpacing = 12;
            }
            if (RequireReference(_scene).GameMode == MphRead::GameMode::Battle)
            {
                if (value1 == 1)
                {
                    value1 = 2;
                }
                else if (value1 == 2)
                {
                    value1 = 1;
                }
            }
            SetHudMessage(value1, value2);
        }
        else
        {
            return;
        }
        _dialogBuffer = _dialogText;
        _dialogPage = 0;
        _dialogPageCount = 0;
        const std::int32_t length = ManagedStringLength(_dialogText);
        for (std::int32_t i = 0; i < length; i = ManagedIncrement(i))
        {
            if (ManagedCharAt(_dialogText, i) == u'\n' && i < length - 1)
            {
                if (ManagedCharAt(_dialogText, ManagedAdd(i, 1)) == u'\n')
                {
                    i = ManagedIncrement(i);
                    _dialogPageCount = ManagedIncrement(_dialogPageCount);
                }
                else if (ManagedCharAt(_dialogText, ManagedAdd(i, 1)) == u'\r')
                {
                    _dialogPageCount = ManagedIncrement(_dialogPageCount);
                }
            }
        }
        _dialogPageLength = 0;
        _dialogPageLines = 0;
        _dialogPageTimer = 0;
        _dialogTextChars = 0;
        if (messageId == Metadata::DialogIds::GameOver)
        {
            _dialogButton = DialogButton::No;
        }
        else if (messageId == Metadata::DialogIds::ShipHatch)
        {
            _dialogButton = DialogButton::Yes;
        }
    }

    void PlayerEntity::SetOverlayMessage(std::int32_t index, std::int32_t param1)
    {
        Scene& scene = RequireReference(_scene);
        if (scene.Multiplayer)
        {
            if (GameState::Teams)
            {
                if (index == 128 || index == 138)
                {
                    param1 = ManagedAdd(
                        GameState::GetTeamIndex(static_cast<PlayerIndex>(param1)), 4);
                }
            }
            else
            {
                if (index == 130)
                {
                    param1 = ManagedAdd(param1, scene.MainPlayerIndex) % scene.PlayerCount;
                }
            }
            Text::TextItem text = Text::Strings::GetHudMessage(index);
            index = text.SceneId;
            std::string prepend = "";
            if (index == 130)
            {
                prepend = "\\c5";
            }
            else if (index == 131)
            {
                prepend = "\\c0";
            }
            else if (index == 132)
            {
                prepend = "\\c1";
            }
            else if (index == 134)
            {
                prepend = "\\c2";
            }
            else if (index == 133)
            {
                prepend = "\\c3";
            }
            else if (index == 129)
            {
                if (text.Team == 0)
                {
                    prepend = "\\c4";
                }
                else
                {
                    prepend = "\\c6";
                }
            }
            _dialogText = prepend + text.Value1;
            ReplaceAll(_dialogText, "&tab;", "-");
            ReplaceAll(_dialogText, "%d", std::to_string(param1));
            const std::string playerName = RequireReference(scene.MainPlayer).Name();
            ReplaceAll(_dialogText, "%hunter", playerName);
            ReplaceAll(_dialogText, "%rival", playerName);
            ReplaceAll(_dialogText, "%min", std::to_string(param1 / 30 / 60));
            ReplaceAll(_dialogText, "%sec", FormatDecimal2(param1 / 30 % 60));
        }
        else
        {
            const std::string text = Text::Strings::GetHudMessage(index).Value1;
            _dialogText = text;
            ReplaceAll(_dialogText, "%d", std::to_string(param1));
        }
        _dialogText = WrapText(
            _dialogText, _messageBoxMaxWidth, _dialogTextSpacing, _fontSpacing);
    }

    void PlayerEntity::SetHudMessage(std::int32_t index, std::int32_t param1)
    {
        Text::TextItem text = Text::Strings::GetHudMessage(index);
        index = text.SceneId;
        Scene& scene = RequireReference(_scene);
        if (!scene.Multiplayer)
        {
            _dialogText = text.Value1;
            ReplaceAll(_dialogText, "&tab;", "  ");
            ReplaceAll(_dialogText, "%weste", "Lower");
            ReplaceAll(_dialogText, "%oeste", "Upper");
            std::string name;
            if (scene.GameMode == MphRead::GameMode::Adventure)
            {
                name = RequireReference(scene.MainPlayer).Name();
            }
            else
            {
                name = RequireReference(scene.Room).Meta().Name;
            }
            ReplaceAll(_dialogText, "%mapname", name);
        }
        else
        {
            _dialogText = text.Value1;
            ReplaceAll(_dialogText, "&tab;", "  ");
            ReplaceAll(_dialogText, "%d", std::to_string(param1));
            const std::string name = RequireReference(scene.MainPlayer).Name();
            ReplaceAll(_dialogText, "%player", name);
            ReplaceAll(_dialogText, "%hunter", name);
            if (index == 2)
            {
                switch (param1)
                {
                case 0:
                    ReplaceAll(_dialogText, "%weapon", "Power Beam");
                    break;
                case 1:
                    ReplaceAll(_dialogText, "%weapon", "Volt Driver");
                    break;
                case 2:
                    ReplaceAll(_dialogText, "%weapon", "Missile");
                    break;
                case 3:
                    ReplaceAll(_dialogText, "%weapon", "Battlehammer");
                    break;
                case 4:
                    ReplaceAll(_dialogText, "%weapon", "Imperialist");
                    break;
                case 5:
                    ReplaceAll(_dialogText, "%weapon", "Judicator");
                    break;
                case 6:
                    ReplaceAll(_dialogText, "%weapon", "Magmaul");
                    break;
                case 7:
                    ReplaceAll(_dialogText, "%weapon", "Shock Coil");
                    break;
                case 8:
                    ReplaceAll(_dialogText, "%weapon", "Omega Cannon");
                    break;
                }
            }
            else if (index == 3)
            {
                const std::string hunter = Text::Strings::GetHudMessage(param1).Value1;
                ReplaceAll(_dialogText, "%hunter", hunter);
            }
            else if (index == 4)
            {
                auto&& playerValue = ManagedAt(scene.Players, param1);
                PlayerEntity& player = RequireReference(playerValue);
                const std::int32_t hunterIndex = static_cast<std::int32_t>(player.Hunter());
                const auto& message = ManagedAt(PlayerIndexMessages, hunterIndex);
                const std::string hunter
                    = Text::Strings::GetHudMessage(message.first).Value1;
                ReplaceAll(_dialogText, "%hunter", hunter);
            }
            else if (index == 6)
            {
                const Vector3 position = RequireReference(scene.MainPlayer).Position;
                const std::int32_t x = ConvertToInt32Net9(position.X * 4096.0F);
                const std::int32_t y = ConvertToInt32Net9(position.Y * 4096.0F);
                const std::int32_t z = ConvertToInt32Net9(position.Z * 4096.0F);
                const std::string value
                    = "(" + FormatHex8(x) + "," + FormatHex8(y) + "," + FormatHex8(z) + ")";
                ReplaceAll(_dialogText, "%pos", value);
            }
            ReplaceAll(_dialogText, "&tab;", "  ");
            ReplaceAll(_dialogText, "%d", std::to_string(param1));
        }
        _dialogText = WrapText(
            _dialogText, _messageBoxMaxWidth, _dialogTextSpacing, _fontSpacing);
    }

    std::string PlayerEntity::WrapText(
        std::string text, std::int32_t maxWidth,
        std::int32_t spacing, std::int32_t fontSpacing)
    {
        const std::u16string source = Utf8ToUtf16(text);
        std::int32_t offset = 0;
        std::int32_t lineWidth = 0;
        bool colorCode = false;
        std::u16string result;
        for (const char16_t c : source)
        {
            if (c == u'\\')
            {
                colorCode = true;
            }
            else if (colorCode)
            {
                result += c;
                colorCode = false;
                continue;
            }
            else if (c == u' ')
            {
                std::int32_t wordWidth = 0;
                std::int32_t wordChar = ManagedAdd(offset, 1);
                while (wordChar < static_cast<std::int32_t>(source.size())
                    && source[static_cast<std::size_t>(wordChar)] != u' '
                    && source[static_cast<std::size_t>(wordChar)] != u'\n')
                {
                    wordWidth = ManagedAdd(wordWidth, ManagedAdd(spacing, fontSpacing));
                    wordChar = ManagedIncrement(wordChar);
                }
                if (ManagedAdd(lineWidth, wordWidth) > maxWidth)
                {
                    result += u'\n';
                    lineWidth = 0;
                    offset = ManagedIncrement(offset);
                    continue;
                }
            }
            if (c == u'\n')
            {
                lineWidth = 0;
            }
            else
            {
                lineWidth = ManagedAdd(lineWidth, ManagedAdd(spacing, fontSpacing));
            }
            result += c;
            offset = ManagedIncrement(offset);
        }
        return Utf16ToUtf8(result);
    }
}

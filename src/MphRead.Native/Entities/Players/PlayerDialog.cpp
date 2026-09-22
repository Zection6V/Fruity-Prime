#include "PlayerDialog.hpp"

#include "../../GameState.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Formats/Formats.hpp"
#include "../../Program.hpp"
#include "../../Scene.hpp"
#include "../../Sound/Music.hpp"
#include "../../Strings.hpp"
#include "PlayerEntity.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
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

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(const TContainer& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] T& RequireOptional(std::optional<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] const T& RequireOptional(const std::optional<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T, std::size_t N>
    [[nodiscard]] std::span<const T> ManagedSpan(
        const std::array<T, N>& values, std::int32_t start, std::int32_t length)
    {
        if (start < 0 || length < 0
            || static_cast<std::size_t>(start) > values.size()
            || static_cast<std::size_t>(length)
                > values.size() - static_cast<std::size_t>(start))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return std::span<const T>(values.data() + start, static_cast<std::size_t>(length));
    }

    struct Utf8Character final
    {
        char32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] Utf8Character DecodeUtf8(std::string_view text, std::size_t offset) noexcept
    {
        const auto byte = static_cast<unsigned char>(text[offset]);
        if (byte < 0x80)
        {
            return {byte, 1};
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
            return {0xFFFD, 1};
        }
        if (offset + length > text.size())
        {
            return {0xFFFD, 1};
        }
        for (std::size_t i = 1; i < length; ++i)
        {
            const auto continuation = static_cast<unsigned char>(text[offset + i]);
            if ((continuation & 0xC0) != 0x80)
            {
                return {0xFFFD, 1};
            }
            value = (value << 6) | (continuation & 0x3F);
        }
        if (value < minimum || value > 0x10FFFF
            || (value >= 0xD800 && value <= 0xDFFF))
        {
            return {0xFFFD, 1};
        }
        return {value, length};
    }

    [[nodiscard]] std::int32_t ManagedStringLength(const std::string& text) noexcept
    {
        std::int32_t length = 0;
        std::size_t offset = 0;
        while (offset < text.size())
        {
            const Utf8Character character = DecodeUtf8(text, offset);
            length = ManagedAdd(length, character.Value > 0xFFFF ? 2 : 1);
            offset += character.Length;
        }
        return length;
    }

    void ReplaceAll(std::string& value, std::string_view oldValue, const std::string& newValue)
    {
        std::size_t position = 0;
        while ((position = value.find(oldValue, position)) != std::string::npos)
        {
            value.replace(position, oldValue.size(), newValue);
            position += newValue.size();
        }
    }
}

namespace MphRead::Entities
{
    void PlayerEntity::ShowDialog(::MphRead::Entities::DialogType type,
        std::int32_t messageId, std::int32_t param1, std::int32_t param2,
        std::optional<std::string> value1, std::optional<std::string> value2)
    {
        if (!TestFlag(LoadFlags(), ::MphRead::Entities::LoadFlags::Initial)
            || GameState::Mode() != GameMode::SinglePlayer || !IsMainPlayer())
        {
            return;
        }

        _dialogType = type;
        _silentVisorSwitch = false;
        _dialogValue1 = std::move(value1);
        _dialogValue2 = std::move(value2);

        const auto checkPrompt = [this]()
        {
            if (!IsMainPlayer())
            {
                CloseDialogs();
                return false;
            }
            if (ScanVisor() && _dialogType != ::MphRead::Entities::DialogType::Scan)
            {
                _silentVisorSwitch = true;
                SwitchVisors(false);
            }
            return true;
        };

        if (type == ::MphRead::Entities::DialogType::Overlay)
        {
            ShowDialogOverlay(messageId, param1, param2 != 0);
        }
        else if (type == ::MphRead::Entities::DialogType::Hud)
        {
            ShowDialogHud(messageId, param1, param2 != 0);
        }
        else if (type == ::MphRead::Entities::DialogType::Okay
            || type == ::MphRead::Entities::DialogType::YesNo)
        {
            if (checkPrompt())
            {
                ShowDialogPrompt(messageId);
            }
        }
        else if (type == ::MphRead::Entities::DialogType::Scan)
        {
            if (checkPrompt())
            {
                ShowDialogScan();
            }
        }
        else if (type == ::MphRead::Entities::DialogType::Event)
        {
            if (checkPrompt())
            {
                ShowDialogEvent(messageId, static_cast<::MphRead::Entities::EventType>(param1));
            }
        }
        else
        {
            CloseDialogs();
        }
    }

    void PlayerEntity::BufferDialogPages()
    {
        assert(_overlayMessage2.has_value() && !_overlayMessage2->empty());
        std::int32_t maxWidth = 200;
        if (Paths::IsMphJapan() || Paths::IsMphKorea())
        {
            maxWidth = 160;
        }
        WrapText(RequireOptional(_overlayMessage2), maxWidth, _overlayBuffer2, 90);
        std::int32_t index = 0;
        std::int32_t line = 1;
        std::int32_t page = 0;
        std::int32_t length = 0;
        char16_t ch = ManagedAt(_overlayBuffer2, index);
        while (ch != u'\0')
        {
            if (ch == u'\n')
            {
                line = ManagedIncrement(line);
                if (line == 4)
                {
                    ManagedAt(_dialogPageLengths, page) = length;
                    page = ManagedIncrement(page);
                    length = 0;
                    line = 1;
                }
                else
                {
                    length = ManagedIncrement(length);
                }
            }
            else
            {
                length = ManagedIncrement(length);
            }
            index = ManagedIncrement(index);
            if (index == static_cast<std::int32_t>(_overlayBuffer2.size()))
            {
                break;
            }
            ch = ManagedAt(_overlayBuffer2, index);
        }
        ManagedAt(_dialogPageLengths, page) = length;
        _dialogPageCount = ManagedAdd(page, 1);
    }

    void PlayerEntity::ShowDialogOverlay(std::int32_t messageId,
        std::int32_t duration, bool warning)
    {
        if (ScanVisor())
        {
            CloseDialogs();
            return;
        }
        auto entry = Text::Strings::GetEntry('M', messageId, Text::StringTables::GameMessages);
        if (!entry)
        {
            CloseDialogs();
            return;
        }
        if (_overlayMessage1)
        {
            if (RequireOptional(_overlayMessage1) == entry->Value1)
            {
                _overlayTimer = duration / 30.0F;
            }
            return;
        }
        assert(!_overlayMessage2.has_value());
        _overlayMessage1 = entry->Value1;
        _overlayMessage2.reset();
        _dialogValue1.reset();
        _dialogValue2.reset();
        _overlayBuffer1.fill(u'\0');
        const std::int32_t lineCount = WrapText(RequireOptional(_overlayMessage1), 142, _overlayBuffer1);
        _overlayTextOffsetY = static_cast<float>(ManagedMultiply(lineCount, 5));
        _overlayTimer = duration / 30.0F;
        _dialogCharTimer = 0.0F;
        _dialogPalette = warning ? 3 : 0;
        RequireReference(_messageBoxInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageSpacerInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageBoxInst).SetAnimation(0, 65, 66, 65);
    }

    void PlayerEntity::ShowDialogHud(std::int32_t messageId,
        std::int32_t duration, bool unpause)
    {
        auto message = Text::Strings::GetHudMessage(messageId);
        if (!message)
        {
            CloseDialogs();
            return;
        }
        if (_overlayMessage1)
        {
            if (RequireOptional(_overlayMessage1) == RequireOptional(message))
            {
                _overlayTimer = duration / 30.0F;
            }
            return;
        }
        _overlayMessage1 = RequireOptional(message);
        _overlayMessage2.reset();
        _dialogValue1.reset();
        _dialogValue2.reset();
        if (unpause)
        {
            GameState::UnpauseDialog();
        }
        _overlayBuffer1.fill(u'\0');
        const std::int32_t lineCount = WrapText(RequireOptional(_overlayMessage1), 142, _overlayBuffer1);
        _overlayTextOffsetY = static_cast<float>(ManagedMultiply(lineCount, 5));
        _overlayTimer = duration / 30.0F;
        _dialogCharTimer = 9999.0F;
        _prevScrollingChars = 9999;
        _dialogPalette = 2;
        RequireReference(_messageBoxInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageSpacerInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageBoxInst).SetAnimation(0, 65, 66, 65);
    }

    void PlayerEntity::ShowDialogPrompt(std::int32_t messageId)
    {
        auto entry = Text::Strings::GetEntry('M', messageId, Text::StringTables::GameMessages);
        if (!entry)
        {
            CloseDialogs();
            return;
        }
        StopLongSfx();
        EndWeaponMenu();
        if (entry->Prefix == 'G')
        {
            RequireReference(_soundSource).PlayFreeSfx(SfxId::GUNSHIP_TRANSMISSION);
        }
        else if (entry->Prefix == 'H')
        {
            RequireReference(_soundSource).PlayFreeSfx(SfxId::GAME_HINT);
        }
        else if (entry->Prefix == 'T')
        {
            RequireReference(_soundSource).StopFreeSfxScripts();
            RequireReference(_soundSource).PlayFreeSfx(SfxId::TELEPATHIC_MESSAGE);
        }
        else if (entry->Prefix == 'B')
        {
            RequireReference(_soundSource).PlayFreeSfx(SfxId::GUNSHIP_TRANSMISSION);
            RequireReference(_soundSource).StopFreeSfxScripts();
            RequireReference(_soundSource).PlayFreeSfx(SfxId::TELEPATHIC_MESSAGE);
        }
        GameState::PauseDialog();
        _overlayMessage1 = entry->Value1;
        _overlayMessage2 = entry->Value2;
        _overlayBuffer1.fill(u'\0');
        _overlayBuffer2.fill(u'\0');
        const std::int32_t lineCount = WrapText(RequireOptional(_overlayMessage1), 142, _overlayBuffer1);
        _overlayTextOffsetY = static_cast<float>(ManagedMultiply(lineCount, 5));
        BufferDialogPages();
        _dialogCharTimer = 0.0F;
        _dialogPalette = 0;
        RequireReference(_messageBoxInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageSpacerInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageBoxInst).SetAnimation(0, 65, 66, 65);
    }

    void PlayerEntity::ShowDialogScan()
    {
        StopLongSfx();
        EndWeaponMenu();
        GameState::PauseDialog();
        _dialogCharTimer = 0.0F;
        _dialogPalette = 0;
        RequireReference(_messageBoxInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageSpacerInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageBoxInst).SetAnimation(0, 65, 66, 65);
    }

    void PlayerEntity::ShowDialogEvent(std::int32_t messageId,
        ::MphRead::Entities::EventType eventType)
    {
        auto entry = Text::Strings::GetEntry('M', messageId, Text::StringTables::GameMessages);
        if (!entry)
        {
            CloseDialogs();
            return;
        }
        StopLongSfx();
        EndWeaponMenu();
        GameState::PausePrevented(true);
        GameState::PauseDialog();
        _eventType = eventType;
        _overlayMessage1 = entry->Value1;
        _overlayMessage2 = entry->Value2;
        if (_dialogValue1)
        {
            ReplaceAll(RequireOptional(_overlayMessage1), "&tab0", RequireOptional(_dialogValue1));
            ReplaceAll(RequireOptional(_overlayMessage2), "&tab0", RequireOptional(_dialogValue1));
        }
        if (_dialogValue2)
        {
            ReplaceAll(RequireOptional(_overlayMessage1), "&tab1", RequireOptional(_dialogValue2));
            ReplaceAll(RequireOptional(_overlayMessage2), "&tab1", RequireOptional(_dialogValue2));
        }
        _overlayBuffer1.fill(u'\0');
        _overlayBuffer2.fill(u'\0');
        const std::int32_t lineCount = WrapText(RequireOptional(_overlayMessage1), 142, _overlayBuffer1);
        _overlayTextOffsetY = static_cast<float>(ManagedMultiply(lineCount, 5));
        BufferDialogPages();
        _dialogCharTimer = 0.0F;
        _dialogPalette = 0;
        RequireReference(_messageBoxInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageSpacerInst).SetPalette(_dialogPalette, RequireReference(_scene));
        RequireReference(_messageBoxInst).SetAnimation(0, 65, 66, 65);

        if (_eventType == ::MphRead::Entities::EventType::EnergyTank
            || _eventType == ::MphRead::Entities::EventType::MissileTank
            || _eventType == ::MphRead::Entities::EventType::UATank)
        {
            Music::FadeVolume(50.0F / 127.0F, 5.0F / 30.0F);
            RequireReference(_soundSource).PlayFreeSfx(SfxId::GET_ITEM);
            _dialogConfirmTimer = 60.0F / 30.0F;
            if (_eventType != ::MphRead::Entities::EventType::UATank)
            {
                RequireReference(_dialogPickupInst).SetIndex(
                    static_cast<std::int32_t>(_eventType), RequireReference(_scene));
            }
        }
        else if (_eventType >= ::MphRead::Entities::EventType::VoltDriver
            && _eventType <= ::MphRead::Entities::EventType::ShockCoil)
        {
            Music::Pause();
            Music::PlaySeq(SeqId::GET_WEAPON);
            RequireReference(_soundSource).PlayFreeSfx(SfxId::WEAPON_POWER_UP);
            _dialogConfirmTimer = 150.0F / 30.0F;
            RequireReference(_dialogPickupInst).SetIndex(
                static_cast<std::int32_t>(_eventType), RequireReference(_scene));
        }
        else if (_eventType == ::MphRead::Entities::EventType::OmegaCannon
            || _eventType == ::MphRead::Entities::EventType::Artifact)
        {
            Music::FadeVolume(50.0F / 127.0F, 5.0F / 30.0F);
            RequireReference(_soundSource).PlayFreeSfx(SfxId::GET_ITEM2);
            _dialogConfirmTimer = 60.0F / 30.0F;
            if (_eventType == ::MphRead::Entities::EventType::OmegaCannon)
            {
                RequireReference(_dialogPickupInst).SetIndex(
                    static_cast<std::int32_t>(_eventType), RequireReference(_scene));
            }
        }
        else if (_eventType == ::MphRead::Entities::EventType::Octolith)
        {
            Music::Pause();
            Music::PlaySeq(SeqId::GET_OCTOLITH);
            RequireReference(_dialogCrystalInst).SetAnimation(0, 35, 36, true);
            _dialogConfirmTimer = 150.0F / 30.0F;
        }
    }

    void PlayerEntity::CloseDialogs()
    {
        if (!TestFlag(LoadFlags(), ::MphRead::Entities::LoadFlags::Initial)
            || GameState::Mode() != GameMode::SinglePlayer || !IsMainPlayer())
        {
            return;
        }

        _dialogType = ::MphRead::Entities::DialogType::None;
        _overlayTimer = 0.0F;
        _dialogCharTimer = 0.0F;
        _dialogValue1.reset();
        _dialogValue2.reset();
        _overlayMessage1.reset();
        _overlayMessage2.reset();
        _overlayTextOffsetY = 0.0F;
        _prevOverlayCharacters = 0;
        _showDialogConfirm = false;
        _dialogConfirmTimer = 0.0F;
        _lastDialogPageSeen = false;
        _dialogPageCount = 0;
        _dialogPageIndex = 0;
        _dialogPageLengths.fill(0);
        if (_silentVisorSwitch)
        {
            SwitchVisors(false);
        }
        _silentVisorSwitch = false;
        RequireReference(_messageBoxInst).SetIndex(0, RequireReference(_scene));
        RequireReference(_dialogButtonInst).SetIndex(0, RequireReference(_scene));
        RequireReference(_dialogArrowInst).SetIndex(0, RequireReference(_scene));
        RequireReference(_dialogCrystalInst).SetIndex(0, RequireReference(_scene));
        RequireReference(_scene).Layer5Info.BindingId = -1;
    }

    void PlayerEntity::UpdateDialogs()
    {
        auto& scene = RequireReference(_scene);
        auto& messageBox = RequireReference(_messageBoxInst);
        if (!ScanVisor() || _dialogType == ::MphRead::Entities::DialogType::Scan)
        {
            if (_dialogType == ::MphRead::Entities::DialogType::Overlay
                || _dialogType == ::MphRead::Entities::DialogType::Hud)
            {
                _overlayTimer -= scene.FrameTime;
                if (_overlayTimer <= 0.0F)
                {
                    CloseDialogs();
                    return;
                }
            }
            messageBox.ProcessAnimation(scene);
            if (messageBox.Time - messageBox.Timer >= 16.0F / 30.0F)
            {
                _dialogCharTimer += scene.FrameTime;
            }
            if (messageBox.CurrentFrame >= 5)
            {
                const std::int32_t spacerIndex = (messageBox.CurrentFrame & 1) != 0 ? 0 : 1;
                RequireReference(_messageSpacerInst).SetIndex(spacerIndex, scene);
            }
        }
        if (GameState::DialogPause()
            && (_dialogType == ::MphRead::Entities::DialogType::Scan
                || messageBox.Time - messageBox.Timer >= 16.0F / 30.0F))
        {
            bool closed = false;
            if (_showDialogConfirm)
            {
                if (_dialogType == ::MphRead::Entities::DialogType::YesNo)
                {
                    if (CheckButtonPressed(DialogButton::Yes))
                    {
                        closed = true;
                        _dialogConfirmState = ::MphRead::Entities::ConfirmState::Yes;
                        if (_dialogPromptType != ::MphRead::Entities::PromptType::ShipHatch
                            && _dialogPromptType != ::MphRead::Entities::PromptType::GameOver)
                        {
                            CloseDialogs();
                        }
                    }
                    else if (CheckButtonPressed(DialogButton::No))
                    {
                        closed = true;
                        _dialogConfirmState = ::MphRead::Entities::ConfirmState::No;
                        if (_dialogPromptType != ::MphRead::Entities::PromptType::GameOver)
                        {
                            CloseDialogs();
                        }
                    }
                }
                else if (CheckButtonPressed(DialogButton::Okay))
                {
                    closed = true;
                    if (_dialogType == ::MphRead::Entities::DialogType::Event)
                    {
                        if (Music::IsPaused())
                        {
                            Music::PlayPausedMusic();
                        }
                        else
                        {
                            Music::FadeVolume(1.0F, 5.0F / 30.0F);
                        }
                        RestartLongSfx();
                        GameState::PausePrevented(false);
                    }
                    else if (GameState::DialogPause())
                    {
                        RestartLongSfx();
                    }
                    const bool scan = _dialogType == ::MphRead::Entities::DialogType::Scan;
                    RequireReference(_soundSource).PlayFreeSfx(SfxId::SCAN_OK);
                    CloseDialogs();
                    _dialogConfirmState = ::MphRead::Entities::ConfirmState::Okay;
                    GameState::UnpauseDialog();
                    if (scan)
                    {
                        AfterScan();
                    }
                }
            }
            if (closed)
            {
                _ignoreClick = true;
            }
            else
            {
                if (CheckButtonPressed(DialogButton::Right))
                {
                    if (_dialogPageIndex != ManagedSubtract(_dialogPageCount, 1))
                    {
                        RequireReference(_soundSource).PlayFreeSfx(SfxId::SCAN_SCROLL_BUTTONS);
                        _dialogPageIndex = ManagedIncrement(_dialogPageIndex);
                    }
                }
                else if (CheckButtonPressed(DialogButton::Left))
                {
                    if (_dialogPageIndex != 0)
                    {
                        RequireReference(_soundSource).PlayFreeSfx(SfxId::SCAN_SCROLL_BUTTONS);
                        _dialogPageIndex = ManagedDecrement(_dialogPageIndex);
                    }
                }
                if (_dialogConfirmTimer > 0.0F)
                {
                    _dialogConfirmTimer -= scene.FrameTime;
                }
                if (_dialogPageIndex == ManagedSubtract(_dialogPageCount, 1))
                {
                    _lastDialogPageSeen = true;
                }
                if (_dialogConfirmTimer <= 0.0F && _lastDialogPageSeen)
                {
                    if (!_showDialogConfirm)
                    {
                        RequireReference(_dialogButtonInst).SetAnimation(0, 2, 3, 2);
                        RequireReference(_dialogArrowInst).SetIndex(0, scene);
                    }
                    _showDialogConfirm = true;
                }
                if (!_showDialogConfirm && RequireReference(_dialogArrowInst).Timer <= 0.0F)
                {
                    RequireReference(_dialogArrowInst).SetAnimation(0, 29, 30, 29, true);
                }
                RequireReference(_dialogButtonInst).ProcessAnimation(scene);
                RequireReference(_dialogArrowInst).ProcessAnimation(scene);
            }
            if (_dialogType == ::MphRead::Entities::DialogType::Event
                && messageBox.Timer <= 0.0F)
            {
                RequireReference(_dialogCrystalInst).ProcessAnimation(scene);
            }
        }
    }

    void PlayerEntity::DrawDialogs()
    {
        auto& scene = RequireReference(_scene);
        auto& messageBox = RequireReference(_messageBoxInst);
        const float baseY = _dialogType == ::MphRead::Entities::DialogType::Event
            ? 27.0F : 47.0F;
        if (!ScanVisor() && _overlayMessage1)
        {
            const float posX = 64.0F / 256.0F;
            const float posY = baseY / 192.0F;
            const float width = static_cast<float>(messageBox.Width) / 256.0F;
            const float height = static_cast<float>(messageBox.Height) / 192.0F;
            float spacerOffset = 0.0F;
            if (messageBox.CurrentFrame >= 5)
            {
                spacerOffset = 16.0F / 256.0F;
                auto& spacer = RequireReference(_messageSpacerInst);
                spacer.Alpha = 0.5F;
                spacer.PositionX = posX + width - spacerOffset;
                spacer.PositionY = posY;
                spacer.FlipVertical = false;
                scene.DrawHudObject(_messageSpacerInst, 1);
                spacer.PositionY = posY + height;
                spacer.FlipVertical = true;
                scene.DrawHudObject(_messageSpacerInst, 1);
            }
            const float leftPos = posX - spacerOffset;
            const float rightPos = posX + spacerOffset + width;
            const float topPos = posY;
            const float bottomPos = posY + height;
            messageBox.Alpha = 0.5F;
            messageBox.PositionX = leftPos;
            messageBox.PositionY = topPos;
            messageBox.FlipHorizontal = false;
            messageBox.FlipVertical = false;
            scene.DrawHudObject(_messageBoxInst, 1);
            messageBox.PositionX = rightPos;
            messageBox.PositionY = topPos;
            messageBox.FlipHorizontal = true;
            messageBox.FlipVertical = false;
            scene.DrawHudObject(_messageBoxInst, 1);
            messageBox.PositionX = leftPos;
            messageBox.PositionY = bottomPos;
            messageBox.FlipHorizontal = false;
            messageBox.FlipVertical = true;
            scene.DrawHudObject(_messageBoxInst, 1);
            messageBox.PositionX = rightPos;
            messageBox.PositionY = bottomPos;
            messageBox.FlipHorizontal = true;
            messageBox.FlipVertical = true;
            scene.DrawHudObject(_messageBoxInst, 1);
            if (messageBox.Time - messageBox.Timer >= 16.0F / 30.0F)
            {
                _textSpacingY = 10;
                RequireReference(_textInst).SetPaletteData(_dialogPaletteData, scene);
                const std::int32_t characters
                    = ConvertToInt32Net9(_dialogCharTimer / (1.0F / 30.0F));
                const std::int32_t offset = 17
                    + (Paths::IsMphJapan() || Paths::IsMphKorea() ? 8 : 17);
                DrawText2D(128.0F, baseY + static_cast<float>(offset) - _overlayTextOffsetY,
                    Hud::Align::PadCenter, _dialogPalette, _overlayBuffer1, characters);
                RequireReference(_textInst).SetPaletteData(_textPaletteData, scene);
                _textSpacingY = 0;
                if (characters > _prevOverlayCharacters
                    && characters <= ManagedStringLength(RequireOptional(_overlayMessage1)))
                {
                    RequireReference(_soundSource).StopFreeSfx(SfxId::LETTER_BLIP);
                    RequireReference(_soundSource).PlayFreeSfx(SfxId::LETTER_BLIP);
                    _prevOverlayCharacters = characters;
                }
            }
        }
        if (_dialogType == ::MphRead::Entities::DialogType::Event
            && messageBox.Timer <= 0.0F
            && messageBox.Time - messageBox.Timer >= 16.0F / 30.0F)
        {
            if (_eventType <= ::MphRead::Entities::EventType::OmegaCannon
                || _eventType == ::MphRead::Entities::EventType::Octolith
                || _eventType == ::MphRead::Entities::EventType::UATank)
            {
                auto& frame = RequireReference(_dialogFrameInst);
                const std::int32_t posXInt = ManagedSubtract(
                    ManagedAdd(64, messageBox.Width), frame.Width / 2);
                const float posX = static_cast<float>(posXInt);
                const float posY = baseY
                    + static_cast<float>(ManagedMultiply(2, messageBox.Height)) - 12.0F;
                frame.Alpha = 0.5F;
                frame.PositionX = posX / 256.0F;
                frame.PositionY = posY / 192.0F;
                if (_eventType == ::MphRead::Entities::EventType::Octolith)
                {
                    scene.DrawHudObject(_dialogFrameInst);
                    auto& crystal = RequireReference(_dialogCrystalInst);
                    crystal.PositionX = (posX + 16.0F) / 256.0F;
                    crystal.PositionY = posY / 192.0F;
                    scene.DrawHudObject(_dialogCrystalInst);
                }
                else if (_eventType != ::MphRead::Entities::EventType::UATank)
                {
                    scene.DrawHudObject(_dialogFrameInst);
                    auto& pickup = RequireReference(_dialogPickupInst);
                    pickup.PositionX = (posX + 16.0F) / 256.0F;
                    pickup.PositionY = (posY + 16.0F) / 192.0F;
                    scene.DrawHudObject(_dialogPickupInst);
                }
            }
        }

        std::int32_t layerIndex = 4;
        std::int32_t scanYOffset = 0;
        if (Paths::IsMphJapan())
        {
            scanYOffset = -4;
        }
        else if (Paths::IsMphKorea())
        {
            scanYOffset = -6;
        }
        if (_dialogType == ::MphRead::Entities::DialogType::Scan)
        {
            assert(_overlayMessage1.has_value());
            auto text = Text::Strings::GetHudMessage(102);
            DrawText2D(128.0F + _objShiftX, 58.0F + _objShiftY,
                Hud::Align::Center, 0, RequireOptional(text));
            auto iconInst = ManagedAt(_scanIconInsts, ManagedMultiply(_scanCategoryIndex, 2));
            auto& icon = RequireReference(iconInst);
            icon.PositionX = 20.0F / 256.0F;
            icon.PositionY = 96.0F / 192.0F;
            icon.Center = false;
            icon.Alpha = 1.0F;
            icon.UseMask = false;
            scene.DrawHudObject(iconInst);
            assert(icon.PaletteData != nullptr);
            RequireReference(_textInst).SetPaletteData(icon.PaletteData, scene);
            DrawText2D(58.0F, 116.0F + static_cast<float>(scanYOffset),
                Hud::Align::Left, 0, RequireOptional(_overlayMessage1));
            RequireReference(_textInst).SetPaletteData(_textPaletteData, scene);
            layerIndex = ManagedAt(_scanCategoryLayers, _scanCategoryIndex);
        }
        if (((_dialogType == ::MphRead::Entities::DialogType::Okay
                || _dialogType == ::MphRead::Entities::DialogType::Event
                || _dialogType == ::MphRead::Entities::DialogType::YesNo)
                && messageBox.Time - messageBox.Timer >= 16.0F / 30.0F)
            || _dialogType == ::MphRead::Entities::DialogType::Scan)
        {
            std::int32_t start = 0;
            for (std::int32_t i = 1; i <= _dialogPageIndex; ++i)
            {
                start = ManagedAdd(start, ManagedAt(_dialogPageLengths, ManagedSubtract(i, 1)));
            }
            start = ManagedAdd(start, _dialogPageIndex);
            const auto text = ManagedSpan(_overlayBuffer2, start,
                ManagedAt(_dialogPageLengths, _dialogPageIndex));
            RequireReference(_textInst).SetPaletteData(_dialogPaletteData, scene);
            DrawText2D(128.0F, 134.0F + static_cast<float>(scanYOffset),
                Hud::Align::Center, 0, text);
            RequireReference(_textInst).SetPaletteData(_textPaletteData, scene);
            scene.Layer5Info.BindingId = ManagedAt(_dialogBindingIds, layerIndex);
            scene.Layer5Info.Alpha = 1.0F;
            scene.Layer5Info.ScaleX = 1.0F;
            scene.Layer5Info.ScaleY = 1.0F;
            if (_dialogPageIndex != ManagedSubtract(_dialogPageCount, 1))
            {
                auto& arrow = RequireReference(_dialogArrowInst);
                arrow.PositionX = 169.0F / 256.0F;
                arrow.PositionY = 173.0F / 192.0F;
                arrow.FlipHorizontal = false;
                scene.DrawHudObject(_dialogArrowInst);
            }
            if (_dialogPageIndex != 0)
            {
                auto& arrow = RequireReference(_dialogArrowInst);
                arrow.PositionX = 55.0F / 256.0F;
                arrow.PositionY = 173.0F / 192.0F;
                arrow.FlipHorizontal = true;
                scene.DrawHudObject(_dialogArrowInst);
            }
            if (_showDialogConfirm)
            {
                DrawDialogConfirmButtons(_dialogType);
            }
        }
    }

    void PlayerEntity::DrawDialogConfirmButtons(::MphRead::Entities::DialogType dialogType)
    {
        auto& scene = RequireReference(_scene);
        auto& button = RequireReference(_dialogButtonInst);
        const float posX = 112.0F;
        const float posY = 174.0F;
        if (dialogType == ::MphRead::Entities::DialogType::YesNo)
        {
            button.PositionX = (posX - static_cast<float>(button.Width)) / 256.0F;
            button.PositionY = posY / 192.0F;
            scene.DrawHudObject(_dialogButtonInst);
            button.PositionX = (posX + static_cast<float>(button.Width)) / 256.0F;
            button.PositionY = posY / 192.0F;
            scene.DrawHudObject(_dialogButtonInst);
            auto text = Text::Strings::GetHudMessage(105);
            float textPosX = posX - static_cast<float>(button.Width / 2);
            DrawText2D(textPosX, posY + 5.0F, Hud::Align::Center, 0, RequireOptional(text));
            text = Text::Strings::GetHudMessage(106);
            textPosX = posX + static_cast<float>(button.Width) * 1.5F + 1.0F;
            DrawText2D(textPosX, posY + 5.0F, Hud::Align::Center, 0, RequireOptional(text));
        }
        else
        {
            button.PositionX = posX / 256.0F;
            button.PositionY = posY / 192.0F;
            scene.DrawHudObject(_dialogButtonInst);
            auto text = Text::Strings::GetHudMessage(104);
            DrawText2D(posX + static_cast<float>(button.Width / 2) + 1.0F,
                posY + 5.0F, Hud::Align::Center, 0, RequireOptional(text));
        }
    }

    bool PlayerEntity::CheckButtonPressed(DialogButton type)
    {
        if (Input::ClickX() >= 0.0F && Input::ClickY() >= 0.0F)
        {
            const auto& scene = RequireReference(_scene);
            const float clickX = Input::ClickX() / scene.Size.X;
            const float clickY = Input::ClickY() / scene.Size.Y;
            const ButtonInfo info = ManagedAt(_buttonInfo, static_cast<std::int32_t>(type));
            if (clickX >= info.Left && clickX < info.Right
                && clickY >= info.Top && clickY < info.Bottom)
            {
                return true;
            }
        }
        return false;
    }
}

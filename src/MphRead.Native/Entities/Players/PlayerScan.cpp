#include "PlayerScan.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Formats/Formats.hpp"
#include "../../GameState.hpp"
#include "../../Scene.hpp"
#include "../../Strings.hpp"
#include "../EnemyInstanceEntity.hpp"
#include "PlayerEntity.hpp"
#include "PlayerHud.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
    using MphRead::Entities::EntityBase;
    using MphRead::Entities::EnemyFlags;
    using MphRead::Entities::EnemyInstanceEntity;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;

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

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range(
                "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(const TContainer& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range(
                "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename TTarget, typename TSource>
    [[nodiscard]] TTarget& ManagedCast(const std::shared_ptr<TSource>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        auto* cast = dynamic_cast<TTarget*>(value.get());
        if (cast == nullptr)
        {
            throw MphRead::SceneDetail::InvalidCastException();
        }
        return *cast;
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

    [[nodiscard]] constexpr std::int32_t ManagedMultiply(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t value
            = static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(value);
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

    [[nodiscard]] constexpr float ClampFloat(float value, float minimum, float maximum) noexcept
    {
        if (value < minimum)
        {
            return minimum;
        }
        if (value > maximum)
        {
            return maximum;
        }
        return value;
    }
}

namespace MphRead::Entities
{
    void PlayerEntity::SetCombatVisor()
    {
        if (GameState::SinglePlayer() && ScanVisor())
        {
            SwitchVisors(false);
        }
    }

    void PlayerEntity::ResetCombatVisor()
    {
        if (GameState::SinglePlayer() && ScanVisor())
        {
            SwitchVisors(true);
        }
    }

    void PlayerEntity::SwitchVisors(bool reset)
    {
        _visorMessageTimer = _visorMessageTime = 30.0F / 30.0F;
        UpdateScanSfx(-1, false);
        if (_scanVisor)
        {
            if (!reset && !_silentVisorSwitch)
            {
                _soundSource.PlayFreeSfx(SfxId::SCAN_VISOR_OFF);
            }
            _scanVisor = false;
            _visorMessageId = 108;
            _scanning = false;
            _smallReticle = false;
            _smallReticleTimer = 0;
            ResetScanValues();
        }
        else if (!reset)
        {
            UpdateScanSfx(2, true);
            if (!_silentVisorSwitch)
            {
                _soundSource.PlayFreeSfx(SfxId::SCAN_VISOR_ON2);
            }
            _scanVisor = true;
            _visorMessageId = 107;
            _scanning = false;
            _smallReticle = false;
            _smallReticleTimer = 0;
        }
    }

    void PlayerEntity::ResetScanValues()
    {
        _scanComplete = false;
        _scanningTime = 0.0F;
        _scanningTimer = 0.0F;
        _scanningEntity.reset();
        ScanTarget& current = RequireReference(_curScanTarget);
        current.Entity.reset();
        current.CenterDist = std::numeric_limits<float>::max();
        UpdateScanSfx(1, false);
    }

    void PlayerEntity::UpdateScanHud()
    {
        ScanTarget& current = RequireReference(_curScanTarget);
        std::shared_ptr<EntityBase> curEnt = current.Entity;
        Vector3 curTargetPos = Vector3::Zero;
        float curScreenX = 0.0F;
        float curScreenY = 0.0F;
        float minCenter = std::numeric_limits<float>::max();
        _scanTargetCount = 0;
        bool update = false;

        auto enumerator = RequireReference(_scene).Entities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<EntityBase> entityRef = enumerator.Current();
            EntityBase& entity = RequireReference(entityRef);
            const std::int32_t scanId = entity.GetScanId();
            if (scanId == 0 || !entity.ScanVisible())
            {
                continue;
            }
            const std::int32_t category = Text::Strings::GetScanEntryCategory(scanId);
            if (category == 5)
            {
                continue;
            }

            Vector3 entPos{};
            entity.GetPosition(entPos);
            float dist = 0.0F;
            float depth = 0.0F;
            float scaleInv = 0.0F;
            Vector3 targetPos{};
            Vector2 distPos{};
            const Vector3 cameraPosition = RequireReference(CameraInfo()).Position;
            const auto viewMatrix = RequireReference(_scene).ViewMatrix;
            const auto perspectiveMatrix = RequireReference(_scene).PerspectiveMatrix;
            Matrix::GetProjectedValues(entPos, cameraPosition, viewMatrix, perspectiveMatrix,
                dist, depth, scaleInv, targetPos, distPos);
            const Vector2 screenPos((distPos.X + 1.0F) / 2.0F, (1.0F - distPos.Y) / 2.0F);
            if (depth < 0.0F)
            {
                continue;
            }

            bool noMaxDist = false;
            if (entity.Type == EntityType::EnemyInstance)
            {
                EnemyInstanceEntity& enemy = ManagedCast<EnemyInstanceEntity>(entityRef);
                noMaxDist = TestFlag(static_cast<EnemyFlags>(enemy.Flags), EnemyFlags::NoMaxDistance);
            }
            if (dist >= 24.0F && !noMaxDist)
            {
                continue;
            }

            const std::int32_t pixelX = ConvertToInt32Net9(std::floor(screenPos.X * 256.0F));
            const std::int32_t pixelY = ConvertToInt32Net9(std::floor(screenPos.Y * 192.0F));
            if (pixelX <= -16 || pixelX >= 272 || pixelY <= -16 || pixelY >= 208)
            {
                continue;
            }

            ScanTarget& target = RequireReference(ManagedAt(_scanTargets, _scanTargetCount));
            target.Category = category;
            target.Entity = entityRef;
            target.Scale = scaleInv;
            target.Position = targetPos;
            target.ScreenX = screenPos.X;
            target.ScreenY = screenPos.Y;
            target.Dim = RequireReference(GameState::StorySave()).CheckLogbook(scanId);
            target.Distance = dist;
            if (pixelX > 58 && pixelX < 198 && pixelY > 64 && pixelY < 128)
            {
                const float center = 4.0F * (distPos.X * distPos.X + distPos.Y * distPos.Y) + dist / 32.0F;
                if (center < minCenter && !_scanning)
                {
                    curTargetPos = targetPos;
                    curEnt = entityRef;
                    minCenter = center;
                }
                if (current.Entity == entityRef)
                {
                    current.Distance = dist;
                    current.Scale = scaleInv;
                    current.Position = targetPos;
                    current.CenterDist = center;
                    current.Dim = target.Dim;
                    curScreenX = screenPos.X;
                    curScreenY = screenPos.Y;
                    update = true;
                }
            }
            ++_scanTargetCount;
            if (_scanTargetCount == static_cast<std::int32_t>(_scanTargets.size()))
            {
                break;
            }
        }

        const std::shared_ptr<EntityBase> prevEnt = current.Entity;
        if (current.Entity != curEnt
            && (!current.Entity || minCenter <= current.CenterDist))
        {
            current.Entity = curEnt;
            current.Position = curTargetPos;
            current.CenterDist = minCenter;
            update = true;
        }
        if (current.Entity != prevEnt)
        {
            if (!prevEnt)
            {
                _boxSizeFac = 3.0F;
                _boxCornerX = 0.5F;
                _boxCornerY = 0.5F;
            }
            _boxCornerFac = 0.125F;
        }
        if (current.Entity)
        {
            Formats::CollisionResult discard{};
            if (!update)
            {
                current.Entity.reset();
            }
            else
            {
                const Vector3 cameraPosition = RequireReference(CameraInfo()).Position;
                if (Formats::CollisionDetection::CheckBetweenPoints(
                        cameraPosition, current.Position, Formats::TestFlags::Scan, _scene, discard))
                {
                    current.Entity.reset();
                }
            }
        }

        const float scale = RequireReference(_scene).FrameTime / (1.0F / 30.0F);
        if (current.Entity)
        {
            if (_boxCornerFac < 1.0F)
            {
                _boxCornerFac += 0.0625F * scale;
                if (_boxCornerFac > 1.0F)
                {
                    _boxCornerFac = 1.0F;
                }
            }
            if (_boxSizeFac > 1.0F)
            {
                _boxSizeFac -= 0.25F * scale;
                if (_boxSizeFac < 1.0F)
                {
                    _boxSizeFac = 1.0F;
                }
            }
            _boxCornerX += (curScreenX - _boxCornerX) * _boxCornerFac;
            _boxCornerY += (curScreenY - _boxCornerY) * _boxCornerFac;
        }
        else
        {
            if (_boxCornerFac > 0.125F)
            {
                _boxCornerFac -= 0.0625F * scale;
                if (_boxCornerFac < 0.125F)
                {
                    _boxCornerFac = 0.125F;
                }
            }
            if (_boxSizeFac < 4.0F)
            {
                _boxSizeFac += 1.0F * scale;
                if (_boxSizeFac > 4.0F)
                {
                    _boxSizeFac = 4.0F;
                }
            }
        }
    }

    void PlayerEntity::UpdateScanning(bool scanning)
    {
        if (!scanning)
        {
            UpdateScanSfx(1, false);
            _scanning = false;
        }
        else if (RequireReference(_curScanTarget).Entity)
        {
            if (RequireReference(_curScanTarget).Distance < 12.0F)
            {
                _scanning = true;
            }
            else if (_visorMessageTimer == 0.0F)
            {
                _soundSource.PlayFreeSfx(SfxId::SCAN_OUT_OF_RANGE);
                _visorMessageId = 118;
                _visorMessageTimer = _visorMessageTime = 60.0F / 30.0F;
            }
        }
    }

    void PlayerEntity::UpdateScanState()
    {
        if (GameState::DialogPause())
        {
            return;
        }

        ScanTarget& current = RequireReference(_curScanTarget);
        if (!current.Entity)
        {
            UpdateScanSfx(1, false);
            _scanning = false;
        }
        std::shared_ptr<EntityBase> curEnt = current.Entity;
        if (curEnt && curEnt != _scanningEntity)
        {
            ResetScanValues();
            _scanningEntity = curEnt;
            current.Entity = curEnt;
            UpdateScanSfx(1, false);
            _scanning = false;
            const std::int32_t scanId = RequireReference(_scanningEntity).GetScanId();
            _scanningTime = Text::Strings::GetScanEntryTime(scanId);
        }

        if (_scanning && _scanningTimer < _scanningTime)
        {
            assert(current.Entity != nullptr);
            auto storySave = GameState::StorySave();
            const std::int32_t scanId = RequireReference(current.Entity).GetScanId();
            if (RequireReference(storySave).CheckLogbook(scanId))
            {
                UpdateScanSfx(1, false);
                _scanningTimer = _scanningTime;
                _showDialogConfirm = true;
            }
            else
            {
                UpdateScanSfx(1, true);
                _scanningTimer += RequireReference(_scene).FrameTime;
            }
        }
        else if (_scanning && _scanningTimer >= _scanningTime
            && !_scanComplete && current.Entity)
        {
            UpdateScanSfx(1, false);
            _soundSource.PlayFreeSfx(SfxId::SCAN_COMPLETE);
            StopLongSfx();
            _scanComplete = true;
        }

        if (_scanComplete)
        {
            assert(current.Entity != nullptr);
            _scanning = false;
            const std::int32_t scanId = RequireReference(current.Entity).GetScanId();
            std::shared_ptr<StringTableEntry> entry = Text::Strings::GetScanEntry(scanId);
            if (!entry)
            {
                entry = Text::Strings::EmptyScanEntry;
            }
            _scanCategoryIndex = Text::Strings::GetScanEntryCategory(scanId);
            StringTableEntry& entryRef = RequireReference(entry);
            _overlayMessage1 = entryRef.Value1;
            _overlayMessage2 = entryRef.Value2;
            _overlayBuffer1.fill(u'\0');
            _overlayBuffer2.fill(u'\0');
            [[maybe_unused]] const std::int32_t lineCount
                = WrapText(RequireOptional(_overlayMessage1), 256, _overlayBuffer1);
            BufferDialogPages();
            ShowDialog(DialogType::Scan, 0);
        }
    }

    void PlayerEntity::AfterScan()
    {
        assert(_scanningEntity != nullptr);
        RequireReference(_scanningEntity).OnScanned();
        const std::int32_t scanId = RequireReference(_scanningEntity).GetScanId();
        const std::int32_t altScanId = RequireReference(_scanningEntity).GetScanId(true);
        RequireReference(GameState::StorySave()).UpdateLogbook(scanId);
        if (altScanId != scanId)
        {
            RequireReference(GameState::StorySave()).UpdateLogbook(altScanId);
        }
        RestartLongSfx();
        ResetScanValues();
    }

    void PlayerEntity::DrawScanModels()
    {
        for (std::int32_t i = 0; i < _scanTargetCount; ++i)
        {
            ScanTarget& target = RequireReference(ManagedAt(_scanTargets, i));
            const float iconScale = target.Scale * 90.0F;
            if (target.Entity != RequireReference(_curScanTarget).Entity || iconScale <= 14.0F)
            {
                float particleScale = 0.625F;
                const float value = Fixed::ToFloat(820);
                if (target.Scale > value)
                {
                    const float inv = value / target.Scale;
                    particleScale *= inv * inv;
                }
                const std::int32_t particleIndex = ManagedAdd(
                    ManagedMultiply(2, target.Category), target.Dim ? 1 : 0);
                const SingleType particle = ManagedAt(_scanParticles, particleIndex);
                const float alpha = target.Dim ? 24.0F / 31.0F : 1.0F;
                RequireReference(_scene).AddSingleParticle(
                    particle, target.Position, Vector3(1.0F, 1.0F, 1.0F), alpha, particleScale);
            }
        }
    }

    void PlayerEntity::DrawScanObjects()
    {
        for (std::int32_t i = 0; i < _scanTargetCount; ++i)
        {
            ScanTarget& target = RequireReference(ManagedAt(_scanTargets, i));
            const float iconScale = target.Scale * 90.0F;
            if (target.Entity == RequireReference(_curScanTarget).Entity && iconScale > 14.0F)
            {
                const std::int32_t iconIndex = ManagedAdd(
                    ManagedMultiply(2, target.Category), target.Dim ? 1 : 0);
                if (iconIndex < 0 || static_cast<std::size_t>(iconIndex) >= _scanIconInsts.size())
                {
                    throw MphRead::SceneDetail::IndexOutOfRangeException();
                }
                std::shared_ptr<Hud::HudObjectInstance> iconInst
                    = _scanIconInsts[static_cast<std::size_t>(iconIndex)];
                Hud::HudObjectInstance& icon = RequireReference(iconInst);
                icon.PositionX = target.ScreenX;
                icon.PositionY = target.ScreenY;
                icon.Center = true;
                icon.Alpha = 9.0F / 16.0F;
                icon.UseMask = true;
                RequireReference(_scene).DrawHudObject(icon);
            }
        }

        ScanTarget& current = RequireReference(_curScanTarget);
        if (current.Entity && _boxSizeFac < 4.0F)
        {
            float pixelSize = current.Scale * 90.0F;
            pixelSize = ClampFloat(pixelSize, 4.0F, 14.0F);
            pixelSize *= _boxSizeFac;
            const bool small = pixelSize < 8.0F;
            Hud::HudObjectInstance& cornerInst = RequireReference(_scanCornerInst);
            if (small)
            {
                Hud::HudObject& corner = RequireReference(_scanCornerSmallObj);
                cornerInst.SetCharacterData(
                    corner.CharacterData, corner.Width, corner.Height, RequireReference(_scene));
            }
            else
            {
                Hud::HudObject& corner = RequireReference(_scanCornerObj);
                cornerInst.SetCharacterData(
                    corner.CharacterData, corner.Width, corner.Height, RequireReference(_scene));
            }
            const std::int32_t index = current.Distance < 12.0F ? 0 : 1;
            cornerInst.SetIndex(index, RequireReference(_scene));
            Hud::HudObjectInstance& lineHoriz = RequireReference(_scanLineHorizInst);
            lineHoriz.SetIndex(index, RequireReference(_scene));
            Hud::HudObjectInstance& lineVert = RequireReference(_scanLineVertInst);
            lineVert.SetIndex(index, RequireReference(_scene));
            cornerInst.UseMask = true;
            lineHoriz.UseMask = true;
            lineVert.UseMask = true;

            const float offsetX = (pixelSize - 16.0F) / 256.0F;
            const float offsetY = (pixelSize - 16.0F) / 192.0F;
            const float leftPos = _boxCornerX - pixelSize / 256.0F;
            const float rightPos = _boxCornerX + offsetX;
            const float topPos = _boxCornerY - pixelSize / 192.0F;
            const float bottomPos = _boxCornerY + offsetY;

            cornerInst.PositionX = leftPos;
            cornerInst.PositionY = topPos;
            cornerInst.FlipHorizontal = false;
            cornerInst.FlipVertical = false;
            RequireReference(_scene).DrawHudObject(cornerInst, 1);
            cornerInst.PositionX = rightPos;
            cornerInst.PositionY = topPos;
            cornerInst.FlipHorizontal = true;
            cornerInst.FlipVertical = false;
            RequireReference(_scene).DrawHudObject(cornerInst, 1);
            cornerInst.PositionX = rightPos;
            cornerInst.PositionY = bottomPos;
            cornerInst.FlipHorizontal = true;
            cornerInst.FlipVertical = true;
            RequireReference(_scene).DrawHudObject(cornerInst, 1);
            cornerInst.PositionX = leftPos;
            cornerInst.PositionY = bottomPos;
            cornerInst.FlipHorizontal = false;
            cornerInst.FlipVertical = true;
            RequireReference(_scene).DrawHudObject(cornerInst, 1);

            float curX = 16.0F / 256.0F;
            lineHoriz.PositionY = _boxCornerY;
            for (std::int32_t i = 0; i < 10; ++i)
            {
                lineHoriz.PositionX = _boxCornerX + offsetX + curX;
                RequireReference(_scene).DrawHudObject(lineHoriz);
                curX += 16.0F / 256.0F;
            }
            curX = 32.0F / 256.0F;
            lineHoriz.PositionY = _boxCornerY;
            for (std::int32_t i = 0; i < 10; ++i)
            {
                lineHoriz.PositionX = _boxCornerX - offsetX - curX;
                RequireReference(_scene).DrawHudObject(lineHoriz);
                curX += 16.0F / 256.0F;
            }

            float curY = 16.0F / 192.0F;
            lineVert.PositionX = _boxCornerX;
            for (std::int32_t i = 0; i < 10; ++i)
            {
                lineVert.PositionY = _boxCornerY + offsetY + curY;
                RequireReference(_scene).DrawHudObject(lineVert);
                curY += 16.0F / 192.0F;
            }
            curY = 32.0F / 192.0F;
            lineVert.PositionY = _boxCornerY;
            for (std::int32_t i = 0; i < 10; ++i)
            {
                lineVert.PositionY = _boxCornerY - offsetY - curY;
                RequireReference(_scene).DrawHudObject(lineVert);
                curY += 16.0F / 192.0F;
            }
        }
    }

    void PlayerEntity::DrawScanProgress()
    {
        const float posY = 128.0F + _objShiftY;
        const std::string text = Text::Strings::GetHudMessage(103);
        DrawText2D(128.0F + _objShiftX, posY - 8.0F, Hud::Align::Center, 0, text);
        Hud::HudMeter& meter = RequireReference(_scanProgressMeter);
        const std::int32_t length = meter.Length;
        meter.TankAmount = ConvertToInt32Net9(_scanningTime * 120.0F);
        meter.Horizontal = true;
        meter.TankCount = 0;
        meter.Length = 40;
        const std::int32_t currentAmount = ConvertToInt32Net9(_scanningTimer * 120.0F);
        DrawMeter(108.0F + _objShiftX, posY, meter.TankAmount, currentAmount,
            0, _scanProgressMeter, false, false);
        RequireReference(_scanProgressMeter).Length = length;
    }

    void PlayerEntity::UpdateVisorMessage()
    {
        if (_visorMessageTimer > 0.0F && _visorMessageId != 0)
        {
            _visorMessageTimer -= RequireReference(_scene).FrameTime;
            if (_visorMessageTimer <= 0.0F)
            {
                if (_visorMessageScrollOut)
                {
                    _visorMessageScrollOut = false;
                    _visorMessageTimer = 0.0F;
                }
                else
                {
                    _visorMessageScrollOut = true;
                    _visorMessageTimer = _visorMessageTime;
                }
            }
        }
    }

    void PlayerEntity::DrawVisorMessage()
    {
        if (_visorMessageTimer > 0.0F && _visorMessageId != 0)
        {
            const std::string text = Text::Strings::GetHudMessage(_visorMessageId);
            float time = _visorMessageTimer;
            if (!_visorMessageScrollOut)
            {
                time = _visorMessageTime - _visorMessageTimer;
            }
            const std::int32_t characters = ConvertToInt32Net9(time / (1.0F / 30.0F));
            const float posX = 128.0F + _objShiftX;
            const float posY = 157.0F + _objShiftY;
            DrawText2D(posX, posY, Hud::Align::PadCenter, 0, text,
                std::nullopt, 1.0F, -1.0F, characters);
        }
    }
}

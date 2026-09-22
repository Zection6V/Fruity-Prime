#include "SpectatorMode.hpp"

#include "../Entities/Players/PlayerEntity.hpp"
#include "../GameState.hpp"
#include "../Scene.hpp"
#include "Network/NetHooks.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace MphRead::Mods::SpectatorModeInterop
{
    [[nodiscard]] bool HudReady(Entities::PlayerEntity& player);
    void SetUpHud(Entities::PlayerEntity& player);
    void ModSetSpectating(Entities::PlayerEntity& player, bool value);
}

namespace
{
    using MphRead::Entities::LoadFlags;
    using MphRead::Entities::PlayerEntity;

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer&& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return std::forward<TContainer>(values)[static_cast<std::size_t>(index)];
    }

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    [[nodiscard]] constexpr std::int32_t AddInt32Unchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }
}

namespace MphRead::Mods
{
    bool SpectatorMode::_isSpectating = false;
    bool SpectatorMode::_freeCamera = false;
    std::optional<bool> SpectatorMode::_cameraRequest{};
    bool SpectatorMode::_showScoreboard = false;

    bool SpectatorMode::IsSpectating() noexcept
    {
        return _isSpectating;
    }

    bool SpectatorMode::FreeCamera() noexcept
    {
        return _freeCamera;
    }

    bool SpectatorMode::CanSpectate()
    {
        return GameState::Multiplayer();
    }

    void SpectatorMode::Start(bool watchSomeone)
    {
        if (_isSpectating || !CanSpectate())
        {
            return;
        }
        const std::int32_t next = FindNextActiveSlot(PlayerEntity::MainPlayerIndex());
        if (watchSomeone && next == -1)
        {
            return;
        }
        _isSpectating = true;

        const std::int32_t localSlot = Network::NetHooks::LocalSlot();
        if (localSlot >= 0
            && static_cast<std::size_t>(localSlot) < PlayerEntity::Players().size())
        {
            SpectatorModeInterop::ModSetSpectating(
                RequireReference(ManagedAt(PlayerEntity::Players(), localSlot)), true);
        }
        if (watchSomeone)
        {
            Switch(next);
            return;
        }
        _cameraRequest = true;
    }

    void SpectatorMode::CycleNext()
    {
        if (!_isSpectating)
        {
            return;
        }
        const std::int32_t next = FindNextActiveSlot(PlayerEntity::MainPlayerIndex());
        if (next == -1)
        {
            return;
        }
        if (_freeCamera)
        {
            _cameraRequest = false;
        }
        Switch(next);
    }

    void SpectatorMode::ToggleView()
    {
        if (!_isSpectating)
        {
            return;
        }
        if (_freeCamera)
        {
            CycleNext();
            return;
        }
        _cameraRequest = true;
    }

    bool SpectatorMode::ShowScoreboard() noexcept
    {
        return _showScoreboard;
    }

    void SpectatorMode::NoteScoreboard(bool down) noexcept
    {
        _showScoreboard = down && _isSpectating;
    }

    void SpectatorMode::NoteFreeCamera(bool on) noexcept
    {
        _freeCamera = on;
    }

    std::optional<bool> SpectatorMode::TakeCameraRequest() noexcept
    {
        const std::optional<bool> request = _cameraRequest;
        _cameraRequest.reset();
        return request;
    }

    void SpectatorMode::Switch(std::int32_t slot)
    {
        const std::shared_ptr<PlayerEntity> target
            = ManagedAt(PlayerEntity::Players(), slot);
        PlayerEntity& targetRef = RequireReference(target);
        if (!SpectatorModeInterop::HudReady(targetRef))
        {
            SpectatorModeInterop::SetUpHud(targetRef);
        }
        PlayerEntity::SetMainPlayerIndex(slot);
        Entities::CameraInfo& cameraInfo = RequireReference(targetRef.CameraInfo());
        const Formats::Culling::NodeRef nodeRef = targetRef.NodeRef;
        cameraInfo.NodeRef = nodeRef;
    }

    void SpectatorMode::Rejoin()
    {
        if (!_isSpectating)
        {
            return;
        }
        const std::int32_t localSlot = Network::NetHooks::LocalSlot();
        PlayerEntity::SetMainPlayerIndex(localSlot);
        _isSpectating = false;
        _showScoreboard = false;
        _cameraRequest = false;

        if (localSlot >= 0
            && static_cast<std::size_t>(localSlot) < GameState::Points().size())
        {
            SpectatorModeInterop::ModSetSpectating(
                RequireReference(ManagedAt(PlayerEntity::Players(), localSlot)), false);
            std::int32_t& pointTarget = ManagedAt(GameState::Points(), localSlot);
            const std::int32_t pointValue = ManagedAt(GameState::Points(), localSlot);
            pointTarget = std::min<std::int32_t>(0, pointValue);
            ManagedAt(GameState::Kills(), localSlot) = 0;
            ManagedAt(GameState::Deaths(), localSlot) = 0;
        }
    }

    void SpectatorMode::Reset() noexcept
    {
        _isSpectating = false;
        _freeCamera = false;
        _showScoreboard = false;
        _cameraRequest.reset();
    }

    std::int32_t SpectatorMode::FindNextActiveSlot(std::int32_t fromSlot)
    {
        const std::int32_t localSlot = Network::NetHooks::LocalSlot();
        const auto& players = PlayerEntity::Players();
        for (std::int32_t offset = 1;
            offset <= static_cast<std::int32_t>(players.size()); ++offset)
        {
            const std::int32_t index
                = AddInt32Unchecked(fromSlot, offset)
                % static_cast<std::int32_t>(players.size());
            if (index == localSlot)
            {
                continue;
            }
            const std::shared_ptr<PlayerEntity> candidate
                = ManagedAt(players, index);
            PlayerEntity& candidateRef = RequireReference(candidate);
            if (TestFlag(candidateRef.LoadFlags(), LoadFlags::Active)
                && TestFlag(candidateRef.LoadFlags(), LoadFlags::Spawned)
                && candidateRef.Health() > 0)
            {
                return index;
            }
        }
        return -1;
    }
}

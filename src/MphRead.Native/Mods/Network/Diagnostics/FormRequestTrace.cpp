#include "FormRequestTrace.hpp"
#include "Mods/Network/Diagnostics/NetLog.hpp"
#include "Mods/Network/Session/NetSession.hpp"
#include "Entities/Players/PlayerEntity.hpp"

namespace MphRead::Mods::Network
{
    bool FormTransitionTrace::_enabled = false;
    void FormTransitionTrace::RecordDraw(const Entities::PlayerEntity& player)
    {
        using namespace Entities;
        if (!Enabled() || !NetLog::Enabled() || !NetSession::Active()
            || NetSession::IsAuthority() || player.SlotIndex() == NetSession::LocalSlot()
            || !player.ModIsInPlay() || !TestFlag(player.Flags2(), PlayerFlags2::DrawnThirdPerson)) return;
        const bool attack = TestFlag(player.Flags2(), PlayerFlags2::AltAttack)
            || TestFlag(player.Flags1(), PlayerFlags1::Boosting);
        if (attack) NetLog::Event("[form-draw] slot=" + std::to_string(player.SlotIndex())
            + " hunter=" + ToString(player.Hunter()) + " frame=" + std::to_string(NetSession::NetFrame())
            + " alt=" + (player.IsAltForm() ? std::string("1") : std::string("0"))
            + " state=" + player.ModFormState());
    }
    namespace
    {
        std::uint8_t Direction(const Entities::PlayerEntity& player)
        {
            return player.IsMorphing() ? 1 : player.IsUnmorphing() ? 2 : 0;
        }
    }
    FormRequestTrace::FormRequestTrace(const Entities::PlayerEntity& player)
    {
        if (!FormTransitionTrace::Enabled() || !NetLog::Enabled() || !NetSession::IsAuthority()) return;
        _player = &player;
        _before = Direction(player);
    }
    FormRequestTrace::~FormRequestTrace()
    {
        if (_player == nullptr) return;
        const auto after = Direction(*_player);
        NetLog::Event("[form-request] slot=" + std::to_string(_player->SlotIndex())
            + " frame=" + std::to_string(NetSession::NetFrame())
            + " accepted=" + (after != 0 && after != _before ? std::string("1") : std::string("0"))
            + " direction=" + std::to_string(after));
    }
}

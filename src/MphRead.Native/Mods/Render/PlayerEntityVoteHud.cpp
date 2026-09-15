#include "PlayerEntityVoteHud.hpp"

#include "../../Entities/Players/DynamicLightEntity.hpp"
#include "../../HUD/HudInfo.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace System
{
    class Math
    {
    public:
        Math() = delete;

        [[nodiscard]] static float Max(float val1, float val2);
    };

    class OperatingSystem
    {
    public:
        OperatingSystem() = delete;

        [[nodiscard]] static bool IsAndroid();
    };
}

namespace MphRead
{
    class Scene
    {
    public:
        void DrawHudFlatBox(float left, float top, float right, float bottom,
            OpenTK::Mathematics::Vector4 colour);
    };
}

namespace MphRead::Mods::Network
{
    class MapVote final
    {
    public:
        MapVote() = delete;

        [[nodiscard]] static bool Active();
        [[nodiscard]] static bool Answered();
        [[nodiscard]] static std::string PromptLine();
        [[nodiscard]] static std::string TallyLine();
        static void NoteLayout(Mods::EndScreen::Hit accept, Mods::EndScreen::Hit deny);
    };
}

namespace
{
    MphRead::Scene& RequireScene(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *scene;
    }
}

namespace MphRead::Entities
{
    class PlayerEntity final : public DynamicLightEntityBase,
        public std::enable_shared_from_this<PlayerEntity>
    {
    public:
        [[nodiscard]] bool IsMainPlayer() const;

    private:
        [[nodiscard]] float HudAspectFix() const;
        OpenTK::Mathematics::Vector2 DrawText2D(float x, float y, Hud::Align type,
            std::int32_t palette, std::string_view text,
            std::optional<ColorRgba> color = std::nullopt, float alpha = 1.0F,
            float fontSpacing = -1.0F, std::int32_t maxLength = -1, float scale = 1.0F);

        MPHREAD_PLAYER_VOTE_HUD_MEMBERS
    };

    const OpenTK::Mathematics::Vector4 PlayerEntity::_votePanel(0.0F, 0.0F, 0.0F, 0.42F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::_voteAccept(0.24F, 0.78F, 0.33F, 0.40F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::_voteAcceptLit(0.30F, 0.92F, 0.40F, 0.60F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::_voteDeny(0.85F, 0.24F, 0.24F, 0.40F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::_voteDenyLit(0.98F, 0.32F, 0.32F, 0.60F);
    const ColorRgba PlayerEntity::_voteInk(235, 238, 245, 255);
    const ColorRgba PlayerEntity::_voteDimInk(170, 178, 190, 255);

    float PlayerEntity::VoteLeft()
    {
        return System::OperatingSystem::IsAndroid() ? 52.0F : 6.0F;
    }

    float PlayerEntity::VoteTop()
    {
        return System::OperatingSystem::IsAndroid() ? 42.0F : 8.0F;
    }

    float PlayerEntity::VoteButtonScale()
    {
        return System::OperatingSystem::IsAndroid() ? 1.35F : 1.0F;
    }

    float PlayerEntity::VoteButtonWidth()
    {
        return 52.0F * VoteButtonScale();
    }

    float PlayerEntity::VoteButtonHeight()
    {
        return 13.0F * VoteButtonScale();
    }

    float PlayerEntity::VoteButtonGap()
    {
        return 4.0F * VoteButtonScale();
    }

    bool PlayerEntity::VoteByTouch()
    {
        return System::OperatingSystem::IsAndroid();
    }

    void PlayerEntity::ModDrawVote()
    {
        using Mods::Network::MapVote;

        if (!MapVote::Active() || !IsMainPlayer())
        {
            return;
        }
        if (Mods::EndScreen::Available())
        {
            MapVote::NoteLayout({}, {});
            return;
        }

        float aspect = HudAspectFix();
        std::string prompt = MapVote::PromptLine();
        std::string tally = MapVote::TallyLine();
        bool buttons = VoteByTouch() && !MapVote::Answered();
        float height = VoteLineHeight * 2.0F + 4.0F
            + (buttons ? VoteButtonHeight() + VoteButtonGap() : 0.0F);
        float width = System::Math::Max(
            118.0F, VoteButtonWidth() * 2.0F + VoteButtonGap() + 6.0F);
        float right = VoteLeft() + width * aspect;

        float panelLeft = VoteLeft();
        float panelTop = VoteTop();
        float panelBottomTop = VoteTop();
        RequireScene(_scene).DrawHudFlatBox(
            panelLeft, panelTop, right, panelBottomTop + height, _votePanel);

        float promptXLeft = VoteLeft();
        float promptTop = VoteTop();
        DrawText2D(promptXLeft + 3.0F * aspect, promptTop + 2.0F, Hud::Align::Left, 0,
            prompt, _voteInk, 1.0F, 8.0F, -1, 0.42F);

        float tallyXLeft = VoteLeft();
        float tallyTop = VoteTop();
        DrawText2D(tallyXLeft + 3.0F * aspect, tallyTop + 2.0F + VoteLineHeight,
            Hud::Align::Left, 0, tally, _voteDimInk, 1.0F, 8.0F, -1, 0.42F);

        if (!buttons)
        {
            MapVote::NoteLayout({}, {});
            return;
        }

        float buttonTop = VoteTop() + VoteLineHeight * 2.0F + 4.0F;
        float bottom = buttonTop + VoteButtonHeight();
        float acceptLeft = VoteLeft() + 3.0F * aspect;
        float acceptRight = acceptLeft + VoteButtonWidth() * aspect;
        float denyLeft = acceptRight + VoteButtonGap() * aspect;
        float denyRight = denyLeft + VoteButtonWidth() * aspect;
        Mods::EndScreen::Hit accept = ModVoteHit(acceptLeft, buttonTop, acceptRight, bottom);
        Mods::EndScreen::Hit deny = ModVoteHit(denyLeft, buttonTop, denyRight, bottom);

        float acceptPointerX = Mods::EndScreen::PointerX();
        float acceptPointerY = Mods::EndScreen::PointerY();
        bool overAccept = accept.Contains(acceptPointerX, acceptPointerY);
        float denyPointerX = Mods::EndScreen::PointerX();
        float denyPointerY = Mods::EndScreen::PointerY();
        bool overDeny = deny.Contains(denyPointerX, denyPointerY);

        RequireScene(_scene).DrawHudFlatBox(
            acceptLeft, buttonTop, acceptRight, bottom, overAccept ? _voteAcceptLit : _voteAccept);
        RequireScene(_scene).DrawHudFlatBox(
            denyLeft, buttonTop, denyRight, bottom, overDeny ? _voteDenyLit : _voteDeny);

        float labelY = buttonTop
            + (VoteButtonHeight() - 16.0F * 0.42F * VoteButtonScale()) / 2.0F;
        DrawText2D((acceptLeft + acceptRight) / 2.0F, labelY, Hud::Align::Center, 0,
            "ACCEPT", _voteInk, 1.0F, 8.0F, -1, 0.42F * VoteButtonScale());
        DrawText2D((denyLeft + denyRight) / 2.0F, labelY, Hud::Align::Center, 0,
            "DENY", _voteInk, 1.0F, 8.0F, -1, 0.42F * VoteButtonScale());
        MapVote::NoteLayout(accept, deny);
    }

    Mods::EndScreen::Hit PlayerEntity::ModVoteHit(
        float left, float top, float right, float bottom)
    {
        return Mods::EndScreen::Hit{
            left / 256.0F, top / 192.0F, right / 256.0F, bottom / 192.0F
        };
    }
}

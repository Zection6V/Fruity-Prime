#pragma once

#include "../../Formats/Types.hpp"

namespace MphRead::Mods
{
    class EndScreen final
    {
    public:
        EndScreen() = delete;

        struct Hit
        {
            const float Left = 0.0F;
            const float Top = 0.0F;
            const float Right = 0.0F;
            const float Bottom = 0.0F;

            [[nodiscard]] bool Contains(float x, float y) const;
        };

        [[nodiscard]] static bool Available();
        [[nodiscard]] static float PointerX();
        [[nodiscard]] static float PointerY();
    };
}

#define MPHREAD_PLAYER_VOTE_HUD_MEMBERS \
public: \
    void ModDrawVote(); \
private: \
    [[nodiscard]] static float VoteLeft(); \
    [[nodiscard]] static float VoteTop(); \
    static constexpr float VoteLineHeight = 8.0F; \
    [[nodiscard]] static float VoteButtonScale(); \
    [[nodiscard]] static float VoteButtonWidth(); \
    [[nodiscard]] static float VoteButtonHeight(); \
    [[nodiscard]] static float VoteButtonGap(); \
    static const ::OpenTK::Mathematics::Vector4 _votePanel; \
    static const ::OpenTK::Mathematics::Vector4 _voteAccept; \
    static const ::OpenTK::Mathematics::Vector4 _voteAcceptLit; \
    static const ::OpenTK::Mathematics::Vector4 _voteDeny; \
    static const ::OpenTK::Mathematics::Vector4 _voteDenyLit; \
    static const ::MphRead::ColorRgba _voteInk; \
    static const ::MphRead::ColorRgba _voteDimInk; \
    [[nodiscard]] static bool VoteByTouch(); \
    [[nodiscard]] static ::MphRead::Mods::EndScreen::Hit ModVoteHit( \
        float left, float top, float right, float bottom);

#pragma once

#include "../../Formats/Types.hpp"

#include "../EndScreen.hpp"

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

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../Entities/Players/PlayerEntity.hpp"
#endif

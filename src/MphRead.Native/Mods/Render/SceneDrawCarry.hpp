#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

// The scene's side of drawing the world at the picture's instant
// (Mods/Render/ScenePresentation): what an entity drew, carried on.
#define MPHREAD_SCENE_DRAW_CARRY_MEMBERS                                                       \
public:                                                                                        \
    /* How much has been drawn so far this picture: render items, single particles. */        \
    struct ModDrawMark final                                                                   \
    {                                                                                          \
        std::size_t Items = 0;                                                                 \
        std::int32_t Singles = 0;                                                              \
    };                                                                                         \
    [[nodiscard]] ModDrawMark ModMarkDrawn() const noexcept;                                   \
    /* Hook: GetDrawItems, after an entity or a player has drawn since `from`. */              \
    void ModCarryEntityDrawn(const ModDrawMark& from, const ::MphRead::Entities::EntityBase& entity); \
    void ModCarryPlayerDrawn(const ModDrawMark& from, const ::MphRead::Entities::PlayerEntity& player); \
private:                                                                                       \
    void ModCarryDrawn(const ModDrawMark& from, const ::OpenTK::Mathematics::Matrix4& carry,  \
        std::optional<RenderItemType> keep = std::nullopt);

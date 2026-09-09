#pragma once

#include "Formats/enum_tables.hpp"

#include <cstdint>
#include <optional>

namespace fruityprime::selection {

using SelectionType = formats::SelectionType;

enum class Type {
    None,
    Entity,
    Instance,
    Node,
    Mesh,
    Parent,
    Child
};

struct Key {
    Type type = Type::None;
    std::int32_t id = -1;
    friend bool operator==(const Key&, const Key&) = default;
};

// The managed selector is a hierarchy (entity -> model instance -> node ->
// mesh), not just one flat object id.  Keep the four references together so
// CheckSelection can apply the same precedence as Selection.cs.
struct Path {
    std::int32_t entity = -1;
    std::int32_t instance = -1;
    std::int32_t node = -1;
    std::int32_t mesh = -1;
    friend bool operator==(const Path&, const Path&) = default;
};

struct Color {
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float alpha = 1.0F;
};

class State {
public:
    void select(Key key) noexcept;
    void select_path(Path path) noexcept;
    void clear() noexcept;
    void toggle_show_selection() noexcept { show_selection_ = !show_selection_; }
    void toggle_hide_unselected_volumes() noexcept {
        hide_unselected_volumes_ = !hide_unselected_volumes_;
    }
    [[nodiscard]] const std::optional<Key>& selected() const noexcept {
        return selected_;
    }
    [[nodiscard]] bool show_selection() const noexcept { return show_selection_; }
    [[nodiscard]] bool hide_unselected_volumes() const noexcept {
        return hide_unselected_volumes_;
    }
    [[nodiscard]] bool is_selected(Key key) const noexcept;
    [[nodiscard]] bool check_volume(std::int32_t entity_id) const noexcept;
    [[nodiscard]] formats::SelectionType check_selection(
        Path candidate, std::int32_t parent_entity = -1,
        std::int32_t child_entity = -1) const noexcept;
    [[nodiscard]] const Path& selected_path() const noexcept {
        return selected_path_;
    }
    [[nodiscard]] static Color color(Type type) noexcept;
    [[nodiscard]] std::optional<Color> selection_color(
        formats::SelectionType type, std::uint64_t tick_ms) const noexcept;

private:
    std::optional<Key> selected_;
    Path selected_path_;
    bool show_selection_ = true;
    bool hide_unselected_volumes_ = false;
};

} // namespace fruityprime::selection

namespace MphReadNative {
using SelectionState = ::fruityprime::selection::State;
namespace Selection = ::fruityprime::selection;
}

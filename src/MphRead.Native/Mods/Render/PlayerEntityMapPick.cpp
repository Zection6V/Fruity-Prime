#include "PlayerEntityMapPick.hpp"

#include "MapThumbnail.hpp"
#include "../MapPick.hpp"
#include "../Input/GamepadGlyphs.hpp"
#include "../Input/InputSourceTracker.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <memory>

namespace MphRead::Entities
{
    using ::MphRead::NativeRuntime::RequireReference;
    using OpenTK::Mathematics::Vector4;

    const Vector4 PlayerEntity::_pickPanel(0, 0, 0, 0.62F);
    const Vector4 PlayerEntity::_pickEdge(1, 1, 1, 0.16F);
    const Vector4 PlayerEntity::_pickRing(1, 0.84F, 0.35F, 0.95F);
    const Vector4 PlayerEntity::_pickHover(1, 1, 1, 0.16F);
    const Vector4 PlayerEntity::_pickCursor(1, 1, 1, 0.09F);
    const Vector4 PlayerEntity::_pickVoted(1, 0.84F, 0.35F, 0.12F);
    const Vector4 PlayerEntity::_pickCarriedBand(0.35F, 0.85F, 0.4F, 0.16F);
    const Vector4 PlayerEntity::_pickWell(0, 0, 0, 0.5F);
    const Vector4 PlayerEntity::_pickBar(1, 1, 1, 0.22F);
    const ColorRgba PlayerEntity::_pickInk(235, 238, 245, 255);
    const ColorRgba PlayerEntity::_pickDim(165, 174, 190, 255);
    const ColorRgba PlayerEntity::_pickTally(255, 215, 90, 255);
    const ColorRgba PlayerEntity::_pickCarried(150, 240, 160, 255);

    // The results screen's ballot: under the hunter picker, in the same column.
    void PlayerEntity::ModDrawMapPick(float panelBottom)
    {
        const auto total = static_cast<std::int32_t>(Mods::MapPick::Order().size());
        if (!Mods::MapPick::Available() || total == 0)
        {
            return;
        }
        const float top = panelBottom + 3;
        const float room = PickFloor - top;
        std::int32_t rows = PickRowsMax;
        float scale = 0;
        while (rows >= 1)
        {
            const float needed = PickTitle + static_cast<float>(rows) * PickRow + 2;
            scale = std::min(EndScale(), room / needed);
            if (scale >= 0.5F)
            {
                break;
            }
            rows--;
        }
        if (rows < 1 || scale < 0.5F)
        {
            return;
        }
        rows = std::min(rows, total);
        const float aspect = HudAspectFix();
        const float width = EndPanelWidth() * scale;
        const float right = 254;
        const float left = right - width * aspect;
        const float centre = left + width / 2 * aspect;
        const float bottom = top + (PickTitle + static_cast<float>(rows) * PickRow + 2) * scale;
        Scene& scene = RequireReference(_scene);
        scene.DrawHudFlatBox(left, top, right, bottom, _pickPanel);
        scene.DrawHudFlatBox(left, top, left + 0.6F * aspect, bottom, _pickEdge);
        std::string title = Mods::MapPick::Eligible() > 1 ? "VOTE NEXT MAP  MOST WINS" : "VOTE NEXT MAP";
        if (Mods::Input::InputSourceTracker::Current() == Mods::Input::InputSource::Gamepad)
        {
            title = ::MphRead::NativeRuntime::ToUpperInvariant(
                Mods::Input::GamepadGlyphs::Resolve(Mods::Input::GamepadButtons::RightBumper)) + " VOTE  UP/DOWN SELECT";
        }
        static_cast<void>(DrawText2D(centre, top + 1.5F * scale, Hud::Align::Center, 0, title, _pickDim, 1.0F, 8.0F, -1,
            0.42F * scale));
        const std::int32_t picked = Mods::MapPick::PickedIndex();
        const std::int32_t hovered = Mods::MapPick::Hovered();
        const std::int32_t cursor = Mods::MapPick::Cursor();
        const std::int32_t scroll = std::clamp(Mods::MapPick::Scroll(), 0, std::max(0, total - rows));
        float rowTop = top + PickTitle * scale;
        for (std::int32_t i = 0; i < rows; i++)
        {
            const std::int32_t index = scroll + i;
            DrawPickRow(i, index, left, rowTop, width, scale, aspect, picked == index, hovered == index, cursor == index);
            rowTop += PickRow * scale;
        }
        DrawPickScrollBar(right, top + PickTitle * scale, static_cast<float>(rows) * PickRow * scale, scroll, rows, total, aspect);
        Mods::MapPick::NoteLayout(_pickHits, rows);
    }

    void PlayerEntity::DrawPickScrollBar(float right, float top, float height, std::int32_t scroll, std::int32_t rows,
        std::int32_t total, float aspect)
    {
        if (total <= rows || height <= 0)
        {
            return;
        }
        const float width = 0.8F * aspect;
        const float thumb = std::max(height * static_cast<float>(rows) / static_cast<float>(total), 3.0F);
        const float travel = height - thumb;
        const float at = total > rows ? static_cast<float>(scroll) / static_cast<float>(total - rows) : 0;
        RequireReference(_scene).DrawHudFlatBox(right - width, top + travel * at, right, top + travel * at + thumb, _pickBar);
    }

    void PlayerEntity::DrawPickRow(std::int32_t slot, std::int32_t index, float left, float top, float width, float scale,
        float aspect, bool picked, bool hovered, bool cursor)
    {
        Scene& scene = RequireReference(_scene);
        const std::string& key = Mods::MapPick::Order().at(static_cast<std::size_t>(index));
        const std::int32_t votes = Mods::MapPick::VotesFor(key);
        const bool leading = votes > 0 && ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(key, Mods::MapPick::Leader());
        const float height = PickThumb * scale;
        const float rowRight = left + width * aspect;
        const float bottom = top + height;
        const float bandTop = top - PickGap / 2 * scale;
        const float bandBottom = bottom + PickGap / 2 * scale;
        if (votes > 0)
        {
            scene.DrawHudFlatBox(left, bandTop, rowRight, bandBottom, leading ? _pickCarriedBand : _pickVoted);
        }
        if (hovered)
        {
            scene.DrawHudFlatBox(left, bandTop, rowRight, bandBottom, _pickHover);
        }
        else if (cursor)
        {
            scene.DrawHudFlatBox(left, bandTop, rowRight, bandBottom, _pickCursor);
        }
        const float thumbLeft = left + 1.5F * scale * aspect;
        const float thumbRight = thumbLeft + PickThumbWidth * scale * aspect;
        scene.DrawHudFlatBox(thumbLeft, top, thumbRight, bottom, _pickWell);
        const std::int32_t texture = Mods::Render::MapThumbnail::For(key, scene);
        if (texture > 0)
        {
            scene.DrawHudTexture(thumbLeft, top, thumbRight, bottom, texture);
        }
        if (picked)
        {
            DrawPickRing(thumbLeft, top, thumbRight, bottom, scale, aspect);
        }
        const float textLeft = thumbRight + 2 * scale * aspect;
        std::string name = ::MphRead::NativeRuntime::ToUpperInvariant(Mods::MapPick::NameOf(key));
        if (name.size() > 15)
        {
            name = name.substr(0, 15);
        }
        static_cast<void>(DrawText2D(textLeft, top + 1.5F * scale, Hud::Align::Left, 0, name,
            votes > 0 || picked ? _pickInk : _pickDim, 1.0F, 8.0F, -1, 0.4F * scale));
        if (votes > 0)
        {
            const std::string tally = Mods::MapPick::Eligible() > 1
                ? std::to_string(votes) + " OF " + std::to_string(Mods::MapPick::Eligible())
                : votes == 1 ? std::string("1 VOTE") : std::to_string(votes) + " VOTES";
            static_cast<void>(DrawText2D(textLeft, top + 7 * scale, Hud::Align::Left, 0,
                leading ? tally + "  NEXT" : tally, leading ? _pickCarried : _pickTally, 1.0F, 8.0F, -1, 0.36F * scale));
        }
        // Hit is a readonly struct: replaced whole, as the C# array slot is.
        Mods::EndScreen::Hit& hit = _pickHits.at(static_cast<std::size_t>(slot));
        std::destroy_at(&hit);
        std::construct_at(&hit, ModHudHit(left, bandTop, rowRight, bandBottom));
    }

    void PlayerEntity::DrawPickRing(float left, float top, float right, float bottom, float scale, float aspect)
    {
        Scene& scene = RequireReference(_scene);
        const float line = std::max(0.5F, 0.8F * scale);
        scene.DrawHudFlatBox(left - line * aspect, top - line, right + line * aspect, top, _pickRing);
        scene.DrawHudFlatBox(left - line * aspect, bottom, right + line * aspect, bottom + line, _pickRing);
        scene.DrawHudFlatBox(left - line * aspect, top, left, bottom, _pickRing);
        scene.DrawHudFlatBox(right, top, right + line * aspect, bottom, _pickRing);
    }
}

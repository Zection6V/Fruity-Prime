#include "MenuData.hpp"

#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace MphRead::Mods::ClassicMenu
{
    const MenuFont::UnicodeGlyph& MenuFont::Rasterize(char32_t cp) const
    {
        if (const auto it = _unicodeGlyphs.find(cp); it != _unicodeGlyphs.end()) return it->second;

        // Each atlas is allocated on first use and contains many glyphs.
        // Pages are immutable in size, so existing UVs and texture IDs remain
        // valid while subsequent glyphs fill unused portions of a page.
        constexpr int AtlasSide = 256;
        constexpr int Padding = 1;

        QFont font(QStringLiteral("Sans Serif"));
        font.setPixelSize(11);
        const QFontMetricsF metrics(font);
        const QString letter = QString::fromUcs4(&cp, 1);
        const int advance = std::max(1, static_cast<int>(std::ceil(metrics.horizontalAdvance(letter))));
        const int width = advance + 2;
        const int height = std::max(1, static_cast<int>(std::ceil(metrics.height()))) + 2;
        if (width + Padding > AtlasSide || height + Padding > AtlasSide)
        {
            // Guard against unexpected gigantic fallback glyph metrics.
            return Rasterize(U'?');
        }

        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.setFont(font);
            painter.setPen(Qt::white);
            painter.drawText(QPointF(1, 1 + metrics.ascent()), letter);
        }

        const auto createPage = [&]
        {
            const int id = _textures.GetOrAdd("menu-unicode-atlas-" + std::to_string(_glyphAtlases.size()), []
            {
                UiTexture texture;
                texture.Width = AtlasSide;
                texture.Height = AtlasSide;
                texture.Pixels.assign(AtlasSide * AtlasSide, ColorRgba(255, 255, 255, 0));
                return texture;
            });
            _glyphAtlases.push_back(GlyphAtlas{id, 0, 0, 0});
        };

        if (_glyphAtlases.empty()) createPage();
        GlyphAtlas* page = &_glyphAtlases.back();
        if (page->NextX + width + Padding > AtlasSide)
        {
            page->NextX = 0;
            page->NextY += page->RowHeight;
            page->RowHeight = 0;
        }
        if (page->NextY + height + Padding > AtlasSide)
        {
            createPage();
            page = &_glyphAtlases.back();
        }

        const int x0 = page->NextX;
        const int y0 = page->NextY;
        page->NextX += width + Padding;
        page->RowHeight = std::max(page->RowHeight, height + Padding);

        UiTexture& texture = _textures.Mutable(page->TextureId);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                texture.Pixels[static_cast<std::size_t>(y0 + y) * AtlasSide
                    + static_cast<std::size_t>(x0 + x)] = ColorRgba(255, 255, 255,
                        static_cast<std::uint8_t>(qAlpha(image.pixel(x, y))));
            }
        }
        const UnicodeGlyph glyph{
            page->TextureId, width, height, advance,
            static_cast<float>(x0) / AtlasSide, static_cast<float>(y0) / AtlasSide,
            static_cast<float>(x0 + width) / AtlasSide, static_cast<float>(y0 + height) / AtlasSide
        };
        return _unicodeGlyphs.emplace(cp, glyph).first->second;
    }
}

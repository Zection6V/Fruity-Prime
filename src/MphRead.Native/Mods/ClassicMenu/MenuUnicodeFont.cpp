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

        // The cartridge atlas contains only the legacy glyphs. Render each
        // non-ASCII code point exactly once through Qt's OS font fallback.
        // Never ask Qt to paint in the hot per-frame path after the first use.
        QFont font(QStringLiteral("Sans Serif"));
        font.setPixelSize(11);
        const QFontMetricsF metrics(font);
        const QString letter = QString::fromUcs4(&cp, 1);
        const int advance = std::max(1, static_cast<int>(std::ceil(metrics.horizontalAdvance(letter))));
        const int width = advance + 2;
        const int height = std::max(1, static_cast<int>(std::ceil(metrics.height()))) + 2;

        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.setFont(font);
            painter.setPen(Qt::white);
            painter.drawText(QPointF(1, 1 + metrics.ascent()), letter);
        }

        UiTexture texture;
        texture.Width = width;
        texture.Height = height;
        texture.Pixels.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                texture.Pixels.emplace_back(255, 255, 255,
                    static_cast<std::uint8_t>(qAlpha(image.pixel(x, y))));
            }
        }

        const int textureId = _textures.GetOrAdd("menu-unicode-" + std::to_string(static_cast<std::uint32_t>(cp)),
            [texture = std::move(texture)]() mutable { return std::move(texture); });
        return _unicodeGlyphs.emplace(cp, UnicodeGlyph{textureId, width, height, advance}).first->second;
    }
}

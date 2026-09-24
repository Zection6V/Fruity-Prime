#include "Text.hpp"

#include "Backend.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace MphRead::NativeRuntime::Gui
{
    namespace
    {
        constexpr std::int32_t AtlasPixels = 1024;

        struct GlyphKey final
        {
            char32_t Code = 0;
            std::int32_t Size = 0;
            FontWeight Weight = FontWeight::Normal;

            [[nodiscard]] friend bool operator<(
                const GlyphKey& left, const GlyphKey& right) noexcept
            {
                if (left.Code != right.Code) { return left.Code < right.Code; }
                if (left.Size != right.Size) { return left.Size < right.Size; }
                return left.Weight < right.Weight;
            }
        };

        // The platform's own UI face, in the three weights the launcher asks
        // for. The first path that exists wins.
        [[nodiscard]] const char* const* FontCandidates(FontWeight weight)
        {
#if defined(_WIN32)
            static const char* const normal[] = {
                "C:\\Windows\\Fonts\\segoeui.ttf",
                "C:\\Windows\\Fonts\\arial.ttf", nullptr};
            static const char* const semiBold[] = {
                "C:\\Windows\\Fonts\\seguisb.ttf",
                "C:\\Windows\\Fonts\\segoeui.ttf", nullptr};
            static const char* const bold[] = {
                "C:\\Windows\\Fonts\\segoeuib.ttf",
                "C:\\Windows\\Fonts\\arialbd.ttf", nullptr};
#elif defined(__APPLE__)
            static const char* const normal[] = {
                "/System/Library/Fonts/SFNS.ttf",
                "/System/Library/Fonts/Helvetica.ttc", nullptr};
            static const char* const semiBold[] = {
                "/System/Library/Fonts/SFNS.ttf", nullptr};
            static const char* const bold[] = {
                "/System/Library/Fonts/SFNSRounded.ttf",
                "/System/Library/Fonts/Helvetica.ttc", nullptr};
#else
            static const char* const normal[] = {
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/usr/share/fonts/TTF/DejaVuSans.ttf",
                "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", nullptr};
            static const char* const semiBold[] = {
                "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", nullptr};
            static const char* const bold[] = {
                "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
                "/usr/share/fonts/liberation/LiberationSans-Bold.ttf", nullptr};
#endif
            switch (weight)
            {
            case FontWeight::SemiBold: return semiBold;
            case FontWeight::Bold: return bold;
            default: return normal;
            }
        }

        class FontAtlas final
        {
        public:
            static FontAtlas& Instance()
            {
                static FontAtlas instance;
                return instance;
            }

            [[nodiscard]] const Glyph* Get(char32_t code, double fontSize, FontWeight weight)
            {
                const std::lock_guard<std::mutex> guard(_mutex);
                const GlyphKey key{code, static_cast<std::int32_t>(std::lround(fontSize)), weight};
                const auto found = _glyphs.find(key);
                if (found != _glyphs.end())
                {
                    return &found->second;
                }
                FT_Face face = Face(weight);
                if (face == nullptr)
                {
                    return nullptr;
                }
                if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(key.Size)) != 0)
                {
                    return nullptr;
                }
                if (FT_Load_Char(face, code, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0)
                {
                    return nullptr;
                }
                const FT_GlyphSlot slot = face->glyph;
                Glyph glyph;
                glyph.Width = static_cast<std::int32_t>(slot->bitmap.width);
                glyph.Height = static_cast<std::int32_t>(slot->bitmap.rows);
                glyph.BearingX = slot->bitmap_left;
                glyph.BearingY = slot->bitmap_top;
                glyph.Advance = static_cast<double>(slot->advance.x) / 64.0;
                Place(glyph, slot->bitmap.buffer, static_cast<std::int32_t>(slot->bitmap.pitch));
                return &_glyphs.emplace(key, glyph).first->second;
            }

            [[nodiscard]] double Ascent(double fontSize, FontWeight weight)
            {
                const std::lock_guard<std::mutex> guard(_mutex);
                FT_Face face = Face(weight);
                if (face == nullptr)
                {
                    return fontSize * 0.8;
                }
                FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::lround(fontSize)));
                return static_cast<double>(face->size->metrics.ascender) / 64.0;
            }

            [[nodiscard]] double LineHeight(double fontSize, FontWeight weight)
            {
                const std::lock_guard<std::mutex> guard(_mutex);
                FT_Face face = Face(weight);
                if (face == nullptr)
                {
                    return fontSize * 1.3;
                }
                FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::lround(fontSize)));
                return static_cast<double>(face->size->metrics.height) / 64.0;
            }

            // The texture, with everything rasterised since the last ask
            // uploaded into it. A glyph may be rasterised while measuring,
            // which happens with no drawing surface current, so nothing is
            // sent to the backend until a draw asks for the texture.
            [[nodiscard]] TextureHandle Texture()
            {
                Flush();
                return _texture;
            }
            [[nodiscard]] std::int32_t Pixels() const noexcept { return AtlasPixels; }

        private:
            FontAtlas() = default;

            ~FontAtlas()
            {
                for (FT_Face face : _faces)
                {
                    if (face != nullptr)
                    {
                        FT_Done_Face(face);
                    }
                }
                if (_library != nullptr)
                {
                    FT_Done_FreeType(_library);
                }
            }

            FontAtlas(const FontAtlas&) = delete;
            FontAtlas& operator=(const FontAtlas&) = delete;

            [[nodiscard]] FT_Face Face(FontWeight weight)
            {
                const std::size_t index = static_cast<std::size_t>(weight);
                if (_faces[index] != nullptr)
                {
                    return _faces[index];
                }
                if (_library == nullptr && FT_Init_FreeType(&_library) != 0)
                {
                    return nullptr;
                }
                for (const char* const* path = FontCandidates(weight); *path != nullptr; ++path)
                {
                    FT_Face face = nullptr;
                    if (FT_New_Face(_library, *path, 0, &face) == 0)
                    {
                        _faces[index] = face;
                        return face;
                    }
                }
                return nullptr;
            }

            // A shelf allocator: each row is as tall as its tallest glyph.
            void Place(Glyph& glyph, const unsigned char* pixels, std::int32_t pitch)
            {
                if (glyph.Width <= 0 || glyph.Height <= 0)
                {
                    return;
                }
                if (_penX + glyph.Width + 1 > AtlasPixels)
                {
                    _penX = 0;
                    _penY += _rowHeight + 1;
                    _rowHeight = 0;
                }
                if (_penY + glyph.Height + 1 > AtlasPixels)
                {
                    // The atlas is full; the glyph draws as nothing rather
                    // than as somebody else's pixels.
                    glyph.Width = 0;
                    glyph.Height = 0;
                    return;
                }
                glyph.AtlasX = _penX;
                glyph.AtlasY = _penY;

                std::vector<std::uint8_t> packed(
                    static_cast<std::size_t>(glyph.Width) * glyph.Height);
                for (std::int32_t row = 0; row < glyph.Height; ++row)
                {
                    const unsigned char* const source
                        = pixels + static_cast<std::ptrdiff_t>(row) * pitch;
                    std::copy(source, source + glyph.Width,
                        packed.begin() + static_cast<std::ptrdiff_t>(row) * glyph.Width);
                }
                if (_mask.empty())
                {
                    _mask.assign(
                        static_cast<std::size_t>(AtlasPixels) * AtlasPixels, 0);
                }
                for (std::int32_t row = 0; row < glyph.Height; ++row)
                {
                    std::copy(packed.begin()
                            + static_cast<std::ptrdiff_t>(row) * glyph.Width,
                        packed.begin()
                            + static_cast<std::ptrdiff_t>(row + 1) * glyph.Width,
                        _mask.begin()
                            + static_cast<std::ptrdiff_t>(glyph.AtlasY + row)
                                * AtlasPixels
                            + glyph.AtlasX);
                }
                // The dirty region is kept as whole rows, which is what the
                // shelf allocator fills anyway.
                _dirtyTop = _dirty ? std::min(_dirtyTop, glyph.AtlasY) : glyph.AtlasY;
                _dirtyBottom = _dirty
                    ? std::max(_dirtyBottom, glyph.AtlasY + glyph.Height)
                    : glyph.AtlasY + glyph.Height;
                _dirty = true;

                _penX += glyph.Width + 1;
                _rowHeight = std::max(_rowHeight, glyph.Height);
            }

            void EnsureTexture()
            {
                if (_texture == 0)
                {
                    _texture = Backend().CreateAlphaTexture(AtlasPixels, AtlasPixels);
                }
            }

            void Flush()
            {
                EnsureTexture();
                if (!_dirty)
                {
                    return;
                }
                _dirty = false;
                Backend().UpdateAlphaTexture(_texture, 0, _dirtyTop, AtlasPixels,
                    _dirtyBottom - _dirtyTop,
                    _mask.data()
                        + static_cast<std::ptrdiff_t>(_dirtyTop) * AtlasPixels);
            }

            std::mutex _mutex;
            FT_Library _library = nullptr;
            FT_Face _faces[3]{};
            std::map<GlyphKey, Glyph> _glyphs;
            TextureHandle _texture = 0;
            std::vector<std::uint8_t> _mask;
            std::int32_t _dirtyTop = 0;
            std::int32_t _dirtyBottom = 0;
            bool _dirty = false;
            std::int32_t _penX = 0;
            std::int32_t _penY = 0;
            std::int32_t _rowHeight = 0;
        };
    }

    std::vector<char32_t> Decode(std::string_view text)
    {
        std::vector<char32_t> result;
        std::size_t index = 0;
        while (index < text.size())
        {
            const unsigned char lead = static_cast<unsigned char>(text[index]);
            char32_t code = lead;
            std::size_t extra = 0;
            if (lead >= 0xF0) { code = lead & 0x07U; extra = 3; }
            else if (lead >= 0xE0) { code = lead & 0x0FU; extra = 2; }
            else if (lead >= 0xC0) { code = lead & 0x1FU; extra = 1; }
            ++index;
            for (std::size_t i = 0; i < extra && index < text.size(); ++i, ++index)
            {
                code = (code << 6) | (static_cast<unsigned char>(text[index]) & 0x3FU);
            }
            result.push_back(code);
        }
        return result;
    }

    const Glyph* GetGlyph(char32_t code, double fontSize, FontWeight weight)
    {
        return FontAtlas::Instance().Get(code, fontSize, weight);
    }

    TextureHandle AtlasTexture()
    {
        return FontAtlas::Instance().Texture();
    }

    std::int32_t AtlasSize()
    {
        return FontAtlas::Instance().Pixels();
    }

    double FontAscent(double fontSize, FontWeight weight)
    {
        return FontAtlas::Instance().Ascent(fontSize, weight);
    }

    double FontLineHeight(double fontSize, FontWeight weight)
    {
        return FontAtlas::Instance().LineHeight(fontSize, weight);
    }

    std::vector<std::string> WrapLines(
        std::string_view text, double fontSize, FontWeight weight, double wrapWidth)
    {
        std::vector<std::string> lines;
        std::string current;
        std::string word;
        double currentWidth = 0.0;
        double wordWidth = 0.0;

        const auto flushWord = [&]()
        {
            if (word.empty())
            {
                return;
            }
            if (wrapWidth > 0.0 && !current.empty() && currentWidth + wordWidth > wrapWidth)
            {
                lines.push_back(current);
                current.clear();
                currentWidth = 0.0;
            }
            current += word;
            currentWidth += wordWidth;
            word.clear();
            wordWidth = 0.0;
        };

        for (const char32_t code : Decode(text))
        {
            if (code == U'\n')
            {
                flushWord();
                lines.push_back(current);
                current.clear();
                currentWidth = 0.0;
                continue;
            }
            const Glyph* const glyph = GetGlyph(code, fontSize, weight);
            const double advance = glyph != nullptr ? glyph->Advance : fontSize * 0.5;
            // A space ends a word and may itself be dropped at a break.
            if (code == U' ')
            {
                flushWord();
                current += ' ';
                currentWidth += advance;
                continue;
            }
            // Append to the word as UTF-8.
            if (code < 0x80U)
            {
                word += static_cast<char>(code);
            }
            else if (code < 0x800U)
            {
                word += static_cast<char>(0xC0U | (code >> 6));
                word += static_cast<char>(0x80U | (code & 0x3FU));
            }
            else
            {
                word += static_cast<char>(0xE0U | (code >> 12));
                word += static_cast<char>(0x80U | ((code >> 6) & 0x3FU));
                word += static_cast<char>(0x80U | (code & 0x3FU));
            }
            wordWidth += advance;
        }
        flushWord();
        lines.push_back(current);
        return lines;
    }

    Size MeasureText(
        std::string_view text, double fontSize, FontWeight weight, double wrapWidth)
    {
        if (text.empty())
        {
            return Size{0.0, FontLineHeight(fontSize, weight)};
        }
        const std::vector<std::string> lines
            = WrapLines(text, fontSize, weight, wrapWidth);
        double widest = 0.0;
        for (const std::string& line : lines)
        {
            double width = 0.0;
            for (const char32_t code : Decode(line))
            {
                const Glyph* const glyph = GetGlyph(code, fontSize, weight);
                width += glyph != nullptr ? glyph->Advance : fontSize * 0.5;
            }
            widest = std::max(widest, width);
        }
        return Size{std::ceil(widest),
            std::ceil(FontLineHeight(fontSize, weight) * static_cast<double>(lines.size()))};
    }
}

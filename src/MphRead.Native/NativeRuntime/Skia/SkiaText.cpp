#include "Skia.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <tuple>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

namespace MphRead::NativeRuntime::Skia
{
    namespace
    {
        // One library for every face, as SkFontMgr keeps one.
        [[nodiscard]] FT_Library Library()
        {
            static FT_Library library = []
            {
                FT_Library created = nullptr;
                if (FT_Init_FreeType(&created) != 0)
                {
                    return static_cast<FT_Library>(nullptr);
                }
                return created;
            }();
            return library;
        }

        [[nodiscard]] std::mutex& FaceMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        // FreeType wants its sizes in 26.6; text is laid out in fractional
        // pixels, so the size is quantised to a sixty-fourth.
        [[nodiscard]] FT_F26Dot6 SizeKey(double size)
        {
            return static_cast<FT_F26Dot6>(std::lround(size * 64.0));
        }
    }

    struct Typeface::Impl final
    {
        std::vector<std::uint8_t> Data;
        FT_Face Face = nullptr;
        FT_F26Dot6 CurrentSize = 0;
        mutable std::map<std::pair<char32_t, FT_F26Dot6>, GlyphImage> Glyphs;
        mutable std::map<std::pair<char32_t, FT_F26Dot6>, double> Advances;

        void SetSize(FT_F26Dot6 size)
        {
            if (size != CurrentSize)
            {
                FT_Set_Char_Size(Face, 0, std::max<FT_F26Dot6>(size, 1), 72, 72);
                CurrentSize = size;
            }
        }
    };

    Typeface::~Typeface()
    {
        if (_impl != nullptr && _impl->Face != nullptr)
        {
            std::lock_guard lock(FaceMutex());
            FT_Done_Face(_impl->Face);
        }
    }

    std::shared_ptr<Typeface> Typeface::FromData(std::vector<std::uint8_t> data)
    {
        FT_Library library = Library();
        if (library == nullptr || data.empty())
        {
            return nullptr;
        }
        std::shared_ptr<Typeface> typeface(new Typeface());
        typeface->_impl = std::make_unique<Impl>();
        typeface->_impl->Data = std::move(data);
        std::lock_guard lock(FaceMutex());
        if (FT_New_Memory_Face(library, typeface->_impl->Data.data(),
            static_cast<FT_Long>(typeface->_impl->Data.size()), 0, &typeface->_impl->Face) != 0)
        {
            typeface->_impl->Face = nullptr;
            return nullptr;
        }
        return typeface;
    }

    std::shared_ptr<Typeface> Typeface::FromFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return nullptr;
        }
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return FromData(std::move(data));
    }

    std::shared_ptr<Typeface> Typeface::Default(std::int32_t weight)
    {
        static std::map<std::int32_t, std::shared_ptr<Typeface>> cache;
        static std::mutex mutex;
        std::lock_guard lock(mutex);
        const std::int32_t key = weight >= 600 ? 700 : weight >= 500 ? 600 : 400;
        auto found = cache.find(key);
        if (found != cache.end())
        {
            return found->second;
        }
#if defined(_WIN32)
        const char* candidates[] = {key == 700 ? "C:\\Windows\\Fonts\\segoeuib.ttf"
                : key == 600 ? "C:\\Windows\\Fonts\\seguisb.ttf" : "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf", nullptr};
#elif defined(__APPLE__)
        const char* candidates[] = {"/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc", nullptr};
#else
        const char* candidates[] = {key == 700 ? "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
                : "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf", "/system/fonts/Roboto-Regular.ttf", nullptr};
#endif
        std::shared_ptr<Typeface> typeface;
        for (const char* const* path = candidates; *path != nullptr && typeface == nullptr; ++path)
        {
            typeface = FromFile(*path);
        }
        cache[key] = typeface;
        return typeface;
    }

    Typeface::Metrics Typeface::MetricsAt(double size) const
    {
        Metrics metrics;
        FT_Face face = _impl->Face;
        const double units = face->units_per_EM == 0 ? 1000.0 : face->units_per_EM;
        // Avalonia reads the OS/2 typographic metrics when the font asks for
        // them (USE_TYPO_METRICS) and hhea otherwise, as Skia reports them.
        const TT_OS2* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
        double ascent = face->ascender;
        double descent = -face->descender;
        double gap = face->height - face->ascender + face->descender;
        if (os2 != nullptr && os2->version != 0xFFFF && (os2->fsSelection & (1 << 7)) != 0)
        {
            ascent = os2->sTypoAscender;
            descent = -os2->sTypoDescender;
            gap = os2->sTypoLineGap;
        }
        metrics.Ascent = ascent * size / units;
        metrics.Descent = descent * size / units;
        metrics.LineGap = std::max(0.0, gap) * size / units;
        return metrics;
    }

    bool Typeface::HasGlyph(char32_t code) const
    {
        std::lock_guard lock(FaceMutex());
        return FT_Get_Char_Index(_impl->Face, code) != 0;
    }

    double Typeface::Advance(char32_t code, double size) const
    {
        const auto key = std::make_pair(code, SizeKey(size));
        std::lock_guard lock(FaceMutex());
        const auto found = _impl->Advances.find(key);
        if (found != _impl->Advances.end())
        {
            return found->second;
        }
        _impl->SetSize(key.second);
        double advance = 0;
        const FT_UInt index = FT_Get_Char_Index(_impl->Face, code);
        if (FT_Load_Glyph(_impl->Face, index, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0)
        {
            advance = _impl->Face->glyph->linearHoriAdvance / 65536.0;
        }
        _impl->Advances[key] = advance;
        return advance;
    }

    double Typeface::Kerning(char32_t left, char32_t right, double size) const
    {
        std::lock_guard lock(FaceMutex());
        if (!FT_HAS_KERNING(_impl->Face))
        {
            return 0;
        }
        _impl->SetSize(SizeKey(size));
        FT_Vector delta{};
        if (FT_Get_Kerning(_impl->Face, FT_Get_Char_Index(_impl->Face, left), FT_Get_Char_Index(_impl->Face, right),
            FT_KERNING_UNFITTED, &delta) != 0)
        {
            return 0;
        }
        return delta.x / 64.0;
    }

    const Typeface::GlyphImage* Typeface::Rasterize(char32_t code, double size) const
    {
        const auto key = std::make_pair(code, SizeKey(size));
        std::lock_guard lock(FaceMutex());
        const auto found = _impl->Glyphs.find(key);
        if (found != _impl->Glyphs.end())
        {
            return &found->second;
        }
        _impl->SetSize(key.second);
        GlyphImage image;
        const FT_UInt index = FT_Get_Char_Index(_impl->Face, code);
        if (FT_Load_Glyph(_impl->Face, index, FT_LOAD_TARGET_LIGHT) == 0
            && FT_Render_Glyph(_impl->Face->glyph, FT_RENDER_MODE_NORMAL) == 0)
        {
            const FT_Bitmap& bitmap = _impl->Face->glyph->bitmap;
            image.Width = static_cast<std::int32_t>(bitmap.width);
            image.Height = static_cast<std::int32_t>(bitmap.rows);
            image.Left = _impl->Face->glyph->bitmap_left;
            image.Top = _impl->Face->glyph->bitmap_top;
            image.Coverage.resize(static_cast<std::size_t>(image.Width) * static_cast<std::size_t>(image.Height));
            for (std::int32_t y = 0; y < image.Height; y++)
            {
                const unsigned char* row = bitmap.buffer + static_cast<std::ptrdiff_t>(y) * bitmap.pitch;
                for (std::int32_t x = 0; x < image.Width; x++)
                {
                    image.Coverage[static_cast<std::size_t>(y * image.Width + x)] = row[x];
                }
            }
        }
        return &_impl->Glyphs.emplace(key, std::move(image)).first->second;
    }
}

# ClassicMenu localization — SRP / SSOT / AOT / DOD

Mods/ClassicMenu's language is independent of the game's Scene/SettingsModel language.
`LauncherPrefs` owns only menu-language persistence in `launcher.txt` (`menu_language=en|ja|fr`);
`Compose` owns only user interaction; `MenuStrings` owns the ROM text ID mapping;
`MenuLocalization` owns only lookup; `MenuUnicodeFont` owns only Unicode glyph rasterization.

## Translation source of truth

- `Languages/en.json`, `ja.json`, and `fr.json` contain all authored translations using stable `menu.*` IDs.
- `tools/generate-classicmenu-locales.py` validates strict UTF-8, duplicate keys, missing/extra IDs, empty translations, duplicate English fallback strings and inconsistent placeholders.
- CMake generates `generated/classicmenu/ClassicMenuLanguages.generated.hpp` during the build. The generated file is NOT checked in. Invalid translations fail the build.
- At runtime, translation lookups use static read-only UTF-8 byte pools, compact offset/length records, and pre-sorted index arrays. No JSON parser or locale heap-allocated dictionary is used.

## Runtime data flow

ROM English strings resolve once into a compact `uint16_t` index per ROM ID as they are loaded. `Fill()` and `Add()` resolve only their changed ID. Rendering calls `Localized()` to read the static translation by index, without a per-frame hash search. Unknown or user-supplied text falls back to original text. Changing menu language changes only an enum and invalidates layout measurement when needed.

`MenuFont` consumes `std::string_view` and wraps without allocating new strings for each line. Original ASCII draws from the DS `hudfont` texture; non-ASCII UTF-8 scalars are lazily rasterized into 256x256 shared atlas pages. No Unicode atlas exists if it is never used.

## Validation and performance

Build generation is mandatory for native and test builds, including Android cross compilation (Python runs on the host). The standalone `FruityPrime.MenuLocalization` regression executable uses pure C++ without Qt, GPU, display, network, or ROM files.

Validate manually with all three languages in title, options, server browser, lobby, pause, results and while switching language; verify the game language is unchanged. Measure initial load, text draw CPU cost, atlas pages/bytes and representative FPS before asserting numerical performance improvements.


## Japanese bitmap fallback: complete k8x12L

The source of truth is the **complete upstream k8x12L.bdf**, checked in at
`src/MphRead.Native/Mods/ClassicMenu/Fonts/k8x12L.bdf`. It is the original
2021-05-05 version by Num Kadoma (**7,170 glyphs**). No transliteration
tables, bitmap arrays, or per-Japanese-translation glyph lists are maintained.

CMake runs `tools/generate-classicmenu-bitmap-font.py` with the complete
BDF and emits `generated/classicmenu/MenuK8x12L.generated.hpp`
**inside the build tree**. The output is not checked in. The build depends
on both the script and the BDF, so changing the font automatically rebuilds
the generated compact data. The 7,170 8x12 bitmaps plus 32-bit Unicode
codepoints occupy **114,720 bytes (112.0 KiB)** of read-only memory (16
bytes per record); neither the large BDF text nor a runtime parser is
included in the executable.

The menu's font resolver uses:
1. Original DS 8x8 `hudfont` whenever a packed original-game glyph exists.
2. k8x12L for other Unicode scalars present in the full bitmap font.
3. Qt font rasterization only for characters not contained in either font.

Glyph metadata is binary-searched directly in the sorted static array; no
runtime hash map of the full font is constructed. Glyphs are **lazily copied**
into shared 256x256 pixel atlas pages on first encounter, and only rendered
characters consume GPU/CPU atlas space. Source bitmap rows remain 8x12 and
are scaled to the same 8-unit display height as the DS glyphs. Glyphs
already cached by the menu are not rasterized again. Qt's final fallback
remains for e.g. U+2197, absent from k8x12L.

**License / provenance:** k8x12L BDF (2021-05-05), Copyright (C)
2015-2021 Num Kadoma; https://littlelimit.net/k8x12.htm and
https://littlelimit.net/font.htm#license. Permission to use, copy, modify
and distribute is unrestricted, including commercial use. Supplied AS IS,
without warranty. The author's original BDF copyright header is retained.

The font generator runs automatically with CMake; for manual regeneration:
```sh
python tools/generate-classicmenu-bitmap-font.py \
  --bdf src/MphRead.Native/Mods/ClassicMenu/Fonts/k8x12L.bdf \
  --output /tmp/MenuK8x12L.generated.hpp
```

### Glyph ownership

`Text::Strings` owns the original game's packed-code to Unicode table.
`Mods/ClassicMenu/MenuGameGlyphMap.cpp::FindGameGlyphCode` performs the
menu-only reverse lookup. Both original DS and k8x12L glyphs are drawn
without changing the underlying game's language or string handling.

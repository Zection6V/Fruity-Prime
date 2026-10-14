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

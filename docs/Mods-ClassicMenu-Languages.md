# Mods classic-menu localization

## Scope

The classic DS-style menu in `src/MphRead.Native/Mods/ClassicMenu/` has its own language preference. It never reads or updates the gameplay language (`MenuSettings::Language` / `Scene::Language`). Gameplay subtitles and HUD locale are independent.

## Switching language

In the classic menu, open Options and choose the third newly added entry after Display and Profile. Each activation cycles:

**English → 日本語 → Français → English**

The choice is saved immediately in `launcher.txt` as `menu_language=en|ja|fr`. An absent or invalid value defaults to English. No restart is required.

## Architecture

- Original English ROM strings remain unchanged for `Composer::FindText`, callback lookup, animation, gameplay identifiers and menu navigation.
- `MenuStrings::Localized()` applies a presentation-only translation at text emission time.
- `MenuLocalization.cpp` defines Japanese and French translations for menus, modes, settings, servers, lobbies, and common statuses. Unknown strings fall back to English; custom player/server names are preserved.
- ASCII uses the original DS font. Non-ASCII UTF-8 glyphs use Qt platform fonts, rasterized on first use and cached in the existing texture cache.
- Japanese wraps on Unicode code point boundaries. ASCII wrapping retains its ROM behavior.
- The switch never calls `SettingsModel::Commit` and never modifies the game-language setting.

## Validation

Run `ctest --test-dir <build> -R FruityPrime.MenuLocalization --output-on-failure`.

Visual verification should cover English, Japanese, and French on title, Options, server list, lobby, settings, pause, and results screens; restarting should preserve menu language while leaving game language unchanged.

New menu text should be added to `MenuLocalization.cpp` only if it is display text. Never translate user-supplied names or protocol identifiers.

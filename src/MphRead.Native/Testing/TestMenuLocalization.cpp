#include "../Mods/ClassicMenu/MenuLocalization.hpp"
#include "../Mods/ClassicMenu/MenuK8x12LGlyphs.hpp"

#include <iostream>
#include <string>

using namespace MphRead::Mods::ClassicMenu;
namespace
{
    int Check(bool condition, const char* message)
    {
        if (condition) return 0;
        std::cerr << "Menu localization FAILED: " << message << '\n';
        return 1;
    }
}
int main()
{
    int fail = 0;
    // The generated, sorted k8x12L bitmap subset is compile-time data.
    static_assert(FindK8x12LGlyph(U'画') != nullptr);
    static_assert(FindK8x12LGlyph(U'音') != nullptr);
    static_assert(FindK8x12LGlyph(U'あ') == nullptr); // Original DS glyph.
    static_assert(FindK8x12LGlyph(U'ヶ') == nullptr); // Original DS glyph.
    fail += Check(K8x12LGlyphs.size() > 100, "Japanese k8x12L subset is present");
    for (std::size_t i = 0; i < K8x12LGlyphs.size(); ++i)
    {
        const auto& glyph = K8x12LGlyphs[i];
        fail += Check(FindK8x12LGlyph(glyph.Codepoint) == &glyph, "font lookup is exact");
        if (i > 0)
            fail += Check(K8x12LGlyphs[i - 1].Codepoint < glyph.Codepoint, "font glyph codes are sorted");
    }
    fail += Check(MenuLocalizationCatalogValid(), "generated catalog valid");
    fail += Check(MenuLocalizationEntryCount() == 421, "all DS and Qt menu translations retained");
    fail += Check(ParseMenuLanguage("en") == MenuLanguage::English, "en");
    fail += Check(ParseMenuLanguage("ja") == MenuLanguage::Japanese, "ja");
    fail += Check(ParseMenuLanguage("fr") == MenuLanguage::French, "fr");
    fail += Check(ParseMenuLanguage("bogus") == MenuLanguage::English, "default English");
    const std::string english = "BEGIN GAME";
    fail += Check(TranslateMenuText(english, MenuLanguage::English) == english, "English remains original");
    fail += Check(TranslateMenuText(english, MenuLanguage::Japanese) == "ゲーム開始", "Japanese");
    fail += Check(TranslateMenuText(english, MenuLanguage::French) == "COMMENCER", "French");
    const auto id = FindMenuTextIndex(english);
    fail += Check(id != UnknownMenuText, "ROM index exists");
    fail += Check(TranslateMenuIndex(id, MenuLanguage::Japanese) == "ゲーム開始", "ROM index ja");
    fail += Check(TranslateMenuIndex(id, MenuLanguage::French) == "COMMENCER", "ROM index fr");
    fail += Check(TranslateMenuIndex(UnknownMenuText, MenuLanguage::Japanese).empty(), "invalid index");
    fail += Check(TranslateMenuKey("menu.begin_game", MenuLanguage::French) == "COMMENCER", "stable key fr");
    fail += Check(TranslateMenuKey("menu.language_switch.label", MenuLanguage::Japanese) == "メニュー: 日本語", "switch");
    fail += Check(TranslateMenuText("offline\nbots", MenuLanguage::Japanese) == "オフライン\nBOT対戦", "multiline");
    fail += Check(TranslateMenuText("SETTINGS", MenuLanguage::Japanese) == "設定", "Qt settings ja");
    fail += Check(TranslateMenuText("Resume", MenuLanguage::Japanese) == "再開", "Qt pause ja");
    fail += Check(TranslateMenuText("Menu language", MenuLanguage::Japanese) == "メニューの言語", "independent menu language ja");
    fail += Check(TranslateMenuText("Menu language", MenuLanguage::French) == "Langue des menus", "menu language fr");
    fail += Check(TranslateMenuText("Game text", MenuLanguage::Japanese) == "ゲーム内の文字", "game language separate");
    fail += Check(TranslateMenuKey("menu.ui.settings", MenuLanguage::Japanese) == "設定", "Qt stable ID");
    const std::string custom = "My Custom Server Name";
    fail += Check(FindMenuTextIndex(custom) == UnknownMenuText, "custom label is not indexed");
    fail += Check(TranslateMenuText(custom, MenuLanguage::Japanese) == custom, "custom label unchanged");
    fail += Check(TranslateMenuKey("menu.missing_id", MenuLanguage::Japanese).empty(), "unknown stable key");
    if (fail == 0) std::cout << "AOT ClassicMenu localization checks passed\n";
    return fail == 0 ? 0 : 1;
}

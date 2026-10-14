#include "../Mods/ClassicMenu/MenuLocalization.hpp"

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
    fail += Check(MenuLocalizationCatalogValid(), "generated catalog valid");
    fail += Check(MenuLocalizationEntryCount() == 231, "all 231 original entries retained");
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
    const std::string custom = "My Custom Server Name";
    fail += Check(FindMenuTextIndex(custom) == UnknownMenuText, "custom label is not indexed");
    fail += Check(TranslateMenuText(custom, MenuLanguage::Japanese) == custom, "custom label unchanged");
    fail += Check(TranslateMenuKey("menu.missing_id", MenuLanguage::Japanese).empty(), "unknown stable key");
    if (fail == 0) std::cout << "AOT ClassicMenu localization checks passed\n";
    return fail == 0 ? 0 : 1;
}

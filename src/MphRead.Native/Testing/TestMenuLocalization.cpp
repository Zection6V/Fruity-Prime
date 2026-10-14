#include "../Mods/ClassicMenu/MenuLocalization.hpp"

#include <QtCore/QCoreApplication>
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

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    int failures = 0;
    failures += Check(MenuLocalizationCatalogValid(), "all three JSON catalogs are embedded and consistent");
    failures += Check(MenuLocalizationEntryCount() >= 200, "translated menu coverage");

    failures += Check(ParseMenuLanguage("en") == MenuLanguage::English, "English code");
    failures += Check(ParseMenuLanguage("ja") == MenuLanguage::Japanese, "Japanese code");
    failures += Check(ParseMenuLanguage("fr") == MenuLanguage::French, "French code");
    failures += Check(ParseMenuLanguage("unknown") == MenuLanguage::English, "invalid code fallback");

    const std::string begin = "BEGIN GAME";
    failures += Check(TranslateMenuText(begin, MenuLanguage::English) == begin, "original ROM text");
    failures += Check(TranslateMenuText(begin, MenuLanguage::Japanese) == "ゲーム開始", "Japanese ROM translation");
    failures += Check(TranslateMenuText(begin, MenuLanguage::French) == "COMMENCER", "French ROM translation");

    failures += Check(TranslateMenuKey("menu.begin_game", MenuLanguage::English) == "BEGIN GAME", "stable English ID");
    failures += Check(TranslateMenuKey("menu.begin_game", MenuLanguage::Japanese) == "ゲーム開始", "stable Japanese ID");
    failures += Check(TranslateMenuKey("menu.begin_game", MenuLanguage::French) == "COMMENCER", "stable French ID");

    failures += Check(TranslateMenuKey("menu.language_switch.label", MenuLanguage::Japanese) == "メニュー: 日本語", "switch Japanese");
    failures += Check(TranslateMenuKey("menu.language_switch.label", MenuLanguage::French) == "MENU : FRANÇAIS", "switch French");

    const std::string multiline = "offline\nbots";
    failures += Check(TranslateMenuText(multiline, MenuLanguage::Japanese) == "オフライン\nBOT対戦", "Japanese multiline text");
    failures += Check(TranslateMenuText(multiline, MenuLanguage::French) == "hors ligne\nbots", "French multiline text");

    const std::string unknown = "PLAYER_CUSTOM_123";
    failures += Check(TranslateMenuText(unknown, MenuLanguage::Japanese) == unknown, "unknown source fallback");
    failures += Check(TranslateMenuText(unknown, MenuLanguage::French) == unknown, "unknown French fallback");
    failures += Check(TranslateMenuKey("menu.unknown", MenuLanguage::Japanese).empty(), "unknown ID");

    if (failures == 0) std::cout << "Menu localization: all checks passed\n";
    return failures == 0 ? 0 : 1;
}

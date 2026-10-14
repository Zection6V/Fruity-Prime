#include "../Mods/ClassicMenu/MenuLocalization.hpp"

#include <cassert>
#include <string>

using namespace MphRead::Mods::ClassicMenu;

int main()
{
    assert(ParseMenuLanguage("en") == MenuLanguage::English);
    assert(ParseMenuLanguage("ja") == MenuLanguage::Japanese);
    assert(ParseMenuLanguage("fr") == MenuLanguage::French);
    assert(ParseMenuLanguage("unknown") == MenuLanguage::English);

    const std::string begin = "BEGIN GAME";
    assert(TranslateMenuText(begin, MenuLanguage::English) == begin);
    assert(TranslateMenuText(begin, MenuLanguage::Japanese) == "ゲーム開始");
    assert(TranslateMenuText(begin, MenuLanguage::French) == "COMMENCER");

    const std::string quit = "QUIT";
    assert(TranslateMenuText(quit, MenuLanguage::Japanese) == "終了");
    assert(TranslateMenuText(quit, MenuLanguage::French) == "QUITTER");

    const std::string multiline = "offline\nbots";
    assert(TranslateMenuText(multiline, MenuLanguage::Japanese) == "オフライン\nBOT対戦");
    assert(TranslateMenuText(multiline, MenuLanguage::French) == "hors ligne\nbots");

    const std::string unknown = "PLAYER_CUSTOM_123";
    assert(TranslateMenuText(unknown, MenuLanguage::Japanese) == unknown);
    assert(TranslateMenuText(unknown, MenuLanguage::French) == unknown);
    return 0;
}

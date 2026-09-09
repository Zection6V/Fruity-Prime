// Generated from src/MphRead/Metadata/FrontendMeta.cs by
// tools/generate-native-frontend-meta.py. Do not hand-edit.
#include "FrontendMeta.hpp"

namespace fruityprime::metadata {

const ModelMetadata Ad2Dm2("ad2_dm2", MetaDir::Stage);

const std::array<FrontendModelEntry, HudModelsCount> HudModels{{
    {"unit1_land_cockpit", ModelMetadata::with_paths("unit1_land_cockpit", R"(hud\unit1_land_cockpit_model.bin)", std::nullopt, std::nullopt)},
    {"unit2_land_cockpit", ModelMetadata::with_paths("unit2_land_cockpit", R"(hud\unit2_land_cockpit_model.bin)", std::nullopt, std::nullopt)},
    {"unit3_land_cockpit", ModelMetadata::with_paths("unit3_land_cockpit", R"(hud\unit3_land_cockpit_model.bin)", std::nullopt, std::nullopt)},
    {"unit4_land_cockpit", ModelMetadata::with_paths("unit4_land_cockpit", R"(hud\unit4_land_cockpit_model.bin)", std::nullopt, std::nullopt)},
    {"gorea_land_cockpit", ModelMetadata::with_paths("gorea_land_cockpit", R"(hud\gorea_land_cockpit_model.bin)", std::nullopt, std::nullopt)},
    {"unit1_1nav", ModelMetadata::with_paths("unit1_1nav", R"(hud\unit1_1nav_model.bin)", std::nullopt, std::nullopt)},
    {"unit1_2nav", ModelMetadata::with_paths("unit1_2nav", R"(hud\unit1_2nav_model.bin)", std::nullopt, std::nullopt)},
    {"unit2_1nav", ModelMetadata::with_paths("unit2_1nav", R"(hud\unit2_1NAV_Model.bin)", std::nullopt, std::nullopt)},
    {"unit2_2nav", ModelMetadata::with_paths("unit2_2nav", R"(hud\unit2_2nav_model.bin)", std::nullopt, std::nullopt)},
    {"unit3_1nav", ModelMetadata::with_paths("unit3_1nav", R"(hud\unit3_1nav_model.bin)", std::nullopt, std::nullopt)},
    {"unit3_2nav", ModelMetadata::with_paths("unit3_2nav", R"(hud\unit3_2nav_model.bin)", std::nullopt, std::nullopt)},
    {"unit4_1nav", ModelMetadata::with_paths("unit4_1nav", R"(hud\unit4_1nav_model.bin)", std::nullopt, std::nullopt)},
    {"Door_NAV", ModelMetadata("Door_NAV", MetaDir::Hud)},
    {"PlayerPos_NAV", ModelMetadata("PlayerPos_NAV", MetaDir::Hud, std::string{"PlayerPos"})},
    {"damage", ModelMetadata("damage", MetaDir::Hud)},
    {"icons", ModelMetadata("icons", R"(models\icons_Tex.bin)", MetaDir::Hud)},
    {"hud_icon_arrow", ModelMetadata("hud_icon_arrow", MetaDir::Hud)},
    {"hud_icon_nodes", ModelMetadata("hud_icon_nodes", MetaDir::Hud)},
    {"hud_icon_octolith", ModelMetadata("hud_icon_octolith", MetaDir::Hud)},
    {"hud_icon_player", ModelMetadata("hud_icon_player", MetaDir::Hud)}
}};

const std::array<FrontendModelEntry, TouchToStartModelsCount> TouchToStartModels{{
    {"touch_bg", ModelMetadata("touch_bg", MetaDir::TouchToStart)}
}};

const std::array<FrontendModelEntry, MultiplayerModelsCount> MultiplayerModels{{
    {"bigdeathmatch", ModelMetadata("bigdeathmatch", MetaDir::Multiplayer)},
    {"bounty", ModelMetadata("bounty", MetaDir::Multiplayer)},
    {"capture", ModelMetadata("capture", MetaDir::Multiplayer)},
    {"challenge", ModelMetadata("challenge", MetaDir::Multiplayer)},
    {"friendconfig", ModelMetadata("friendconfig", MetaDir::Multiplayer)},
    {"med_highlight", ModelMetadata("med_highlight", MetaDir::Multiplayer)},
    {"multiplayer", ModelMetadata("multiplayer", MetaDir::Multiplayer)},
    {"nodes", ModelMetadata("nodes", MetaDir::Multiplayer)},
    {"primehunter", ModelMetadata("primehunter", MetaDir::Multiplayer)},
    {"rivalradar", ModelMetadata("rivalradar", MetaDir::Multiplayer)},
    {"singlecart", ModelMetadata("singlecart", MetaDir::Multiplayer)},
    {"survival", ModelMetadata("survival", MetaDir::Multiplayer)},
    {"wificonfig", ModelMetadata("wificonfig", MetaDir::Multiplayer)},
    {"wifi", ModelMetadata("wifi", MetaDir::Multiplayer)}
}};

const std::array<FrontendModelEntry, LogoModelsCount> LogoModels{{
    {"flare", ModelMetadata("flare", MetaDir::Logo)},
    {"flare2", ModelMetadata("flare2", MetaDir::Logo)},
    {"flare3", ModelMetadata("flare3", MetaDir::Logo)},
    {"logo1", ModelMetadata("logo1", MetaDir::Logo)},
    {"logo2", ModelMetadata("logo2", MetaDir::Logo)},
    {"logo3", ModelMetadata("logo3", MetaDir::Logo)},
    {"logos", ModelMetadata("logos", MetaDir::Logo)},
    {"name", ModelMetadata("name", MetaDir::Logo)},
    {"nameflare", ModelMetadata("nameflare", MetaDir::Logo)},
    {"nst", ModelMetadata("nst", MetaDir::Logo)},
    {"whitelogo", ModelMetadata("whitelogo", MetaDir::Logo)}
}};

const std::array<FrontendModelEntry, FrontendModelsCount> FrontendModels{{
    {"big_kanden", ModelMetadata("big_kanden", MetaDir::CharSelect)},
    {"big_noxus", ModelMetadata("big_noxus", MetaDir::CharSelect)},
    {"big_samus", ModelMetadata("big_samus", MetaDir::CharSelect)},
    {"big_spire", ModelMetadata("big_spire", MetaDir::CharSelect)},
    {"big_sylux", ModelMetadata("big_sylux", MetaDir::CharSelect)},
    {"big_trace", ModelMetadata("big_trace", MetaDir::CharSelect)},
    {"big_weavel", ModelMetadata("big_weavel", MetaDir::CharSelect)},
    {"character_grid", ModelMetadata("character_grid", MetaDir::CharSelect)},
    {"kandenoff", ModelMetadata("kandenoff", MetaDir::CharSelect)},
    {"kanden", ModelMetadata("kanden", MetaDir::CharSelect)},
    {"noxusoff", ModelMetadata("noxusoff", MetaDir::CharSelect)},
    {"noxus", ModelMetadata("noxus", MetaDir::CharSelect)},
    {"samus", ModelMetadata("samus", MetaDir::CharSelect)},
    {"spireoff", ModelMetadata("spireoff", MetaDir::CharSelect)},
    {"spire", ModelMetadata("spire", MetaDir::CharSelect)},
    {"syluxoff", ModelMetadata("syluxoff", MetaDir::CharSelect)},
    {"sylux", ModelMetadata("sylux", MetaDir::CharSelect)},
    {"traceoff", ModelMetadata("traceoff", MetaDir::CharSelect)},
    {"trace", ModelMetadata("trace", MetaDir::CharSelect)},
    {"weaveloff", ModelMetadata("weaveloff", MetaDir::CharSelect)},
    {"weavel", ModelMetadata("weavel", MetaDir::CharSelect)},
    {"create", ModelMetadata("create", MetaDir::CreateJoin)},
    {"join_highlight", ModelMetadata("join_highlight", MetaDir::CreateJoin)},
    {"join", ModelMetadata("join", MetaDir::CreateJoin)},
    {"redbar", ModelMetadata("redbar", MetaDir::CreateJoin)},
    {"arrows_option", ModelMetadata("arrows_option", MetaDir::GameOption)},
    {"arrows_type", ModelMetadata("arrows_type", MetaDir::GameOption)},
    {"box_arrowsdouble", ModelMetadata("box_arrowsdouble", MetaDir::GameOption)},
    {"box_arrows", ModelMetadata("box_arrows", MetaDir::GameOption)},
    {"box_type", ModelMetadata("box_type", MetaDir::GameOption)},
    {"cancel", ModelMetadata("cancel", MetaDir::GameOption)},
    {"chat", ModelMetadata("chat", MetaDir::GameOption)},
    {"connected", ModelMetadata("connected", MetaDir::GameOption)},
    {"empty", ModelMetadata("empty", MetaDir::GameOption)},
    {"headphones", ModelMetadata("headphones", MetaDir::GameOption)},
    {"highlight_arrowleft", ModelMetadata("highlight_arrowleft", MetaDir::GameOption)},
    {"highlight_arrowright", ModelMetadata("highlight_arrowright", MetaDir::GameOption)},
    {"ok", ModelMetadata("ok", MetaDir::GameOption)},
    {"playmask", ModelMetadata("playmask", MetaDir::GameOption)},
    {"play", ModelMetadata("play", MetaDir::GameOption)},
    {"stereo", ModelMetadata("stereo", MetaDir::GameOption)},
    {"surround", ModelMetadata("surround", MetaDir::GameOption)},
    {"wifiicon", ModelMetadata("wifiicon", MetaDir::GameOption)},
    {"wifionlineicon", ModelMetadata("wifionlineicon", MetaDir::GameOption)},
    {"alimbicswirl", ModelMetadata("alimbicswirl", MetaDir::GamersCard)},
    {"blackstar", ModelMetadata("blackstar", MetaDir::GamersCard)},
    {"bronzemedal", ModelMetadata("bronzemedal", MetaDir::GamersCard)},
    {"bronzestar", ModelMetadata("bronzestar", MetaDir::GamersCard)},
    {"cardbgEURO", ModelMetadata("cardbgEURO", MetaDir::GamersCard)},
    {"cardbgJAP", ModelMetadata("cardbgJAP", MetaDir::GamersCard)},
    {"cardbgUS", ModelMetadata("cardbgUS", MetaDir::GamersCard)},
    {"cardbg", ModelMetadata("cardbg", MetaDir::GamersCard)},
    {"cardheadergold", ModelMetadata("cardheadergold", MetaDir::GamersCard)},
    {"cardmessage", ModelMetadata("cardmessage", MetaDir::GamersCard)},
    {"frame200gold", ModelMetadata("frame200gold", MetaDir::GamersCard)},
    {"frame200", ModelMetadata("frame200", MetaDir::GamersCard)},
    {"goldmedal", ModelMetadata("goldmedal", MetaDir::GamersCard)},
    {"goldstar", ModelMetadata("goldstar", MetaDir::GamersCard)},
    {"kandenmost", ModelMetadata("kandenmost", MetaDir::GamersCard)},
    {"lricons", ModelMetadata("lricons", MetaDir::GamersCard)},
    {"noxmost", ModelMetadata("noxmost", MetaDir::GamersCard)},
    {"octolith", ModelMetadata("octolith", MetaDir::GamersCard)},
    {"redstar", ModelMetadata("redstar", MetaDir::GamersCard)},
    {"samusmost", ModelMetadata("samusmost", MetaDir::GamersCard)},
    {"silvermedal", ModelMetadata("silvermedal", MetaDir::GamersCard)},
    {"silverstar", ModelMetadata("silverstar", MetaDir::GamersCard)},
    {"spiremost", ModelMetadata("spiremost", MetaDir::GamersCard)},
    {"syluxmost", ModelMetadata("syluxmost", MetaDir::GamersCard)},
    {"tab1", ModelMetadata("tab1", MetaDir::GamersCard)},
    {"tab2", ModelMetadata("tab2", MetaDir::GamersCard)},
    {"tab3", ModelMetadata("tab3", MetaDir::GamersCard)},
    {"tab4", ModelMetadata("tab4", MetaDir::GamersCard)},
    {"tab5", ModelMetadata("tab5", MetaDir::GamersCard)},
    {"textarea", ModelMetadata("textarea", MetaDir::GamersCard)},
    {"tracemost", ModelMetadata("tracemost", MetaDir::GamersCard)},
    {"weavelmost", ModelMetadata("weavelmost", MetaDir::GamersCard)},
    {"EURO", ModelMetadata("EURO", MetaDir::Keyboard)},
    {"hl_key", ModelMetadata("hl_key", MetaDir::Keyboard)},
    {"JAP_1", ModelMetadata("JAP_1", MetaDir::Keyboard)},
    {"JAP_2", ModelMetadata("JAP_2", MetaDir::Keyboard)},
    {"keyboardmask", ModelMetadata("keyboardmask", MetaDir::Keyboard)},
    {"keytoggle", ModelMetadata("keytoggle", MetaDir::Keyboard)},
    {"messagebox", ModelMetadata("messagebox", MetaDir::Keyboard)},
    {"US_caps", ModelMetadata("US_caps", MetaDir::Keyboard)},
    {"US_lower", ModelMetadata("US_lower", MetaDir::Keyboard)},
    {"US_upper", ModelMetadata("US_upper", MetaDir::Keyboard)},
    {"numback", ModelMetadata("numback", MetaDir::Keypad)},
    {"numkey", ModelMetadata("numkey", MetaDir::Keypad)},
    {"cover", ModelMetadata("cover", MetaDir::MoviePlayer)},
    {"hotspot", ModelMetadata("hotspot", MetaDir::MoviePlayer)},
    {"thumbnails1", ModelMetadata("thumbnails1", MetaDir::MoviePlayer)},
    {"thumbnails2", ModelMetadata("thumbnails2", MetaDir::MoviePlayer)},
    {"bluedot", ModelMetadata("bluedot", MetaDir::MultiMaster)},
    {"tab", ModelMetadata("tab", MetaDir::MultiMaster)},
    {"topsettings", ModelMetadata("topsettings", MetaDir::MultiMaster)},
    {"topstatus", ModelMetadata("topstatus", MetaDir::MultiMaster)},
    {"dmleft", ModelMetadata("dmleft", MetaDir::PaxControls)},
    {"dmright", ModelMetadata("dmright", MetaDir::PaxControls)},
    {"selectAoff", ModelMetadata("selectAoff", MetaDir::PaxControls)},
    {"selectAon", ModelMetadata("selectAon", MetaDir::PaxControls)},
    {"stylusleft", ModelMetadata("stylusleft", MetaDir::PaxControls)},
    {"stylusright", ModelMetadata("stylusright", MetaDir::PaxControls)},
    {"popup", ModelMetadata("popup", MetaDir::Popup)},
    {"blackarrowleft", ModelMetadata("blackarrowleft", MetaDir::Results)},
    {"blackarrowright", ModelMetadata("blackarrowright", MetaDir::Results)},
    {"crossbar", ModelMetadata("crossbar", MetaDir::Results)},
    {"lightning", ModelMetadata("lightning", MetaDir::Results)},
    {"medalbg", ModelMetadata("medalbg", MetaDir::Results)},
    {"medalbronze", ModelMetadata("medalbronze", MetaDir::Results)},
    {"medalgold", ModelMetadata("medalgold", MetaDir::Results)},
    {"medallast", ModelMetadata("medallast", MetaDir::Results)},
    {"medalsilver", ModelMetadata("medalsilver", MetaDir::Results)},
    {"playagain", ModelMetadata("playagain", MetaDir::Results)},
    {"quit", ModelMetadata("quit", MetaDir::Results)},
    {"results", ModelMetadata("results", MetaDir::Results)},
    {"rivalboxoff", ModelMetadata("rivalboxoff", MetaDir::Results)},
    {"rivalboxon", ModelMetadata("rivalboxon", MetaDir::Results)},
    {"teamdivide", ModelMetadata("teamdivide", MetaDir::Results)},
    {"topbar", ModelMetadata("topbar", MetaDir::Results)},
    {"wincondition", ModelMetadata("wincondition", MetaDir::Results)},
    {"dssystem", ModelMetadata("dssystem", MetaDir::ScStartGame)},
    {"readybox", ModelMetadata("readybox", MetaDir::ScStartGame)},
    {"adbot", ModelMetadata("adbot", MetaDir::StartGame)},
    {"botminus", ModelMetadata("botminus", MetaDir::StartGame)},
    {"chatout", ModelMetadata("chatout", MetaDir::StartGame)},
    {"choicekanden", ModelMetadata("choicekanden", MetaDir::StartGame)},
    {"choicenoxus", ModelMetadata("choicenoxus", MetaDir::StartGame)},
    {"choicesamus", ModelMetadata("choicesamus", MetaDir::StartGame)},
    {"choicespire", ModelMetadata("choicespire", MetaDir::StartGame)},
    {"choicesylux", ModelMetadata("choicesylux", MetaDir::StartGame)},
    {"choicetrace", ModelMetadata("choicetrace", MetaDir::StartGame)},
    {"choiceweavel", ModelMetadata("choiceweavel", MetaDir::StartGame)},
    {"playergrid", ModelMetadata("playergrid", MetaDir::StartGame)},
    {"player_highlight", ModelMetadata("player_highlight", MetaDir::StartGame)},
    {"spicy_1", ModelMetadata("spicy_1", MetaDir::StartGame)},
    {"spicy_2", ModelMetadata("spicy_2", MetaDir::StartGame)},
    {"spicy_3", ModelMetadata("spicy_3", MetaDir::StartGame)},
    {"startgame_highlight", ModelMetadata("startgame_highlight", MetaDir::StartGame)},
    {"startgame", ModelMetadata("startgame", MetaDir::StartGame)},
    {"team_blue", ModelMetadata("team_blue", MetaDir::StartGame)},
    {"team_red", ModelMetadata("team_red", MetaDir::StartGame)},
    {"logofinal", ModelMetadata("logofinal", MetaDir::ToStart)},
    {"splashers", ModelMetadata("splashers", MetaDir::TouchToStart2)},
    {"touch_bg", ModelMetadata("touch_bg", MetaDir::TouchToStart2)},
    {"splitter", ModelMetadata("splitter", MetaDir::WifiCreate)},
    {"wififriend", ModelMetadata("wififriend", MetaDir::WifiCreate)},
    {"wifijoin", ModelMetadata("wifijoin", MetaDir::WifiCreate)},
    {"bigpanel", ModelMetadata("bigpanel", MetaDir::WifiGames)},
    {"creategame", ModelMetadata("creategame", MetaDir::WifiGames)},
    {"downarrow", ModelMetadata("downarrow", MetaDir::WifiGames)},
    {"friendbar_long", ModelMetadata("friendbar_long", MetaDir::WifiGames)},
    {"friendbar_short", ModelMetadata("friendbar_short", MetaDir::WifiGames)},
    {"gamepanel", ModelMetadata("gamepanel", MetaDir::WifiGames)},
    {"gotofriends", ModelMetadata("gotofriends", MetaDir::WifiGames)},
    {"gotogames", ModelMetadata("gotogames", MetaDir::WifiGames)},
    {"joingame", ModelMetadata("joingame", MetaDir::WifiGames)},
    {"locked", ModelMetadata("locked", MetaDir::WifiGames)},
    {"mainframe", ModelMetadata("mainframe", MetaDir::WifiGames)},
    {"namepanel", ModelMetadata("namepanel", MetaDir::WifiGames)},
    {"pending", ModelMetadata("pending", MetaDir::WifiGames)},
    {"rivalbar_long", ModelMetadata("rivalbar_long", MetaDir::WifiGames)},
    {"rivalbar_short", ModelMetadata("rivalbar_short", MetaDir::WifiGames)},
    {"secondframe", ModelMetadata("secondframe", MetaDir::WifiGames)},
    {"unlocked", ModelMetadata("unlocked", MetaDir::WifiGames)},
    {"uparrow", ModelMetadata("uparrow", MetaDir::WifiGames)},
    {"yourpanel", ModelMetadata("yourpanel", MetaDir::WifiGames)},
    {"audio", ModelMetadata("audio", MetaDir::MainMenu)},
    {"backhighlight", ModelMetadata("backhighlight", MetaDir::MainMenu)},
    {"backicon", ModelMetadata("backicon", MetaDir::MainMenu)},
    {"big_highlight", ModelMetadata("big_highlight", MetaDir::MainMenu)},
    {"blackdrop", ModelMetadata("blackdrop", MetaDir::MainMenu)},
    {"controls", ModelMetadata("controls", MetaDir::MainMenu)},
    {"copy", ModelMetadata("copy", MetaDir::MainMenu)},
    {"credits", ModelMetadata("credits", MetaDir::MainMenu)},
    {"delete", ModelMetadata("delete", MetaDir::MainMenu)},
    {"dialog_yesno", ModelMetadata("dialog_yesno", MetaDir::MainMenu)},
    {"disconnect", ModelMetadata("disconnect", MetaDir::MainMenu)},
    {"divider", ModelMetadata("divider", MetaDir::MainMenu)},
    {"edit", ModelMetadata("edit", MetaDir::MainMenu)},
    {"eraseall", ModelMetadata("eraseall", MetaDir::MainMenu)},
    {"esrb", ModelMetadata("esrb", MetaDir::MainMenu)},
    {"fileAbrackets", ModelMetadata("fileAbrackets", MetaDir::MainMenu)},
    {"fileAempty", ModelMetadata("fileAempty", MetaDir::MainMenu)},
    {"fileA", ModelMetadata("fileA", MetaDir::MainMenu)},
    {"fileBempty", ModelMetadata("fileBempty", MetaDir::MainMenu)},
    {"fileB", ModelMetadata("fileB", MetaDir::MainMenu)},
    {"fileCempty", ModelMetadata("fileCempty", MetaDir::MainMenu)},
    {"fileC", ModelMetadata("fileC", MetaDir::MainMenu)},
    {"gamebrackets", ModelMetadata("gamebrackets", MetaDir::MainMenu)},
    {"intogamefade", ModelMetadata("intogamefade", MetaDir::MainMenu)},
    {"microphone", ModelMetadata("microphone", MetaDir::MainMenu)},
    {"movies", ModelMetadata("movies", MetaDir::MainMenu)},
    {"multiplayer", ModelMetadata("multiplayer", MetaDir::MainMenu)},
    {"options", ModelMetadata("options", MetaDir::MainMenu)},
    {"orange", ModelMetadata("orange", MetaDir::MainMenu)},
    {"records", ModelMetadata("records", MetaDir::MainMenu)},
    {"singleplayer", ModelMetadata("singleplayer", MetaDir::MainMenu)},
    {"small_highlight", ModelMetadata("small_highlight", MetaDir::MainMenu)},
    {"topdrop", ModelMetadata("topdrop", MetaDir::MainMenu)},
    {"toplogoR", ModelMetadata("toplogoR", MetaDir::MainMenu)},
    {"toplogo", ModelMetadata("toplogo", MetaDir::MainMenu)},
    {"whitelogo", ModelMetadata("whitelogo", MetaDir::MainMenu)},
    {"wifi1", ModelMetadata("wifi1", MetaDir::MainMenu)},
    {"wifi2", ModelMetadata("wifi2", MetaDir::MainMenu)},
    {"wifi3", ModelMetadata("wifi3", MetaDir::MainMenu)},
    {"wifi4", ModelMetadata("wifi4", MetaDir::MainMenu)},
    {"wireless1", ModelMetadata("wireless1", MetaDir::MainMenu)},
    {"wireless2", ModelMetadata("wireless2", MetaDir::MainMenu)},
    {"wireless3", ModelMetadata("wireless3", MetaDir::MainMenu)},
    {"wireless4", ModelMetadata("wireless4", MetaDir::MainMenu)},
    {"ad1", ModelMetadata("ad1", MetaDir::Stage)},
    {"ad1_dm1", ModelMetadata("ad1_dm1", MetaDir::Stage)},
    {"ad2", ModelMetadata("ad2", MetaDir::Stage)},
    {"ad2_dm1", ModelMetadata("ad2_dm1", MetaDir::Stage)},
    {"ctf1", ModelMetadata("ctf1", MetaDir::Stage)},
    {"ctf1_dm1", ModelMetadata("ctf1_dm1", MetaDir::Stage)},
    {"e3level", ModelMetadata("e3level", MetaDir::Stage)},
    {"goreab2", ModelMetadata("goreab2", MetaDir::Stage)},
    {"mp1", ModelMetadata("mp1", MetaDir::Stage)},
    {"mp2", ModelMetadata("mp2", MetaDir::Stage)},
    {"mp3", ModelMetadata("mp3", MetaDir::Stage)},
    {"mp4", ModelMetadata("mp4", MetaDir::Stage)},
    {"mp4_dm1", ModelMetadata("mp4_dm1", MetaDir::Stage)},
    {"mp5", ModelMetadata("mp5", MetaDir::Stage)},
    {"mp6", ModelMetadata("mp6", MetaDir::Stage)},
    {"mp7", ModelMetadata("mp7", MetaDir::Stage)},
    {"mp8", ModelMetadata("mp8", MetaDir::Stage)},
    {"mp9", ModelMetadata("mp9", MetaDir::Stage)},
    {"mp10", ModelMetadata("mp10", MetaDir::Stage)},
    {"mp11", ModelMetadata("mp11", MetaDir::Stage)},
    {"mp12", ModelMetadata("mp12", MetaDir::Stage)},
    {"mp13", ModelMetadata("mp13", MetaDir::Stage)},
    {"mp14", ModelMetadata("mp14", MetaDir::Stage)},
    {"random", ModelMetadata("random", MetaDir::Stage)},
    {"unit1land", ModelMetadata("unit1land", MetaDir::Stage)},
    {"unit2land", ModelMetadata("unit2land", MetaDir::Stage)},
    {"unit3land", ModelMetadata("unit3land", MetaDir::Stage)},
    {"unit4land", ModelMetadata("unit4land", MetaDir::Stage)},
    {"blackout", ModelMetadata("blackout", MetaDir::Stage)},
    {"screenshot", ModelMetadata("screenshot", MetaDir::Stage)},
    {"stage_left", ModelMetadata("stage_left", MetaDir::Stage)},
    {"stage_right", ModelMetadata("stage_right", MetaDir::Stage)},
    {"stageleft_highlight", ModelMetadata("stageleft_highlight", MetaDir::Stage)},
    {"stageright_highlight", ModelMetadata("stageright_highlight", MetaDir::Stage)}
}};

const ModelMetadata* find_hud_model(std::string_view name) noexcept {
    for (const auto& entry : HudModels) {
        if (entry.Key == name) {
            return &entry.Value;
        }
    }
    return nullptr;
}

const ModelMetadata* find_touchtostart_model(std::string_view name) noexcept {
    for (const auto& entry : TouchToStartModels) {
        if (entry.Key == name) {
            return &entry.Value;
        }
    }
    return nullptr;
}

const ModelMetadata* find_multiplayer_model(std::string_view name) noexcept {
    for (const auto& entry : MultiplayerModels) {
        if (entry.Key == name) {
            return &entry.Value;
        }
    }
    return nullptr;
}

const ModelMetadata* find_logo_model(std::string_view name) noexcept {
    for (const auto& entry : LogoModels) {
        if (entry.Key == name) {
            return &entry.Value;
        }
    }
    return nullptr;
}

const ModelMetadata* find_frontend_model(std::string_view name) noexcept {
    for (const auto& entry : FrontendModels) {
        if (entry.Key == name) {
            return &entry.Value;
        }
    }
    return nullptr;
}

} // namespace fruityprime::metadata

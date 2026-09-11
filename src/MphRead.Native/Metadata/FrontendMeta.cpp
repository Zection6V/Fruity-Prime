#include "FrontendMeta.hpp"
#include "Metadata.hpp"

#include <initializer_list>
#include <utility>

namespace MphRead
{
    namespace
    {
        using ModelMap = std::unordered_map<std::string, std::shared_ptr<ModelMetadata>>;

        void AddModels(ModelMap& models, MetaDir dir, std::initializer_list<const char*> names)
        {
            for (const char* name : names)
            {
                models.emplace(name, std::make_shared<ModelMetadata>(name, dir));
            }
        }

        ModelMap CreateHudModels()
        {
            ModelMap models;
            models.emplace("unit1_land_cockpit", std::make_shared<ModelMetadata>(
                "unit1_land_cockpit", R"(hud\unit1_land_cockpit_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit2_land_cockpit", std::make_shared<ModelMetadata>(
                "unit2_land_cockpit", R"(hud\unit2_land_cockpit_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit3_land_cockpit", std::make_shared<ModelMetadata>(
                "unit3_land_cockpit", R"(hud\unit3_land_cockpit_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit4_land_cockpit", std::make_shared<ModelMetadata>(
                "unit4_land_cockpit", R"(hud\unit4_land_cockpit_model.bin)", std::nullopt, std::nullopt));
            models.emplace("gorea_land_cockpit", std::make_shared<ModelMetadata>(
                "gorea_land_cockpit", R"(hud\gorea_land_cockpit_model.bin)", std::nullopt, std::nullopt));

            models.emplace("unit1_1nav", std::make_shared<ModelMetadata>(
                "unit1_1nav", R"(hud\unit1_1nav_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit1_2nav", std::make_shared<ModelMetadata>(
                "unit1_2nav", R"(hud\unit1_2nav_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit2_1nav", std::make_shared<ModelMetadata>(
                "unit2_1nav", R"(hud\unit2_1NAV_Model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit2_2nav", std::make_shared<ModelMetadata>(
                "unit2_2nav", R"(hud\unit2_2nav_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit3_1nav", std::make_shared<ModelMetadata>(
                "unit3_1nav", R"(hud\unit3_1nav_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit3_2nav", std::make_shared<ModelMetadata>(
                "unit3_2nav", R"(hud\unit3_2nav_model.bin)", std::nullopt, std::nullopt));
            models.emplace("unit4_1nav", std::make_shared<ModelMetadata>(
                "unit4_1nav", R"(hud\unit4_1nav_model.bin)", std::nullopt, std::nullopt));

            models.emplace("Door_NAV", std::make_shared<ModelMetadata>("Door_NAV", MetaDir::Hud));
            models.emplace("PlayerPos_NAV", std::make_shared<ModelMetadata>(
                "PlayerPos_NAV", MetaDir::Hud, std::optional<std::string>("PlayerPos")));
            models.emplace("damage", std::make_shared<ModelMetadata>("damage", MetaDir::Hud));
            models.emplace("icons", std::make_shared<ModelMetadata>(
                "icons", R"(models\icons_Tex.bin)", MetaDir::Hud));
            models.emplace("hud_icon_arrow", std::make_shared<ModelMetadata>("hud_icon_arrow", MetaDir::Hud));
            models.emplace("hud_icon_nodes", std::make_shared<ModelMetadata>("hud_icon_nodes", MetaDir::Hud));
            models.emplace("hud_icon_octolith", std::make_shared<ModelMetadata>("hud_icon_octolith", MetaDir::Hud));
            models.emplace("hud_icon_player", std::make_shared<ModelMetadata>("hud_icon_player", MetaDir::Hud));
            return models;
        }

        ModelMap CreateTouchToStartModels()
        {
            ModelMap models;
            AddModels(models, MetaDir::TouchToStart, {"touch_bg"});
            return models;
        }

        ModelMap CreateMultiplayerModels()
        {
            ModelMap models;
            AddModels(models, MetaDir::Multiplayer,
            {
                "bigdeathmatch", "bounty", "capture", "challenge", "friendconfig", "med_highlight",
                "multiplayer", "nodes", "primehunter", "rivalradar", "singlecart", "survival",
                "wificonfig", "wifi"
            });
            return models;
        }

        ModelMap CreateLogoModels()
        {
            ModelMap models;
            AddModels(models, MetaDir::Logo,
            {
                "flare", "flare2", "flare3", "logo1", "logo2", "logo3", "logos", "name",
                "nameflare", "nst", "whitelogo"
            });
            return models;
        }

        ModelMap CreateFrontendModels()
        {
            ModelMap models;

            AddModels(models, MetaDir::CharSelect,
            {
                "big_kanden", "big_noxus", "big_samus", "big_spire", "big_sylux", "big_trace",
                "big_weavel", "character_grid", "kandenoff", "kanden", "noxusoff", "noxus", "samus",
                "spireoff", "spire", "syluxoff", "sylux", "traceoff", "trace", "weaveloff", "weavel"
            });

            AddModels(models, MetaDir::CreateJoin,
            {
                "create", "join_highlight", "join", "redbar"
            });

            AddModels(models, MetaDir::GameOption,
            {
                "arrows_option", "arrows_type", "box_arrowsdouble", "box_arrows", "box_type", "cancel",
                "chat", "connected", "empty", "headphones", "highlight_arrowleft", "highlight_arrowright",
                "ok", "playmask", "play", "stereo", "surround", "wifiicon", "wifionlineicon"
            });

            AddModels(models, MetaDir::GamersCard,
            {
                "alimbicswirl", "blackstar", "bronzemedal", "bronzestar", "cardbgEURO", "cardbgJAP",
                "cardbgUS", "cardbg", "cardheadergold", "cardmessage", "frame200gold", "frame200",
                "goldmedal", "goldstar", "kandenmost", "lricons", "noxmost", "octolith", "redstar",
                "samusmost", "silvermedal", "silverstar", "spiremost", "syluxmost", "tab1", "tab2",
                "tab3", "tab4", "tab5", "textarea", "tracemost", "weavelmost"
            });

            AddModels(models, MetaDir::Keyboard,
            {
                "EURO", "hl_key", "JAP_1", "JAP_2", "keyboardmask", "keytoggle", "messagebox",
                "US_caps", "US_lower", "US_upper"
            });

            AddModels(models, MetaDir::Keypad,
            {
                "numback", "numkey"
            });

            AddModels(models, MetaDir::MoviePlayer,
            {
                "cover", "hotspot", "thumbnails1", "thumbnails2"
            });

            AddModels(models, MetaDir::MultiMaster,
            {
                "bluedot", "tab", "topsettings", "topstatus"
            });

            AddModels(models, MetaDir::PaxControls,
            {
                "dmleft", "dmright", "selectAoff", "selectAon", "stylusleft", "stylusright"
            });

            AddModels(models, MetaDir::Popup,
            {
                "popup"
            });

            AddModels(models, MetaDir::Results,
            {
                "blackarrowleft", "blackarrowright", "crossbar", "lightning", "medalbg", "medalbronze",
                "medalgold", "medallast", "medalsilver", "playagain", "quit", "results", "rivalboxoff",
                "rivalboxon", "teamdivide", "topbar", "wincondition"
            });

            AddModels(models, MetaDir::ScStartGame,
            {
                "dssystem", "readybox"
            });

            AddModels(models, MetaDir::StartGame,
            {
                "adbot", "botminus", "chatout", "choicekanden", "choicenoxus", "choicesamus", "choicespire",
                "choicesylux", "choicetrace", "choiceweavel", "playergrid", "player_highlight", "spicy_1",
                "spicy_2", "spicy_3", "startgame_highlight", "startgame", "team_blue", "team_red"
            });

            AddModels(models, MetaDir::ToStart,
            {
                "logofinal"
            });

            AddModels(models, MetaDir::TouchToStart2,
            {
                "splashers", "touch_bg"
            });

            AddModels(models, MetaDir::WifiCreate,
            {
                "splitter", "wififriend", "wifijoin"
            });

            AddModels(models, MetaDir::WifiGames,
            {
                "bigpanel", "creategame", "downarrow", "friendbar_long", "friendbar_short", "gamepanel",
                "gotofriends", "gotogames", "joingame", "locked", "mainframe", "namepanel", "pending",
                "rivalbar_long", "rivalbar_short", "secondframe", "unlocked", "uparrow", "yourpanel"
            });

            AddModels(models, MetaDir::MainMenu,
            {
                "audio", "backhighlight", "backicon", "big_highlight", "blackdrop", "controls", "copy",
                "credits", "delete", "dialog_yesno", "disconnect", "divider", "edit", "eraseall", "esrb",
                "fileAbrackets", "fileAempty", "fileA", "fileBempty", "fileB", "fileCempty", "fileC",
                "gamebrackets", "intogamefade", "microphone", "movies", "multiplayer", "options", "orange",
                "records", "singleplayer", "small_highlight", "topdrop", "toplogoR", "toplogo", "whitelogo",
                "wifi1", "wifi2", "wifi3", "wifi4", "wireless1", "wireless2", "wireless3", "wireless4"
            });

            AddModels(models, MetaDir::Stage,
            {
                "ad1", "ad1_dm1", "ad2", "ad2_dm1", "ctf1", "ctf1_dm1", "e3level", "goreab2",
                "mp1", "mp2", "mp3", "mp4", "mp4_dm1", "mp5", "mp6", "mp7", "mp8", "mp9",
                "mp10", "mp11", "mp12", "mp13", "mp14", "random", "unit1land", "unit2land",
                "unit3land", "unit4land", "blackout", "screenshot", "stage_left", "stage_right",
                "stageleft_highlight", "stageright_highlight"
            });

            return models;
        }
    }

    namespace Metadata
    {
        MovieInfo::MovieInfo(std::optional<std::string> topScreenPath, std::optional<std::string> bottomScreenPath)
            : TopScreenPath(std::move(topScreenPath)), BottomScreenPath(std::move(bottomScreenPath))
        {
        }

        const std::shared_ptr<ModelMetadata> Ad2Dm2
            = std::make_shared<ModelMetadata>("ad2_dm2", MetaDir::Stage);

        const std::vector<std::string> NavMapModelNames =
        {
            "unit1_1nav",
            "unit1_2nav",
            "unit2_1nav",
            "unit2_2nav",
            "unit3_1nav",
            "unit3_2nav",
            "unit4_1nav"
        };

        const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> HudModels = CreateHudModels();
        const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> TouchToStartModels = CreateTouchToStartModels();
        const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> MultiplayerModels = CreateMultiplayerModels();
        const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> LogoModels = CreateLogoModels();
        const std::unordered_map<std::string, std::shared_ptr<ModelMetadata>> FrontendModels = CreateFrontendModels();

        const std::vector<std::shared_ptr<MovieInfo>> MovieFiles =
        {
            /*  0 */ std::make_shared<MovieInfo>(R"(movies\01_top.vx)", R"(movies\01_bot.vx)"),
            /*  1 */ std::make_shared<MovieInfo>(R"(movies\02_top.vx)", R"(movies\02_bot.vx)"),
            /*  2 */ std::make_shared<MovieInfo>(R"(movies\03_top.vx)", R"(movies\03_bot.vx)"),
            /*  3 */ std::make_shared<MovieInfo>(R"(movies\04.vx)"),
            /*  4 */ std::make_shared<MovieInfo>(R"(movies\05.vx)"),
            /*  5 */ std::make_shared<MovieInfo>(R"(movies\06.vx)"),
            /*  6 */ std::make_shared<MovieInfo>(R"(movies\07.vx)"),
            /*  7 */ std::make_shared<MovieInfo>(R"(movies\08.vx)"),
            /*  8 */ std::make_shared<MovieInfo>(R"(movies\09.vx)"),
            /*  9 */ std::make_shared<MovieInfo>(R"(movies\10.vx)"),
            /* 10 */ std::make_shared<MovieInfo>(R"(movies\11.vx)"),
            /* 11 */ std::make_shared<MovieInfo>(R"(movies\12_top.vx)", R"(movies\12_bot.vx)"),
            /* 12 */ std::make_shared<MovieInfo>(R"(movies\13.vx)"),
            /* 13 */ nullptr,
            /* 14 */ std::make_shared<MovieInfo>(R"(movies\15_top.vx)", R"(movies\15_bot.vx)"),
            /* 15 */ std::make_shared<MovieInfo>(R"(movies\16_top.vx)", R"(movies\16_bot.vx)"),
            /* 16 */ std::make_shared<MovieInfo>(R"(movies\17_top.vx)", R"(movies\17_bot.vx)"),
            /* 17 */ std::make_shared<MovieInfo>(R"(movies\18_top.vx)", R"(movies\18_bot.vx)"),
            /* 18 */ std::make_shared<MovieInfo>(R"(movies\19_top.vx)", R"(movies\19_bot.vx)"),
            /* 19 */ std::make_shared<MovieInfo>(R"(movies\20_top.vx)", R"(movies\20_bot.vx)"),
            /* 20 */ std::make_shared<MovieInfo>(R"(movies\21_top.vx)", R"(movies\21_bot.vx)"),
            /* 21 */ std::make_shared<MovieInfo>(R"(movies\22_top.vx)", R"(movies\22_bot.vx)"),
            /* 22 */ std::make_shared<MovieInfo>(R"(movies\23_top.vx)", R"(movies\23_bot.vx)"),
            /* 23 */ std::make_shared<MovieInfo>(R"(movies\24_top.vx)", R"(movies\24_bot.vx)"),
            /* 24 */ std::make_shared<MovieInfo>(R"(movies\25_top.vx)", R"(movies\25_bot.vx)"),
            /* 25 */ std::make_shared<MovieInfo>(R"(movies\26_top.vx)", R"(movies\26_bot.vx)"),
            /* 26 */ std::make_shared<MovieInfo>(R"(movies\27_top.vx)", R"(movies\27_bot.vx)"),
            /* 27 */ std::make_shared<MovieInfo>(R"(movies\28_top.vx)", R"(movies\28_bot.vx)"),
            /* 28 */ std::make_shared<MovieInfo>(R"(movies\29_top.vx)", R"(movies\29_bot.vx)"),
            /* 29 */ std::make_shared<MovieInfo>(R"(movies\30_top.vx)", R"(movies\30_bot.vx)"),
            /* 30 */ std::make_shared<MovieInfo>(R"(movies\31_top.vx)", R"(movies\31_bot.vx)"),
            /* 31 */ std::make_shared<MovieInfo>(R"(movies\32_top.vx)", R"(movies\32_bot.vx)"),
            /* 32 */ std::make_shared<MovieInfo>(R"(movies\33_top.vx)", R"(movies\33_bot.vx)"),
            /* 33 */ std::make_shared<MovieInfo>(R"(movies\34_top.vx)", R"(movies\34_bot.vx)"),
            /* 34 */ nullptr,
            /* 35 */ std::make_shared<MovieInfo>(R"(movies\36_top.vx)", R"(movies\36_bot.vx)")
        };

        const std::vector<std::string> MovieDisplayInfo =
        {
            "Opening (01_top/01_bot) - 0",
            "Story Intro (02_top/02_bot) - 1",
            "Good Ending (03_top/03_bot) - 2",
            "Alinos Landing (04_top) - 3",
            "Alinos Takeoff (05_top) - 4",
            "Celestial Archives Landing (06_top) - 5",
            "Celestial Archives Takeoff (07_top) - 6",
            "Arcterra Landing (08_top) - 7",
            "Arcterra Takeoff (09_top) - 8",
            "VDO Landing (10_top) - 9",
            "VDO Takeoff (11_top) - 10",
            "Oubliette Unlock (12_top/12_bot) - 11",
            "Oubliette Landing (13_top) - 12",
            "Unused (14_top/14_bot) - 13",
            "Octolith Obtained(15_top/15_bot) - 14",
            "Cretaphid V1 Intro (16_top/16_bot) - 15",
            "Cretaphid V1 Defeat (17_top/17_bot) - 16",
            "Cretaphid V2 Intro (18_top/18_bot) - 17",
            "Cretaphid V2 Defeat (19_top/19_bot) - 18",
            "Cretaphid V3 Intro (20_top/20_bot) - 19",
            "Cretaphid V3 Defeat (21_top/21_bot) - 20",
            "Cretaphid V4 Intro (22_top/22_bot) - 21",
            "Cretaphid V4 Defeat (23_top/23_bot) - 22",
            "Slench 1 Intro (24_top/24_bot) - 23",
            "Slench 1 Defeat (25_top/25_bot) - 24",
            "Slench 2 Intro (26_top/26_bot) - 25",
            "Slench 2 Defeat (27_top/27_bot) - 26",
            "Slench 3 Intro (28_top/28_bot) - 27",
            "Slench 3 Defeat (29_top/29_bot) - 28",
            "Slench 4 Intro (30_top/30_bot) - 29",
            "Slench 4 Defeat (31_top/31_bot) - 30",
            "Gorea Intro (32_top/32_bot) - 31",
            "Bad Ending Part 1 (33_top/33_bot) - 32",
            "Gorea 2 Intro (34_top/34_bot) - 33",
            "Unused (35_top/35_bot) - 34",
            "Bad Ending Part 2 (36_top/36_bot) - 35"
        };
    }
}

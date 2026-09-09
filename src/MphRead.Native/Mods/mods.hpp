#pragma once

#include "Mods/branding.hpp"
#include "Mods/Update/build_version.hpp"
#include "Mods/chat.hpp"
#include "Mods/debug_log.hpp"
#include "Mods/Network/demo.hpp"
#include "Mods/Network/dedicated_server.hpp"
#include "Mods/Launcher/Portable/adventure_save.hpp"
#include "Mods/Launcher/Portable/game_files.hpp"
#include "Mods/Input/input.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"
#include "Mods/Launcher/Portable/launch_plan.hpp"
#include "Mods/Network/master_client.hpp"
#include "Mods/Network/master_server.hpp"
#include "Mods/Network/match_client.hpp"
#include "Entities/match_flow.hpp"
#include "Mods/Launcher/Portable/match_start.hpp"
#include "Mods/MapGen/mapgen.hpp"
#include "Mods/Network/net_client.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Mods/Network/net_transport.hpp"
#include "Mods/render_options.hpp"
#include "Mods/Launcher/Portable/setup_progress.hpp"
#include "Mods/settings.hpp"
#include "Mods/Sound/sfx_mixer.hpp"
#include "Mods/window_mode.hpp"
#include "Mods/world_events.hpp"

namespace MphReadNative {
namespace Chat = ::fruityprime::chat;
namespace Demo = ::fruityprime::demo;
namespace Input = ::fruityprime::input;
namespace Launcher = ::fruityprime::launcher;
namespace Net = ::fruityprime::net;
namespace Settings = ::fruityprime::settings;
namespace MapGen = ::fruityprime::mapgen;
namespace Render = ::fruityprime::mods::render;
}

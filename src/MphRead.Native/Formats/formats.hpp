#pragma once

#include "Formats/ai_personality.hpp"
#include "Utility/archive.hpp"
#include "Formats/camera_sequence.hpp"
#include "Formats/collision_format.hpp"
#include "Formats/collision_query.hpp"
#include "Assets/compression.hpp"
#include "Formats/culling.hpp"
#include "Formats/entity_format.hpp"
#include "Formats/enemy_spawn.hpp"
#include "Formats/fixed.hpp"
#include "Formats/model_format.hpp"
#include "Formats/model_instance.hpp"
#include "Formats/movie.hpp"
#include "Assets/nds_rom.hpp"
#include "Formats/node_data.hpp"
#include "Formats/paths.hpp"
#include "Formats/Types.hpp"

namespace MphReadNative {
namespace Formats = ::fruityprime::formats;
namespace Model = ::fruityprime::model;
namespace Collision = ::fruityprime::collision;
namespace Compression = ::fruityprime::compression;
namespace Culling = ::fruityprime::culling;
namespace Node = ::fruityprime::node;
namespace AiPersonality = ::fruityprime::ai;
namespace CameraSequence = ::fruityprime::camera;
namespace EntityEnemy = ::fruityprime::enemy_spawn;
namespace Movie = ::fruityprime::movie;
namespace Types = ::fruityprime::formats;
}

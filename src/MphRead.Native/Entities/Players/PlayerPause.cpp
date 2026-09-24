#include "PlayerPause.hpp"

#include "../../Features.hpp"
#include "../../GameState.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Renderer.hpp"
#include "../../Scene.hpp"
#include "../../Sound/Music.hpp"
#include "../../Strings.hpp"
#include "../../Utility/Rng.hpp"
#include "../RoomEntity.hpp"
#include "PlayerEntity.hpp"
#include "PlayerHud.hpp"
#include "PlayerInput.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::UncheckedIncrement;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::CreateScale;
using ::OpenTK::Mathematics::IdentityMatrix;
using ::OpenTK::Mathematics::IsZero;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::MathHelper::RadiansToDegrees;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::ScaleVector;

namespace
{
    using MphRead::LightInfo;
    using MphRead::Model;
    using MphRead::ModelInstance;
    using MphRead::Node;
    using OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    [[noreturn]] void ThrowNullReference()
    {
        throw System::NullReferenceException();
    }

    [[nodiscard]] Model& RequireModel(ModelInstance& instance)
    {
        return RequireReference(instance.Model());
    }

    [[nodiscard]] std::vector<float> CopyManagedArray(const MphRead::ManagedArray<float>& values)
    {
        std::vector<float> result;
        result.reserve(values.Length());
        for (std::size_t i = 0; i < values.Length(); ++i)
        {
            result.push_back(values[i]);
        }
        return result;
    }

    [[nodiscard]] constexpr Vector4 Transform(Vector4 value, Matrix4 matrix) noexcept
    {
        return Vector4(
            value.X * matrix.M11 + value.Y * matrix.M21 + value.Z * matrix.M31 + value.W * matrix.M41,
            value.X * matrix.M12 + value.Y * matrix.M22 + value.Z * matrix.M32 + value.W * matrix.M42,
            value.X * matrix.M13 + value.Y * matrix.M23 + value.Z * matrix.M33 + value.W * matrix.M43,
            value.X * matrix.M14 + value.Y * matrix.M24 + value.Z * matrix.M34 + value.W * matrix.M44);
    }

    [[nodiscard]] Matrix4 CreateOrthographic(float width, float height, float zNear, float zFar) noexcept
    {
        return Matrix4(
            Vector4(2.0F / width, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 2.0F / height, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, -2.0F / (zFar - zNear), 0.0F),
            Vector4(0.0F, 0.0F, -(zFar + zNear) / (zFar - zNear), 1.0F));
    }

    [[nodiscard]] Matrix4 LookAt(Vector3 eye, Vector3 target, Vector3 up)
    {
        const Vector3 z = (eye - target).Normalized();
        const Vector3 x = Vector3::Cross(up, z).Normalized();
        const Vector3 y = Vector3::Cross(z, x).Normalized();
        return Matrix4(
            Vector4(x.X, y.X, z.X, 0.0F),
            Vector4(x.Y, y.Y, z.Y, 0.0F),
            Vector4(x.Z, y.Z, z.Z, 0.0F),
            Vector4(-Vector3::Dot(x, eye), -Vector3::Dot(y, eye), -Vector3::Dot(z, eye), 1.0F));
    }

    [[nodiscard]] float ProjectPosition(
        Vector3 position, Matrix4 viewMatrix, Matrix4 projectionMatrix, Vector2& projected) noexcept
    {
        const Vector4 view = Transform(Vector4(position, 1.0F), viewMatrix);
        const Vector4 clip = Transform(view, projectionMatrix);
        projected = Vector2(clip.X / clip.W, clip.Y / clip.W);
        return -view.Z;
    }

    [[nodiscard]] constexpr Vector3 Column0(Matrix4 matrix) noexcept
    {
        return Vector3(matrix.M11, matrix.M21, matrix.M31);
    }

    [[nodiscard]] constexpr Vector3 Column1(Matrix4 matrix) noexcept
    {
        return Vector3(matrix.M12, matrix.M22, matrix.M32);
    }

    void SetTranslation(Matrix4& matrix, Vector3 translation) noexcept
    {
        matrix.M41 = translation.X;
        matrix.M42 = translation.Y;
        matrix.M43 = translation.Z;
    }

    [[nodiscard]] bool StartsWith(std::string_view value, std::string_view prefix) noexcept
    {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    [[nodiscard]] bool TryParseConnectorId(std::string_view value, std::int32_t& result) noexcept
    {
        if (value.size() != 2)
        {
            result = 0;
            return false;
        }
        const char* first = value.data();
        const char* last = first + value.size();
        const auto parsed = std::from_chars(first, last, result);
        if (parsed.ec != std::errc{} || parsed.ptr != last)
        {
            result = 0;
            return false;
        }
        return true;
    }

    struct Utf8Character final
    {
        char32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] Utf8Character DecodeUtf8(std::string_view text, std::size_t offset) noexcept
    {
        const auto byte = static_cast<unsigned char>(text[offset]);
        if (byte < 0x80)
        {
            return {byte, 1};
        }
        std::size_t length = 0;
        char32_t value = 0;
        char32_t minimum = 0;
        if ((byte & 0xE0) == 0xC0)
        {
            length = 2;
            value = byte & 0x1F;
            minimum = 0x80;
        }
        else if ((byte & 0xF0) == 0xE0)
        {
            length = 3;
            value = byte & 0x0F;
            minimum = 0x800;
        }
        else if ((byte & 0xF8) == 0xF0)
        {
            length = 4;
            value = byte & 0x07;
            minimum = 0x10000;
        }
        else
        {
            return {0xFFFD, 1};
        }
        if (offset + length > text.size())
        {
            return {0xFFFD, 1};
        }
        for (std::size_t i = 1; i < length; ++i)
        {
            const auto continuation = static_cast<unsigned char>(text[offset + i]);
            if ((continuation & 0xC0) != 0x80)
            {
                return {0xFFFD, 1};
            }
            value = (value << 6) | (continuation & 0x3F);
        }
        if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        {
            return {0xFFFD, 1};
        }
        return {value, length};
    }

    [[nodiscard]] std::int32_t ManagedStringLength(const std::string& text) noexcept
    {
        std::int32_t length = 0;
        std::size_t offset = 0;
        while (offset < text.size())
        {
            const Utf8Character character = DecodeUtf8(text, offset);
            length = UncheckedIncrement(length);
            if (character.Value > 0xFFFF)
            {
                length = UncheckedIncrement(length);
            }
            offset += character.Length;
        }
        return length;
    }

    [[nodiscard]] MphRead::StorySave& RequireStorySave()
    {
        if (MphRead::GameState::StorySave == nullptr)
        {
            ThrowNullReference();
        }
        return *MphRead::GameState::StorySave;
    }
}

namespace MphRead::Entities
{
    using Hud::Align;
    using Hud::HudObjectLoopType;
    using Formats::Collision::CollisionInstance;

    std::int32_t PlayerEntity::_drawPauseState = 0;
    float PlayerEntity::_navTextTimer = 0.0F;
    bool PlayerEntity::_navLoading = false;

    const std::array<Vector3, 10> PlayerEntity::_navDoorColors{
        Vector3(230.0F / 255.0F, 230.0F / 255.0F, 230.0F / 255.0F),
        Vector3(1.0F, 1.0F, 0.0F),
        Vector3(247.0F / 255.0F, 148.0F / 255.0F, 82.0F / 255.0F),
        Vector3(0.0F, 1.0F, 0.0F),
        Vector3(1.0F, 0.0F, 0.0F),
        Vector3(165.0F / 255.0F, 74.0F / 255.0F, 1.0F),
        Vector3(1.0F, 132.0F / 255.0F, 0.0F),
        Vector3(0.0F, 132.0F / 255.0F, 1.0F),
        Vector3(230.0F / 255.0F, 230.0F / 255.0F, 230.0F / 255.0F),
        Vector3(165.0F / 255.0F, 165.0F / 255.0F, 165.0F / 255.0F)};

    const std::array<Vector3, 9> PlayerEntity::_navMapNodeOffsets{
        Vector3::Zero,
        Vector3(-80.87378F, 22.282959F, 205.73096F),
        Vector3::Zero,
        Vector3(0.0F, 73.0F, 0.0F),
        Vector3::Zero,
        Vector3(-136.7998F, -0.21191406F, 4.36499F),
        Vector3::Zero,
        Vector3::Zero,
        Vector3::Zero};

    const std::array<std::pair<std::int16_t, std::int16_t>, 8> PlayerEntity::_mapIconPositions{{
        {15, 102}, {15, 134}, {15, 38}, {15, 70}, {241, 38}, {241, 70}, {241, 102}, {241, 134}}};

    const std::array<std::pair<std::int16_t, std::int16_t>, 3> PlayerEntity::_mapDotOffsets{{
        {-10, -7}, {10, -7}, {0, 12}}};

    void PlayerEntity::SetUpMenuPauseHud()
    {
        EndWeaponMenu();
        _navTextTimer = 0.0F;
        _prevScrollingChars = 0;
        _drawPauseState = 1;
        if (GameState::InRoomTransition())
        {
            _navLoading = true;
        }
        else
        {
            SetUpMenuPauseMapNav();
            _navLoading = false;
        }
        Scene& scene = RequireReference(_scene);
        _pausedPrevBindingId1 = RequireReference(scene.Layer1Info()).BindingId;
        _pausedPrevAlpha1 = RequireReference(scene.Layer1Info()).Alpha;
        _pausedPrevMaskId = RequireReference(scene.Layer1Info()).MaskId;
        _pausedPrevBindingId2 = RequireReference(scene.Layer2Info()).BindingId;
        _pausedPrevAlpha2 = RequireReference(scene.Layer2Info()).Alpha;
        _pausedPrevBindingId3 = RequireReference(scene.Layer3Info()).BindingId;
        _pausedPrevBindingId4 = RequireReference(scene.Layer4Info()).BindingId;
        _pausedPrevBindingId5 = RequireReference(scene.Layer5Info()).BindingId;
        for (std::int32_t i = 0; i < 8; ++i)
        {
            const std::int32_t start = static_cast<std::int32_t>(Rng::GetRandomInt1(20));
            const std::int32_t afterAnim = static_cast<std::int32_t>(Rng::GetRandomInt1(6));
            RequireReference(ManagedAt(_mapOctolithInsts, i)).SetAnimation(
                start, 35, 36, afterAnim, HudObjectLoopType::Offset);
        }
        RequireReference(_mapLostOctolithInst).SetAnimation(0, 19, 20, true);
    }

    void PlayerEntity::SetUpMenuPauseMapNav()
    {
        _navMapDrawNode.reset();
        _navMapModelEnabled = false;
        _navDrawZoom = 0.75F;
        _navDrawRotX = RadiansToDegrees(std::atan2(-_facingVector.X, -_facingVector.Z));
        _navDrawRotY = 28.125F;
        _navPanTimer = 0.0F;
        _navCurRoomNodePos = Vector3::Zero;
        _navCurCenterNodePos = Vector3::Zero;
        _navInitRoomNodePos = Vector3::Zero;
        _navTargetPos = Vector3::Zero;
        _navPanOffset = Vector3::Zero;
        _pauseFrameCount = 0;
        if (NodeRef.RoomName.HasValue())
        {
            const std::string& roomName = *NodeRef.RoomName;
            std::int32_t area = RequireReference(_scene).AreaId() & ~1;
            for (std::int32_t i = 0; i < 2; ++i, ++area)
            {
                if (area >= 0 && area < static_cast<std::int32_t>(_navMapModels.size()))
                {
                    std::shared_ptr<ModelInstance> modelValue = ManagedAt(_navMapModels, area);
                    if (!modelValue)
                    {
                        continue;
                    }
                    ModelInstance& instance = *modelValue;
                    Model& model = RequireModel(instance);
                    const auto& nodes = RequireReference(model.Nodes);
                    std::shared_ptr<Node> roomNode{};
                    for (std::int32_t j = 0; j < static_cast<std::int32_t>(nodes.size()); ++j)
                    {
                        std::shared_ptr<Node> nodeValue = ManagedAt(nodes, j);
                        Node& node = RequireReference(nodeValue);
                        if (node.ParentIndex == 0 && StringEqualsOrdinalIgnoreCase(node.Name, roomName))
                        {
                            roomNode = std::move(nodeValue);
                            break;
                        }
                    }
                    if (!roomNode)
                    {
                        continue;
                    }
                    std::shared_ptr<Node> centerNode{};
                    for (std::int32_t childId = roomNode->ChildIndex;
                        childId != -1;
                        childId = RequireReference(ManagedAt(nodes, childId)).NextIndex)
                    {
                        std::shared_ptr<Node> nodeValue = ManagedAt(nodes, childId);
                        Node& node = RequireReference(nodeValue);
                        if (StartsWith(node.Name, "cent"))
                        {
                            centerNode = std::move(nodeValue);
                            break;
                        }
                    }
                    if (!centerNode)
                    {
                        centerNode = roomNode;
                    }
                    UpdateMapModelTransforms(instance, area);
                    SetNavMapDrawNode(roomNode, centerNode);
                    _navMapModelEnabled = true;
                    _navInitRoomNodePos = _navCurRoomNodePos;
                }
            }
            if (_navMapDrawNode)
            {
                _navTargetPos = _navCurRoomNodePos;
            }
        }
    }

    void PlayerEntity::SetNavMapDrawNode(std::shared_ptr<Node> roomNode, std::shared_ptr<Node> centerNode)
    {
        _navMapDrawNode = roomNode;
        _navCurRoomNodePos = RequireReference(roomNode).Animation.Row3().Xyz();
        _navCurCenterNodePos = RequireReference(centerNode).Animation.Row3().Xyz();
    }

    void PlayerEntity::EndMenuPauseHud()
    {
        InitHudState();
        Scene& scene = RequireReference(_scene);
        RequireReference(scene.Layer1Info()).BindingId = _pausedPrevBindingId1;
        RequireReference(scene.Layer1Info()).Alpha = _pausedPrevAlpha1;
        RequireReference(scene.Layer1Info()).MaskId = _pausedPrevMaskId;
        RequireReference(scene.Layer2Info()).BindingId = _pausedPrevBindingId2;
        RequireReference(scene.Layer2Info()).Alpha = _pausedPrevAlpha2;
        RequireReference(scene.Layer3Info()).BindingId = _pausedPrevBindingId3;
        RequireReference(scene.Layer4Info()).BindingId = _pausedPrevBindingId4;
        RequireReference(scene.Layer5Info()).BindingId = _pausedPrevBindingId5;
        if (GameState::DialogPause())
        {
            UpdateDialogs();
        }
        _navMapDrawNode.reset();
        _navMapModelEnabled = false;
    }

    void PlayerEntity::DrawPauseMenuBackground()
    {
        if (!_navMapModelEnabled || _drawPauseState != 1)
        {
            return;
        }
        if (!RequireReference(_scene).NavMapRoomSymbols() || Controls().HudOverlay().IsDown())
        {
            return;
        }
        Scene& scene = RequireReference(_scene);
        const auto navMapRoomSymbols = *scene.NavMapRoomSymbols();
        const auto matrices = GetPauseMapMatrices();
        const Matrix4 viewMtx = matrices.first;
        const Matrix4 orthoMtx = matrices.second;
        std::int32_t area = scene.AreaId() & ~1;
        for (std::int32_t i = 0; i < 2; ++i, ++area)
        {
            if (area < 0 || area >= static_cast<std::int32_t>(_navMapModels.size()))
            {
                continue;
            }
            std::shared_ptr<ModelInstance> modelValue = ManagedAt(_navMapModels, area);
            if (!modelValue)
            {
                continue;
            }
            Model& model = RequireModel(*modelValue);
            const auto& nodes = RequireReference(model.Nodes);
            for (std::int32_t j = 0; j < static_cast<std::int32_t>(nodes.size()); ++j)
            {
                Node& roomNode = RequireReference(ManagedAt(nodes, j));
                if (roomNode.ParentIndex != 0 || !CheckRoomVisited(roomNode))
                {
                    continue;
                }
                for (std::int32_t k = 0; k < navMapRoomSymbols.Length(); ++k)
                {
                    const auto roomSymbolsValue = navMapRoomSymbols[k];
                    const NavMapRoomSymbols& roomSymbols = RequireReference(roomSymbolsValue);
                    if (!RequireStorySave().CheckVisitedRoom(roomSymbols.Id)
                        || !StringEqualsOrdinalIgnoreCase(roomSymbols.Name, roomNode.Name))
                    {
                        continue;
                    }
                    for (std::int32_t l = 0; l < roomSymbols.Symbols.Length(); ++l)
                    {
                        const auto entitySymbolValue = roomSymbols.Symbols[l];
                        const NavMapEntitySymbol& entitySymbol = RequireReference(entitySymbolValue);
                        if (entitySymbol.Type != EntityType::Teleporter)
                        {
                            continue;
                        }
                        const Vector3 teleporterPos
                            = entitySymbol.Position + AddY(roomNode.Animation.Row3().Xyz(), 1.0F);
                        Vector2 distPos{};
                        if (ProjectPosition(teleporterPos, viewMtx, orthoMtx, distPos) > 0.0F)
                        {
                            const Vector2 screenPos((distPos.X + 1.0F) / 2.0F, (1.0F - distPos.Y) / 2.0F);
                            if (screenPos.X > 0.0F && screenPos.X < 1.0F
                                && screenPos.Y > 0.0F && screenPos.Y < 1.0F)
                            {
                                Hud::HudObjectInstance& legend = RequireReference(_mapLegendOtherInst);
                                legend.PositionX = distPos.X - 1.0F / (256.0F / 8.0F);
                                legend.PositionY = distPos.Y - 1.0F / (192.0F / 8.0F);
                                legend.SetIndex(entitySymbol.SubType, scene);
                                scene.DrawHudObject(_mapLegendOtherInst);
                            }
                        }
                    }
                }
            }
        }
    }

    void PlayerEntity::DrawPauseMenuForeground()
    {
        if (_navLoading)
        {
            const auto entry = Text::Strings::GetEntry('R', 997, Text::StringTables::LocationNames);
            if (entry)
            {
                _textSpacingY = 9.0F;
                std::array<char16_t, 128> buffer{};
                WrapText(entry->Value1, 150, buffer);
                const std::int32_t characters = static_cast<std::int32_t>(_navTextTimer / (1.0F / 30.0F));
                DrawText2D(128.0F, 96.0F, Align::PadCenter, 0, buffer, characters);
                if (characters > 0 && characters != _prevScrollingChars
                    && characters <= ManagedStringLength(entry->Value1))
                {
                    _soundSource.StopFreeSfx(SfxId::LETTER_BLIP);
                    _soundSource.PlayFreeSfx(SfxId::LETTER_BLIP);
                    _prevScrollingChars = characters;
                }
                _textSpacingY = 0.0F;
            }
        }
        else if (_drawPauseState == 1)
        {
            Scene& scene = RequireReference(_scene);
            if (scene.ProcessFrame())
            {
                for (std::int32_t i = 0; i < 8; ++i)
                {
                    RequireReference(ManagedAt(_mapOctolithInsts, i)).ProcessAnimation(scene);
                }
                RequireReference(_mapLostOctolithInst).ProcessAnimation(scene);
            }
            std::int32_t artifactIndex = 0;
            for (std::int32_t i = 0; i < 8; ++i)
            {
                const auto [posX, posY] = ManagedAt(_mapIconPositions, i);
                if ((RequireStorySave().Areas & (1U << (i / 2 * 2))) != 0)
                {
                    const bool hasOctolith = (RequireStorySave().CurrentOctoliths & (1U << i)) != 0;
                    const std::uint32_t lostHunter
                        = (RequireStorySave().LostOctoliths >> (i * 4)) & 15U;
                    if (hasOctolith || lostHunter < 8U)
                    {
                        std::shared_ptr<Hud::HudObjectInstance> octoInst = ManagedAt(_mapOctolithInsts, i);
                        std::int32_t offsetX = 0;
                        if (!hasOctolith)
                        {
                            Hud::HudObjectInstance& portrait
                                = RequireReference(ManagedAt(_hunterInsts, static_cast<std::int32_t>(lostHunter)));
                            portrait.PositionX = (static_cast<float>(posX) - 16.0F) / 256.0F;
                            portrait.PositionY = (static_cast<float>(posY) - 16.0F) / 192.0F;
                            scene.DrawHudObject(ManagedAt(_hunterInsts, static_cast<std::int32_t>(lostHunter)));
                            octoInst = _mapLostOctolithInst;
                            offsetX = 6;
                        }
                        Hud::HudObjectInstance& octolith = RequireReference(octoInst);
                        octolith.PositionX
                            = (static_cast<float>(posX + offsetX) - octolith.Width / 2.0F) / 256.0F;
                        octolith.PositionY
                            = (static_cast<float>(posY) - octolith.Height / 2.0F) / 192.0F;
                        scene.DrawHudObject(octoInst);
                    }
                    else
                    {
                        Hud::HudObjectInstance& teleporter = RequireReference(_mapTeleporterInst);
                        teleporter.PositionX
                            = (static_cast<float>(posX) - teleporter.Width / 2.0F) / 256.0F;
                        teleporter.PositionY
                            = (static_cast<float>(posY) - teleporter.Height / 2.0F) / 192.0F;
                        const std::int32_t teleporterIndex
                            = ((RequireStorySave().Artifacts & (7U << artifactIndex)) >> artifactIndex) == 7U ? 1 : 0;
                        teleporter.SetIndex(teleporterIndex, scene);
                        scene.DrawHudObject(_mapTeleporterInst);
                        std::shared_ptr<Hud::HudObjectInstance> dotInst = ManagedAt(_mapArtifactDotInsts, i);
                        for (std::int32_t j = 0; j < 3; ++j)
                        {
                            if ((RequireStorySave().Artifacts & (1U << (artifactIndex + j))) != 0)
                            {
                                const auto [offsetX, offsetY] = ManagedAt(_mapDotOffsets, j);
                                Hud::HudObjectInstance& dot = RequireReference(dotInst);
                                dot.PositionX = (static_cast<float>(posX + offsetX) - dot.Width / 2.0F) / 256.0F;
                                dot.PositionY = (static_cast<float>(posY + offsetY) - dot.Height / 2.0F) / 192.0F;
                                scene.DrawHudObject(dotInst);
                            }
                        }
                    }
                }
                artifactIndex += 3;
            }
            if (!_navMapModelEnabled)
            {
                const auto entry = Text::Strings::GetEntry('R', 998, Text::StringTables::LocationNames);
                if (entry)
                {
                    _textSpacingY = 9.0F;
                    std::array<char16_t, 256> buffer{};
                    WrapText(entry->Value1, 150, buffer);
                    const std::int32_t characters = static_cast<std::int32_t>(_navTextTimer / (1.0F / 30.0F));
                    DrawText2D(128.0F, 96.0F, Align::PadCenter, 0, buffer, characters);
                    if (characters > 0 && characters != _prevScrollingChars
                        && characters <= ManagedStringLength(entry->Value1))
                    {
                        _soundSource.StopFreeSfx(SfxId::LETTER_BLIP);
                        _soundSource.PlayFreeSfx(SfxId::LETTER_BLIP);
                        _prevScrollingChars = characters;
                    }
                    _textSpacingY = 0.0F;
                }
            }
            else
            {
                std::string roomName{};
                const auto unknownEntry = Text::Strings::GetEntry('R', 999, Text::StringTables::LocationNames);
                if (unknownEntry)
                {
                    roomName = unknownEntry->Value1;
                }
                if (_navMapDrawNode)
                {
                    for (std::int32_t i = 1; i <= 35; ++i)
                    {
                        const std::int32_t id = scene.AreaId() / 2 * 100 + i;
                        const auto roomEntry = Text::Strings::GetEntry('R', id, Text::StringTables::LocationNames);
                        if (!roomEntry)
                        {
                            break;
                        }
                        if (StringEqualsOrdinalIgnoreCase(roomEntry->Value1, _navMapDrawNode->Name))
                        {
                            roomName = roomEntry->Value2;
                            break;
                        }
                    }
                }
                const std::int32_t roomNameLength = ManagedStringLength(roomName);
                if (roomNameLength > 0)
                {
                    assert(roomNameLength <= 200);
                    std::vector<char16_t> buffer(static_cast<std::size_t>(roomNameLength + 10));
                    _textSpacingY = 9.0F;
                    const std::int32_t lines = WrapText(roomName, 175, buffer);
                    const std::int32_t characters = static_cast<std::int32_t>(_navTextTimer / (1.0F / 30.0F));
                    const std::int32_t y = 173 - ((8 * lines) >> 1);
                    DrawText2D(128.0F, static_cast<float>(y), Align::PadCenter, 0,
                        std::span<const char16_t>(buffer.data(), buffer.size()), characters);
                    if ((roomNameLength > 1 || (roomNameLength == 1 && roomName != " "))
                        && characters > 0 && characters != _prevScrollingChars
                        && characters <= roomNameLength)
                    {
                        _soundSource.StopFreeSfx(SfxId::LETTER_BLIP);
                        _soundSource.PlayFreeSfx(SfxId::LETTER_BLIP);
                        _prevScrollingChars = characters;
                    }
                    _textSpacingY = 0.0F;
                }
                if (Controls().HudOverlay().IsDown())
                {
                    ManagedAt(_mapLegendInfo, 0).Unlocked = _availableWeapons[BeamType::Battlehammer];
                    ManagedAt(_mapLegendInfo, 1).Unlocked = _availableWeapons[BeamType::VoltDriver];
                    ManagedAt(_mapLegendInfo, 2).Unlocked = _availableWeapons[BeamType::ShockCoil];
                    ManagedAt(_mapLegendInfo, 3).Unlocked = _availableWeapons[BeamType::Imperialist];
                    ManagedAt(_mapLegendInfo, 4).Unlocked = _availableWeapons[BeamType::Judicator];
                    ManagedAt(_mapLegendInfo, 5).Unlocked = _availableWeapons[BeamType::Magmaul];
                    std::int32_t column = 0;
                    float posY = 65.0F;
                    std::array<char16_t, 128> buffer{};
                    for (std::int32_t i = 0; i < static_cast<std::int32_t>(_mapLegendInfo.size()); ++i)
                    {
                        MapLegendInfo& info = ManagedAt(_mapLegendInfo, i);
                        std::string text;
                        if (info.Group == 0)
                        {
                            if (info.Unlocked)
                            {
                                text = Text::Strings::GetMessage('B', info.MessageId, Text::StringTables::HudMessagesSP);
                            }
                            else
                            {
                                text = Scene::Language() == Language::Spanish ? "(?)" : "???";
                            }
                        }
                        else
                        {
                            text = Text::Strings::GetMessage('M', info.MessageId, Text::StringTables::HudMessagesSP);
                        }
                        const float textX = column == 0 ? 114.0F : 143.0F;
                        const float objX = column == 0 ? 116.0F : 132.0F;
                        Hud::HudObjectInstance& hudObject = RequireReference(info.HudObject);
                        hudObject.PositionX = (objX + static_cast<float>(info.OffsetX)) / 256.0F;
                        hudObject.PositionY = (posY + static_cast<float>(info.OffsetY)) / 192.0F;
                        hudObject.SetIndex(info.ObjectIndex, scene);
                        scene.DrawHudObject(info.HudObject);
                        _textSpacingY = 8.0F;
                        buffer.fill(u'\0');
                        const std::int32_t lines = WrapText(text, 80, buffer);
                        DrawText2D(textX, posY + 1.0F,
                            column == 0 ? Align::Right : Align::Left, 0, buffer);
                        _textSpacingY = 0.0F;
                        posY += static_cast<float>(8 * lines + 3);
                        if (posY >= 129.0F)
                        {
                            posY = 65.0F;
                            ++column;
                        }
                    }
                }
            }
        }
        DrawPauseQuitInterface();
    }

    void PlayerEntity::DrawPauseQuitInterface()
    {
        if (_drawPauseState == 1)
        {
            const std::int32_t posX = 26;
            Hud::HudObjectInstance& quit = RequireReference(_mapQuitInst);
            quit.PositionX = (static_cast<float>(posX) - quit.Width / 2.0F) / 256.0F;
            quit.PositionY = (173.0F - quit.Height / 2.0F) / 192.0F;
            RequireReference(_scene).DrawHudObject(_mapQuitInst);
            const std::string text = Text::Strings::GetHudMessage(119);
            DrawText2D(static_cast<float>(posX), 181.0F, Align::Center, 0, text);
        }
        else if (_drawPauseState == 2)
        {
            Scene& scene = RequireReference(_scene);
            if (scene.ProcessFrame())
            {
                RequireReference(_dialogButtonInst).ProcessAnimation(scene);
            }
            const std::string text = Text::Strings::GetHudMessage(122);
            const std::int32_t characters = static_cast<std::int32_t>(_navTextTimer / (1.0F / 30.0F));
            DrawText2D(128.0F, 90.0F, Align::PadCenter, 0, text,
                std::nullopt, 1.0F, -1.0F, characters, 1.0F);
            if (characters > 0 && characters != _prevScrollingChars
                && characters <= ManagedStringLength(text))
            {
                _soundSource.StopFreeSfx(SfxId::LETTER_BLIP);
                _soundSource.PlayFreeSfx(SfxId::LETTER_BLIP);
                _prevScrollingChars = characters;
            }
            DrawDialogConfirmButtons(DialogType::YesNo);
        }
    }

    void PlayerEntity::ProcessPauseMenu()
    {
        if (_navLoading)
        {
            if (_navTextTimer < 60.0F / 30.0F)
            {
                _navTextTimer += RequireReference(_scene).FrameTime();
            }
            if (_navTextTimer >= 60.0F / 30.0F && !GameState::InRoomTransition())
            {
                SetUpMenuPauseMapNav();
                _navTextTimer = 0.0F;
                _prevScrollingChars = 0;
                _navLoading = false;
            }
        }
        else if (_navTextTimer < 200.0F / 30.0F)
        {
            _navTextTimer += RequireReference(_scene).FrameTime();
        }
        if (_pauseFrameCount > 0 && _pauseFrameCount % 2 == 0)
        {
            RequireReference(_navPlayerPosModel).UpdateAnimFrames();
        }
        _pauseFrameCount = UncheckedIncrement(_pauseFrameCount);
        if (RequireReference(_scene).CameraMode() == CameraMode::Player)
        {
            ProcessPauseMenuInput();
        }
    }

    void PlayerEntity::ResetPauseQuitDisplay()
    {
        _navTextTimer = 0.0F;
        _prevScrollingChars = 0;
        RequireReference(_dialogButtonInst).SetAnimation(0, 2, 3, 2);
    }

    void PlayerEntity::ProcessPauseMenuInput()
    {
        if (GameState::PausePrevented())
        {
            return;
        }
        if (_drawPauseState == 1 && CheckButtonPressed(DialogButton::Quit))
        {
            _soundSource.PlayFreeSfx(SfxId::QUIT_GAME);
            _drawPauseState = 2;
            ResetPauseQuitDisplay();
            return;
        }
        if (_drawPauseState == 2)
        {
            if (CheckButtonPressed(DialogButton::Yes))
            {
                GameState::PausePrevented(true);
                _soundSource.PlayFreeSfx(SfxId::QUIT_GAME);
                _soundSource.PlayFreeSfx(SfxId::RETURN_TO_SHIP_YES);
                _drawPauseState = 0;
                ResetPauseQuitDisplay();
                Music::Stop(20.0F / 30.0F);
                RequireReference(_scene).SetFade(
                    FadeType::FadeOutBlack, 20.0F / 30.0F, true, AfterFade::Exit);
                return;
            }
            if (CheckButtonPressed(DialogButton::No))
            {
                _soundSource.PlayFreeSfx(SfxId::RETURN_TO_SHIP_NO);
                _drawPauseState = 1;
                ResetPauseQuitDisplay();
                return;
            }
        }
        if (_drawPauseState != 1)
        {
            return;
        }
        if (_isScrollingUp)
        {
            _navDrawZoom -= 0.0625F;
        }
        else if (_isScrollingDown)
        {
            _navDrawZoom += 0.0625F;
        }
        _navDrawZoom = std::clamp(_navDrawZoom, 0.4375F, 3.0F);
        if (Controls().AimLeft().IsDown())
        {
            _navDrawRotX += 2.8125F / 2.0F;
        }
        else if (Controls().AimRight().IsDown())
        {
            _navDrawRotX -= 2.8125F / 2.0F;
        }
        else if (_input.MouseState && _input.MouseState->IsButtonDown(MouseButton::Left)
            && _input.MouseDeltaX() != 0.0F)
        {
            _navDrawRotX += -_input.MouseDeltaX() / 8.0F * 2.8125F;
        }
        if (_navDrawRotX >= 360.0F)
        {
            _navDrawRotX -= 360.0F;
        }
        else if (_navDrawRotX <= -360.0F)
        {
            _navDrawRotX += 360.0F;
        }
        if (Controls().AimUp().IsDown())
        {
            _navDrawRotY -= 2.8125F / 2.0F;
        }
        else if (Controls().AimDown().IsDown())
        {
            _navDrawRotY += 2.8125F / 2.0F;
        }
        else if (_input.MouseState && _input.MouseState->IsButtonDown(MouseButton::Left)
            && _input.MouseDeltaY() != 0.0F)
        {
            _navDrawRotY += _input.MouseDeltaY() / 8.0F * 2.8125F;
        }
        _navDrawRotY = std::clamp(_navDrawRotY, -71.41113F, 71.41113F);
        const auto matrices = GetPauseMapMatrices();
        const Matrix4 viewMtx = matrices.first;
        const Matrix4 orthoMtx = matrices.second;
        float panDirX = 0.0F;
        if (Controls().MoveLeft().IsDown())
        {
            panDirX = -1.0F;
        }
        else if (Controls().MoveRight().IsDown())
        {
            panDirX = 1.0F;
        }
        if (panDirX != 0.0F)
        {
            _navPanOffset = _navPanOffset + ScaleVector(Column0(viewMtx), 4.6F / 2.0F * panDirX);
        }
        float panDirY = 0.0F;
        if (Controls().MoveUp().IsDown())
        {
            panDirY = 1.0F;
        }
        else if (Controls().MoveDown().IsDown())
        {
            panDirY = -1.0F;
        }
        if (panDirY != 0.0F)
        {
            _navPanOffset = _navPanOffset + ScaleVector(Column1(viewMtx), 4.6F / 2.0F * panDirY);
        }
        if (panDirX != 0.0F || panDirY != 0.0F)
        {
            _navPanTimer = 0.0F;
            float minDist = 512.0F * 512.0F;
            std::shared_ptr<Node> newRoomNode{};
            std::shared_ptr<Node> newCenterNode{};
            std::int32_t area = RequireReference(_scene).AreaId() & ~1;
            for (std::int32_t i = 0; i < 2; ++i, ++area)
            {
                if (area >= 0 && area < static_cast<std::int32_t>(_navMapModels.size()))
                {
                    std::shared_ptr<ModelInstance> modelValue = ManagedAt(_navMapModels, area);
                    if (!modelValue)
                    {
                        continue;
                    }
                    Model& model = RequireModel(*modelValue);
                    const auto& nodes = RequireReference(model.Nodes);
                    for (std::int32_t j = 0; j < static_cast<std::int32_t>(nodes.size()); ++j)
                    {
                        std::shared_ptr<Node> roomNodeValue = ManagedAt(nodes, j);
                        Node& roomNode = RequireReference(roomNodeValue);
                        if (roomNode.ParentIndex != 0 || !CheckRoomVisited(roomNode))
                        {
                            continue;
                        }
                        for (std::int32_t k = roomNode.ChildIndex; k != -1;)
                        {
                            std::shared_ptr<Node> nodeValue = ManagedAt(nodes, k);
                            Node& node = RequireReference(nodeValue);
                            const Vector3 nodePos = node.Animation.Row3().Xyz();
                            Vector2 distPos{};
                            if (StartsWith(node.Name, "cent")
                                && ProjectPosition(nodePos, viewMtx, orthoMtx, distPos) > 0.0F)
                            {
                                const Vector2 screenPos(
                                    (distPos.X + 1.0F) / 2.0F, (1.0F - distPos.Y) / 2.0F);
                                const float distX = distPos.X * 2.0F - 1.0F;
                                const float distY = 1.0F - distPos.Y * 2.0F;
                                const float dist = distX * distX + distY * distY;
                                if (screenPos.X > 0.0F && screenPos.X < 1.0F
                                    && screenPos.Y > 0.0F && screenPos.Y < 1.0F && dist < minDist)
                                {
                                    newRoomNode = roomNodeValue;
                                    newCenterNode = nodeValue;
                                    minDist = dist;
                                }
                            }
                            k = node.NextIndex;
                        }
                    }
                }
            }
            if (newRoomNode && newRoomNode != _navMapDrawNode)
            {
                assert(newCenterNode != nullptr);
                SetNavMapDrawNode(newRoomNode, newCenterNode);
                _navPanOffset = _navTargetPos - _navCurCenterNodePos;
                _navTextTimer = 0.0F;
                _prevScrollingChars = 0;
            }
        }
        else if (!IsZero(_navPanOffset) && !Features::NoMapCentering())
        {
            if (_navPanTimer < 16.0F * 30.0F)
            {
                _navPanTimer += RequireReference(_scene).FrameTime();
                if (_navPanTimer > 16.0F * 30.0F)
                {
                    _navPanTimer = 16.0F * 30.0F;
                }
            }
            const float frames = _navPanTimer * 30.0F;
            const float factor = 1.0F - 0.1F * (frames / 16.0F);
            float x = ExponentialDecay(factor, _navPanOffset.X);
            float y = ExponentialDecay(factor, _navPanOffset.Y);
            float z = ExponentialDecay(factor, _navPanOffset.Z);
            if (std::fabs(x) < 1.0F / 4096.0F)
            {
                x = 0.0F;
            }
            if (std::fabs(y) < 1.0F / 4096.0F)
            {
                y = 0.0F;
            }
            if (std::fabs(z) < 1.0F / 4096.0F)
            {
                z = 0.0F;
            }
            _navPanOffset = Vector3(x, y, z);
        }
    }

    std::pair<Vector3, Vector3> PlayerEntity::GetPauseMapLookVectors()
    {
        _navTargetPos = _navCurCenterNodePos + _navPanOffset;
        const Vector3 cameraTarget = AddY(_navTargetPos, 3.75F);
        const float radiansX = DegreesToRadians(_navDrawRotX);
        const float radiansY = DegreesToRadians(_navDrawRotY);
        const float sinX = std::sin(radiansX);
        const float cosX = std::cos(radiansX);
        const float sinY = std::sin(radiansY);
        const float cosY = std::cos(radiansY);
        const Vector3 cameraPos = cameraTarget
            + Vector3(sinX * cosY * 90.0F, sinY * 90.0F, cosX * cosY * 90.0F);
        return {cameraPos, cameraTarget};
    }

    std::pair<Matrix4, Matrix4> PlayerEntity::GetPauseMapMatrices()
    {
        const Matrix4 orthoMtx = CreateOrthographic(
            256.0F * _navDrawZoom, 192.0F / 256.0F * 256.0F * _navDrawZoom,
            -400.0F, 400.0F);
        const auto look = GetPauseMapLookVectors();
        const Matrix4 viewMtx = LookAt(look.first, look.second, Vector3(0.0F, 1.0F, 0.0F));
        return {viewMtx, orthoMtx};
    }

    void PlayerEntity::UpdateMapModelTransforms(ModelInstance& inst, std::int32_t area)
    {
        Model& model = RequireModel(inst);
        Matrix4 transform = CreateScale(model.Scale.X);
        const Vector3 offset = ManagedAt(_navMapNodeOffsets, area);
        transform.M41 += offset.X;
        transform.M42 += offset.Y;
        transform.M43 += offset.Z;
        UpdateTransforms(inst, transform, 0);
    }

    bool PlayerEntity::CheckRoomVisited(const Node& node) const
    {
        if (StartsWith(node.Name, "Con") && node.Name.size() >= 5)
        {
            std::int32_t id = 0;
            if (TryParseConnectorId(std::string_view(node.Name).substr(3, 2), id) && id >= 1)
            {
                return RequireStorySave().CheckVisitedConnector(id - 1, RequireReference(_scene).AreaId());
            }
        }
        for (std::int32_t i = 27; i <= 92; ++i)
        {
            const RoomMetadata& meta = RequireReference(Metadata::GetRoomById(i));
            if (StringEqualsOrdinalIgnoreCase(node.Name, meta.Name))
            {
                return RequireStorySave().CheckVisitedRoom(i);
            }
        }
        return false;
    }

    void PlayerEntity::GetPauseMapRenderItems()
    {
        if (!_navMapModelEnabled || _drawPauseState != 1 || Controls().HudOverlay().IsDown())
        {
            return;
        }
        Scene& scene = RequireReference(_scene);
        std::int32_t area = scene.AreaId() & ~1;
        for (std::int32_t i = 0; i < 2; ++i, ++area)
        {
            if (area >= static_cast<std::int32_t>(_navMapModels.size()))
            {
                break;
            }
            std::shared_ptr<ModelInstance> instValue = ManagedAt(_navMapModels, area);
            if (!instValue)
            {
                continue;
            }
            ModelInstance& inst = *instValue;
            UpdateMapModelTransforms(inst, area);
            GetMapDrawItems(inst);
        }
        Matrix4 playerPosTransform = Multiply(CreateScale(2.0F), GetTransformMatrix(_facingVector, _upVector));
        Vector3 roomOffset = Vector3::Zero;
        assert(scene.Room() != nullptr);
        RoomEntity& room = RequireReference(scene.Room());
        const auto& collisions = room.RoomCollision();
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(collisions.size()); ++i)
        {
            const CollisionInstance& collision = RequireReference(ManagedAt(collisions, i));
            if (collision.ConnectorName.has_value() && NodeRef.RoomName.HasValue()
                && *collision.ConnectorName == *NodeRef.RoomName)
            {
                roomOffset = collision.Translation;
                break;
            }
        }
        float posHeightFactor = 1.0F;
        if (NodeRef.RoomName.HasValue() && *NodeRef.RoomName == "UNIT2_C2")
        {
            posHeightFactor = 2.05F;
        }
        else if (NodeRef.RoomName.HasValue() && *NodeRef.RoomName == "UNIT2_C3")
        {
            posHeightFactor = 1.305F;
        }
        const float x = _position.X + _navInitRoomNodePos.X - roomOffset.X;
        const float y = _position.Y * posHeightFactor + _navInitRoomNodePos.Y - roomOffset.Y + 1.0F;
        const float z = _position.Z + _navInitRoomNodePos.Z - roomOffset.Z;
        SetTranslation(playerPosTransform, Vector3(x, y, z));
        ModelInstance& playerPosModel = RequireReference(_navPlayerPosModel);
        UpdateTransforms(playerPosModel, playerPosTransform, 0);
        EntityBase::GetDrawItems(playerPosModel, 0);
        const auto navMapValue = scene.NavMapRoomSymbols();
        if (!_navMapDrawNode || StartsWith(_navMapDrawNode->Name, "Con") || !navMapValue)
        {
            return;
        }
        const auto look = GetPauseMapLookVectors();
        const Vector3 lightVec = (look.second - look.first).Normalized();
        const LightInfo lightInfo(
            lightVec, Vector3(1.0F, 1.0F, 1.0F), Vector3::Zero, Vector3::Zero);
        const auto navMapRoomSymbols = *navMapValue;
        for (std::int32_t i = 0; i < navMapRoomSymbols.Length(); ++i)
        {
            const NavMapRoomSymbols& roomSymbols = RequireReference(navMapRoomSymbols[i]);
            if (!StringEqualsOrdinalIgnoreCase(roomSymbols.Name, _navMapDrawNode->Name))
            {
                continue;
            }
            for (std::int32_t j = 0; j < roomSymbols.Symbols.Length(); ++j)
            {
                const NavMapEntitySymbol& entitySymbol = RequireReference(roomSymbols.Symbols[j]);
                if (entitySymbol.Type == EntityType::Door)
                {
                    bool locked = entitySymbol.Locked;
                    if (entitySymbol.Id != -1)
                    {
                        locked = RequireStorySave().GetRoomState(roomSymbols.Id, entitySymbol.Id) != 0;
                    }
                    std::int32_t palette = 0;
                    if (locked)
                    {
                        palette = entitySymbol.SubType;
                        if (palette > 7)
                        {
                            palette = 9;
                        }
                    }
                    const Vector3 doorPos = entitySymbol.Position + _navCurRoomNodePos;
                    const Matrix4 doorTransform
                        = GetTransformMatrix(entitySymbol.FacingVector, entitySymbol.UpVector, doorPos);
                    ModelInstance& doorModel = RequireReference(_navDoorModel);
                    UpdateTransforms(doorModel, doorTransform, 0);
                    Model& model = RequireModel(doorModel);
                    Material& material = RequireReference(ManagedAt(RequireReference(model.Materials), 0));
                    material.CurrentDiffuse = ManagedAt(_navDoorColors, palette);
                    EntityBase::GetDrawItems(doorModel, 0, lightInfo);
                }
            }
        }
    }

    void PlayerEntity::GetMapDrawItems(ModelInstance& inst)
    {
        Scene& scene = RequireReference(_scene);
        const std::int32_t polygonId = scene.GetNextPolygonId();
        Model& model = RequireModel(inst);
        const auto& nodes = RequireReference(model.Nodes);
        Node& root = RequireReference(ManagedAt(nodes, 0));

        std::function<void(Node&)> getItems;
        getItems = [&](Node& node)
        {
            if (node.Enabled)
            {
                std::shared_ptr<Node> roomParent{};
                std::int32_t parentIndex = node.ParentIndex;
                while (parentIndex > 0)
                {
                    roomParent = ManagedAt(nodes, parentIndex);
                    Node& parent = RequireReference(roomParent);
                    parentIndex = parent.ParentIndex;
                }
                if (roomParent && !CheckRoomVisited(RequireReference(roomParent)))
                {
                    return;
                }
                const bool isSelected = _navMapDrawNode && roomParent == _navMapDrawNode;
                const std::int32_t start = node.MeshId / 2;
                const auto& meshes = RequireReference(model.Meshes);
                const auto& materials = RequireReference(model.Materials);
                for (std::int32_t k = 0; k < node.MeshCount; ++k)
                {
                    Mesh& mesh = RequireReference(ManagedAt(meshes, start + k));
                    if (!mesh.Visible)
                    {
                        continue;
                    }
                    Material& material = RequireReference(ManagedAt(materials, mesh.MaterialId));
                    material.Wireframe = 0;
                    material.Lighting = 1;
                    const float alpha = isSelected ? 20.0F / 31.0F : 4.0F / 31.0F;
                    Vector3 emission = Vector3::Zero;
                    const Vector3 lightColor = isSelected
                        ? Vector3(247.0F / 255.0F, 153.0F / 255.0F, 52.0F / 255.0F)
                        : Vector3(247.0F / 255.0F, 123.0F / 255.0F, 22.0F / 255.0F);
                    const LightInfo lightInfo(
                        Vector3(0.0F, 0.0F, -0.0F), lightColor, Vector3::Zero, lightColor);
                    const std::int32_t matrixCount
                        = static_cast<std::int32_t>(RequireReference(model.NodeMatrixIds).size());
                    const std::vector<float> matrixStack
                        = CopyManagedArray(RequireReference(model.MatrixStackValues));
                    scene.AddRenderItem(material, polygonId, alpha, emission, lightInfo,
                        IdentityMatrix(), node.Animation, mesh.ListId, matrixCount,
                        matrixStack, std::nullopt, std::nullopt,
                        SelectionType::None, node.BillboardMode);
                    if (!isSelected)
                    {
                        scene.AddRenderItem(material, polygonId, alpha, emission, lightInfo,
                            IdentityMatrix(), node.Animation, mesh.ListId, matrixCount,
                            matrixStack, std::nullopt, std::nullopt,
                            SelectionType::None, node.BillboardMode);
                    }
                    else if (!material.Name.empty() && material.Name[0] != 'T')
                    {
                        material.Wireframe = 1;
                        material.Lighting = 0;
                        emission = Vector3(247.0F / 255.0F, 123.0F / 255.0F, 22.0F / 255.0F);
                        const Vector3 prevDiffuse = material.CurrentDiffuse;
                        material.CurrentDiffuse = emission;
                        scene.AddRenderItem(material, polygonId, 1.0F, emission, lightInfo,
                            IdentityMatrix(), node.Animation, mesh.ListId, matrixCount,
                            matrixStack, std::nullopt, std::nullopt,
                            SelectionType::None, node.BillboardMode);
                        material.CurrentDiffuse = prevDiffuse;
                    }
                }
                if (node.ChildIndex != -1)
                {
                    getItems(RequireReference(ManagedAt(nodes, node.ChildIndex)));
                }
            }
            if (node.NextIndex != -1)
            {
                getItems(RequireReference(ManagedAt(nodes, node.NextIndex)));
            }
        };

        getItems(root);
    }
}

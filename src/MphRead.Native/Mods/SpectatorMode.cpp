#include "SpectatorMode.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace OpenTK::Graphics::OpenGL
{
    enum class EnableCap : std::int32_t
    {
        Blend = 0x0BE2,
        CullFace = 0x0B44,
        DepthTest = 0x0B71
    };

    enum class BlendingFactor : std::int32_t
    {
        SrcAlpha = 0x0302,
        OneMinusSrcAlpha = 0x0303
    };

    enum class MatrixMode : std::int32_t
    {
        Modelview = 0x1700,
        Projection = 0x1701
    };

    enum class TextureTarget : std::int32_t
    {
        Texture2D = 0x0DE1
    };

    enum class MaterialFace : std::int32_t
    {
        FrontAndBack = 0x0408
    };

    enum class PolygonMode : std::int32_t
    {
        Fill = 0x1B02
    };

    enum class PrimitiveType : std::int32_t
    {
        Quads = 0x0007
    };

    class Gl final
    {
    public:
        Gl() = delete;

        static void Enable(EnableCap cap);
        static void Disable(EnableCap cap);
        static void BlendFunc(BlendingFactor source, BlendingFactor destination);
        static void MatrixMode(OpenTK::Graphics::OpenGL::MatrixMode mode);
        static void LoadIdentity();
        static void PushMatrix();
        static void PopMatrix();
        static void LoadMatrix(const OpenTK::Mathematics::Matrix4& matrix);
        static void BindTexture(TextureTarget target, std::int32_t texture);
        static void PolygonMode(MaterialFace face, OpenTK::Graphics::OpenGL::PolygonMode mode);
        static void Begin(PrimitiveType primitive);
        static void End();
        static void Color3(float red, float green, float blue);
        static void Color4(float red, float green, float blue, float alpha);
        static void TexCoord2(float s, float t);
        static void Vertex3(float x, float y, float z);
        static void Ortho(double left, double right, double bottom, double top,
            double zNear, double zFar);
    };
}

namespace MphRead::Formats::Culling
{
    class NullableString
    {
    public:
        NullableString() noexcept = default;

    private:
        std::shared_ptr<const std::string> _value{};
    };

    struct NodeRef
    {
        NullableString RoomName{};
        std::int32_t PartIndex = 0;
        std::int32_t NodeIndex = 0;
        std::int32_t ModelIndex = 0;

        static const NodeRef None;
    };
}

namespace MphRead
{
    enum class MatchState : std::int32_t;

    namespace MatchStateValues
    {
        extern const MatchState Playing;
    }

    class CollisionPlane
    {
    public:
        CollisionPlane(OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            OpenTK::Mathematics::Vector3 point3);

    private:
        std::array<OpenTK::Mathematics::Vector3, 4> _interopStorage{};
    };

    class GameState final
    {
    public:
        GameState() = delete;

        [[nodiscard]] static std::int32_t MainPlayerIndex();
        [[nodiscard]] static MphRead::MatchState MatchState();
        [[nodiscard]] static bool Teams();
    };

    class Textures final
    {
    public:
        Textures() = delete;
        [[nodiscard]] static std::int32_t IceLayerTextureId();
    };

    class UiHud final
    {
    public:
        UiHud() = delete;
        [[nodiscard]] static bool DebugCamera();
    };

    class Menu final
    {
    public:
        Menu() = delete;
        static void DrawLogo(std::int32_t x, std::int32_t y, float scale);
    };

    class EntityBase;

    class Scene final
    {
    public:
        Scene() = delete;

        static void RenderRoom(std::int32_t roomId,
            const std::shared_ptr<Entities::CameraInfo>& cameraInfo,
            bool showInvisible,
            OpenTK::Graphics::OpenGL::PolygonMode polygonMode,
            const std::vector<std::shared_ptr<EntityBase>>& entities,
            MphRead::Formats::Culling::NodeRef nodeRef,
            const std::shared_ptr<const std::vector<CollisionPlane>>& roomCollision,
            bool isCurrentRoom);
    };
}

namespace MphRead::Entities
{
    enum class LoadFlags : std::uint8_t
    {
        None = 0,
        Connected = 0x1,
        WasConnected = 0x2,
        Disconnected = 0x4,
        Initial = 0x8,
        Unknown4 = 0x10,
        Active = 0x20,
        SlotActive = 0x40,
        Spawned = 0x80
    };

    enum class Effectiveness : std::uint8_t;
    enum class PlayerAltForm : std::int32_t;
    enum class CameraMode : std::int32_t;

    namespace EffectivenessValues
    {
        extern const Effectiveness Invisible;
    }

    namespace PlayerAltFormValues
    {
        extern const PlayerAltForm Sylux;
    }

    namespace CameraModeValues
    {
        extern const CameraMode Free;
        extern const CameraMode Intro;
    }

    class CameraInfo
    {
    public:
        OpenTK::Mathematics::Vector3 Position{};
        OpenTK::Mathematics::Vector3 PrevPosition{};
        OpenTK::Mathematics::Vector3 Target{};
        OpenTK::Mathematics::Vector3 UpVector{};
        OpenTK::Mathematics::Vector3 TrueUp{};
        OpenTK::Mathematics::Vector3 Facing{};
        float Fov = 0.0F;
        float Shake = 0.0F;
        OpenTK::Mathematics::Matrix4 ViewMatrix{};
        float Field48 = 0.0F;
        float Field4C = 0.0F;
        float Field50 = 0.0F;
        float Field54 = 0.0F;
        MphRead::Formats::Culling::NodeRef NodeRef
            = MphRead::Formats::Culling::NodeRef::None;

        CameraInfo(std::shared_ptr<CameraInfo> source);
        CameraInfo(OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 target,
            OpenTK::Mathematics::Vector3 up,
            float fov);

        [[nodiscard]] bool IsThirdPerson() const;
    };

    class PlayerEntity
    {
    public:
        static constexpr std::size_t SlotCapacity = 4;

        [[nodiscard]] static std::shared_ptr<PlayerEntity> Main();
        static void SetMain(std::shared_ptr<PlayerEntity> value);
        [[nodiscard]] static const std::array<std::shared_ptr<PlayerEntity>, SlotCapacity>& Players();

        [[nodiscard]] std::int32_t Health() const;
        [[nodiscard]] Entities::LoadFlags LoadFlags() const;
        [[nodiscard]] bool IsBot() const;
        [[nodiscard]] bool IsAltForm() const;
        [[nodiscard]] Entities::PlayerAltForm AltForm() const;
        [[nodiscard]] std::int32_t SlotIndex() const;
        [[nodiscard]] std::int32_t TeamIndex() const;
        [[nodiscard]] OpenTK::Mathematics::Vector3 Emission() const;
        [[nodiscard]] Entities::Effectiveness Effectiveness() const;
        [[nodiscard]] bool Zoomed() const;
        [[nodiscard]] std::uint16_t FrozenGfxTimer() const;
        [[nodiscard]] std::int32_t Cloaking() const;
        [[nodiscard]] std::shared_ptr<Entities::CameraInfo> CameraInfo() const;

        void SetIsMainPlayer(bool value);
        void DrawModel();
        void DrawAltForm();
        void DrawMorphBallTrail();
        void DrawOverheadIcon();
        void DrawShield();
        void DrawAltAttack();
    };

    class PlayerCamera final
    {
    public:
        PlayerCamera() = delete;

        [[nodiscard]] static Entities::CameraMode Mode();
        [[nodiscard]] static std::shared_ptr<Entities::CameraInfo> CameraInfo();
        static void UpdateCameraPosition(const std::shared_ptr<PlayerEntity>& player);
        static void UpdateAimVecs(const std::shared_ptr<PlayerEntity>& player);
        static void UpdateFreeCamera(const std::shared_ptr<PlayerEntity>& player);
        static void UpdateFirstPersonCamera(const std::shared_ptr<PlayerEntity>& player);
        static void UpdateCameraSequence(const std::shared_ptr<PlayerEntity>& player);
    };

    class PlayerDraw final
    {
    public:
        PlayerDraw() = delete;

        static void DrawGunSmoke(const std::shared_ptr<PlayerEntity>& player);
        static void DrawMuzzleFx(const std::shared_ptr<PlayerEntity>& player);
        static void DrawZoom(const std::shared_ptr<PlayerEntity>& player);
        static void DrawIceLayer(const std::shared_ptr<PlayerEntity>& player);
    };

    class PlayerHud final
    {
    public:
        PlayerHud() = delete;

        static void DrawAltHud(const std::shared_ptr<PlayerEntity>& player);
        static void DrawHud(const std::shared_ptr<PlayerEntity>& player);
        static void DrawZoomHud(const std::shared_ptr<PlayerEntity>& player);
    };
}

namespace
{
    using MphRead::CollisionPlane;
    using MphRead::Entities::CameraInfo;
    using MphRead::Entities::LoadFlags;
    using MphRead::Entities::PlayerEntity;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    [[nodiscard]] constexpr bool VectorEquals(Vector3 left, Vector3 right) noexcept
    {
        return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
    }

    [[nodiscard]] constexpr Matrix4 CreateOrthographicOffCenter(float left, float right,
        float bottom, float top, float zNear, float zFar) noexcept
    {
        return Matrix4(
            Vector4(2.0F / (right - left), 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 2.0F / (top - bottom), 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, -2.0F / (zFar - zNear), 0.0F),
            Vector4(
                -(right + left) / (right - left),
                -(top + bottom) / (top - bottom),
                -(zFar + zNear) / (zFar - zNear),
                1.0F));
    }
}

namespace MphRead
{
    std::int32_t SpectatorMode::_spectatorEnabled = 0;
    std::int32_t SpectatorMode::_spectatorTarget = -1;
    std::shared_ptr<Entities::CameraInfo> SpectatorMode::_prevCameraInfo{};
    std::int32_t SpectatorMode::_prevPlayerTarget = -1;

    const Vector3 SpectatorMode::_fbPosition = Vector3(0.0F, 5.0F, 0.0F);
    const Vector3 SpectatorMode::_fbFacing = Vector3(1.0F, 0.0F, 0.0F);
    const Vector3 SpectatorMode::_fbUp = Vector3(0.0F, 1.0F, 0.0F);
    const std::shared_ptr<const std::vector<CollisionPlane>> SpectatorMode::_fbCollision
        = std::make_shared<const std::vector<CollisionPlane>>(
            std::initializer_list<CollisionPlane>{
                CollisionPlane(Vector3(0.0F, 5.0F, 5.0F), Vector3(1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, -1.0F, 0.0F), Vector3(0.0F, 0.0F, -1.0F)),
                CollisionPlane(Vector3(0.0F, 5.0F, 5.0F), Vector3(-1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 0.0F, -1.0F), Vector3(0.0F, -1.0F, 0.0F)),
                CollisionPlane(Vector3(0.0F, 5.0F, -5.0F), Vector3(1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 0.0F, 1.0F), Vector3(0.0F, -1.0F, 0.0F)),
                CollisionPlane(Vector3(0.0F, 5.0F, -5.0F), Vector3(-1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, -1.0F, 0.0F), Vector3(0.0F, 0.0F, 1.0F)),
                CollisionPlane(Vector3(5.0F, 5.0F, 0.0F), Vector3(0.0F, 0.0F, 1.0F),
                    Vector3(0.0F, -1.0F, 0.0F), Vector3(-1.0F, 0.0F, 0.0F)),
                CollisionPlane(Vector3(5.0F, 5.0F, 0.0F), Vector3(0.0F, 0.0F, -1.0F),
                    Vector3(-1.0F, 0.0F, 0.0F), Vector3(0.0F, -1.0F, 0.0F)),
                CollisionPlane(Vector3(-5.0F, 5.0F, 0.0F), Vector3(0.0F, 0.0F, 1.0F),
                    Vector3(1.0F, 0.0F, 0.0F), Vector3(0.0F, -1.0F, 0.0F)),
                CollisionPlane(Vector3(-5.0F, 5.0F, 0.0F), Vector3(0.0F, 0.0F, -1.0F),
                    Vector3(0.0F, -1.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F)),
                CollisionPlane(Vector3(0.0F, 10.0F, 0.0F), Vector3(-1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 0.0F, -1.0F), Vector3(0.0F, -1.0F, 0.0F)),
                CollisionPlane(Vector3(0.0F, 10.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, -1.0F, 0.0F), Vector3(0.0F, 0.0F, -1.0F)),
                CollisionPlane(Vector3(0.0F, 10.0F, 0.0F), Vector3(-1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 1.0F, 0.0F), Vector3(0.0F, 0.0F, -1.0F)),
                CollisionPlane(Vector3(0.0F, 10.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 0.0F, -1.0F), Vector3(0.0F, 1.0F, 0.0F))
            });

    bool SpectatorMode::IsSpectator() noexcept
    {
        return _spectatorEnabled != 0;
    }

    std::int32_t SpectatorMode::SpectatorTarget() noexcept
    {
        return _spectatorTarget;
    }

    void SpectatorMode::EnableSpectator() noexcept
    {
        _spectatorEnabled = 1;
    }

    void SpectatorMode::CancelSpectator() noexcept
    {
        _spectatorEnabled = 0;
    }

    void SpectatorMode::EndSpectating()
    {
        if (_spectatorEnabled == 1)
        {
            if (_spectatorTarget != -1)
            {
                std::shared_ptr<PlayerEntity> target
                    = PlayerEntity::Players()[static_cast<std::size_t>(_spectatorTarget)];
                RequireReference(target).SetIsMainPlayer(false);
            }
            _spectatorEnabled = 2;
            _spectatorTarget = -1;
        }
    }

    bool SpectatorMode::TrySelectSpectatorTarget(std::int32_t slotIndex)
    {
        if (_spectatorEnabled == 0
            || slotIndex == GameState::MainPlayerIndex()
            || slotIndex < 0 || slotIndex > 3)
        {
            return false;
        }
        std::shared_ptr<PlayerEntity> player
            = PlayerEntity::Players()[static_cast<std::size_t>(slotIndex)];
        PlayerEntity& playerRef = RequireReference(player);
        if (playerRef.Health() > 0
            && TestFlag(playerRef.LoadFlags(), LoadFlags::Active)
            && !playerRef.IsBot())
        {
            _spectatorTarget = slotIndex;
            return true;
        }
        return false;
    }

    void SpectatorMode::UpdateSpectatorTarget()
    {
        if (_spectatorEnabled == 1)
        {
            if (PlayerEntity::Main() == nullptr
                || RequireReference(PlayerEntity::Main()).Health() > 0
                || RequireReference(PlayerEntity::Main()).IsAltForm())
            {
                EndSpectating();
                return;
            }
            if (_spectatorTarget == -1)
            {
                for (std::int32_t i = 0; i < 4; ++i)
                {
                    if (TrySelectSpectatorTarget(i))
                    {
                        break;
                    }
                }
                if (_spectatorTarget == -1)
                {
                    return;
                }
            }
            std::shared_ptr<PlayerEntity> target
                = PlayerEntity::Players()[static_cast<std::size_t>(_spectatorTarget)];
            PlayerEntity& targetRef = RequireReference(target);
            targetRef.SetIsMainPlayer(true);
            if (_prevPlayerTarget != _spectatorTarget)
            {
                _prevCameraInfo.reset();
                _prevPlayerTarget = _spectatorTarget;
            }
            if (targetRef.Health() == 0 || targetRef.IsBot())
            {
                targetRef.SetIsMainPlayer(false);
                for (std::int32_t i = 0; i < 4; ++i)
                {
                    if (TrySelectSpectatorTarget(i))
                    {
                        break;
                    }
                }
            }
        }
    }

    void SpectatorMode::ClearPlayerPointers()
    {
        if (_spectatorEnabled == 1 && _spectatorTarget != -1)
        {
            std::shared_ptr<PlayerEntity> target
                = PlayerEntity::Players()[static_cast<std::size_t>(_spectatorTarget)];
            RequireReference(target).SetIsMainPlayer(false);
        }
    }

    void SpectatorMode::ResetPlayerPointers()
    {
        if (_spectatorEnabled == 1 && _spectatorTarget != -1)
        {
            std::shared_ptr<PlayerEntity> target
                = PlayerEntity::Players()[static_cast<std::size_t>(_spectatorTarget)];
            RequireReference(target).SetIsMainPlayer(true);
        }
    }

    void SpectatorMode::CheckForNextTarget(const std::shared_ptr<PlayerEntity>& player)
    {
        if (_spectatorEnabled != 1 || _spectatorTarget == -1)
        {
            return;
        }
        PlayerEntity& playerRef = RequireReference(player);
        if (playerRef.IsBot() || playerRef.Health() == 0)
        {
            const std::int32_t prevTarget = _spectatorTarget;
            for (std::int32_t i = _spectatorTarget + 1; i < 4; ++i)
            {
                if (TrySelectSpectatorTarget(i))
                {
                    break;
                }
            }
            if (prevTarget == _spectatorTarget)
            {
                for (std::int32_t i = 0; i < _spectatorTarget; ++i)
                {
                    if (TrySelectSpectatorTarget(i))
                    {
                        break;
                    }
                }
            }
        }
    }

    std::shared_ptr<CameraInfo> SpectatorMode::GetCameraInfo(
        const std::shared_ptr<PlayerEntity>& player,
        std::shared_ptr<CameraInfo> cameraInfo)
    {
        (void)player;
        if (_spectatorEnabled == 1 && _spectatorTarget != -1)
        {
            std::shared_ptr<PlayerEntity> origMain = PlayerEntity::Main();
            std::shared_ptr<PlayerEntity> target
                = PlayerEntity::Players()[static_cast<std::size_t>(_spectatorTarget)];
            PlayerEntity& targetRef = RequireReference(target);
            PlayerEntity::SetMain(target);
            if (!targetRef.IsAltForm())
            {
                Entities::PlayerCamera::UpdateCameraPosition(target);
                Entities::PlayerCamera::UpdateAimVecs(target);
                if (Entities::PlayerCamera::Mode() == Entities::CameraModeValues::Free)
                {
                    Entities::PlayerCamera::UpdateFreeCamera(target);
                }
                else if (Entities::PlayerCamera::Mode() != Entities::CameraModeValues::Intro)
                {
                    Entities::PlayerCamera::UpdateFirstPersonCamera(target);
                }
                Entities::PlayerCamera::UpdateCameraSequence(target);
            }
            cameraInfo = std::make_shared<CameraInfo>(Entities::PlayerCamera::CameraInfo());
            PlayerEntity::SetMain(origMain);
            _prevCameraInfo = cameraInfo;
        }
        else if (_spectatorEnabled == 2)
        {
            assert(_prevCameraInfo != nullptr);
            cameraInfo = _prevCameraInfo;
            _prevCameraInfo.reset();
            _prevPlayerTarget = -1;
            _spectatorEnabled = 0;
        }
        return cameraInfo;
    }

    void SpectatorMode::MoveReflectedCamera(const std::shared_ptr<PlayerEntity>& player)
    {
        if (_spectatorEnabled == 1 && _spectatorTarget != -1)
        {
            std::shared_ptr<PlayerEntity> target
                = PlayerEntity::Players()[static_cast<std::size_t>(_spectatorTarget)];
            PlayerEntity& targetRef = RequireReference(target);
            if (!targetRef.IsAltForm())
            {
                RequireReference(targetRef.CameraInfo()).Position
                    = RequireReference(RequireReference(player).CameraInfo()).Position;
                RequireReference(targetRef.CameraInfo()).Target
                    = RequireReference(RequireReference(player).CameraInfo()).Target;
                RequireReference(targetRef.CameraInfo()).UpVector
                    = RequireReference(RequireReference(player).CameraInfo()).UpVector;
            }
        }
    }

    void SpectatorMode::DrawSpectated(const std::shared_ptr<PlayerEntity>& player)
    {
        if (ShouldDrawPlayer(player))
        {
            PlayerEntity& playerRef = RequireReference(player);
            playerRef.DrawModel();
            playerRef.DrawAltForm();
            if (playerRef.Health() > 0 && !playerRef.IsAltForm())
            {
                if (!VectorEquals(playerRef.Emission(), Vector3(1.0F, 1.0F, 1.0F))
                    && playerRef.Effectiveness() != Entities::EffectivenessValues::Invisible)
                {
                    Entities::PlayerDraw::DrawGunSmoke(player);
                }
                Entities::PlayerDraw::DrawMuzzleFx(player);
                if (playerRef.Zoomed())
                {
                    Entities::PlayerDraw::DrawZoom(player);
                }
                Entities::PlayerDraw::DrawIceLayer(player);
            }
            if (playerRef.Health() > 0)
            {
                playerRef.DrawMorphBallTrail();
            }
            playerRef.DrawOverheadIcon();
            if (PlayerEntity::Main() != nullptr
                && RequireReference(PlayerEntity::Main()).TeamIndex() != -1
                && playerRef.Effectiveness() != Entities::EffectivenessValues::Invisible
                && playerRef.Health() > 0
                && playerRef.TeamIndex() != RequireReference(PlayerEntity::Main()).TeamIndex())
            {
                playerRef.DrawShield();
            }
            if (playerRef.IsAltForm())
            {
                playerRef.DrawAltAttack();
            }
        }
    }

    bool SpectatorMode::ShouldDrawPlayer(const std::shared_ptr<PlayerEntity>& player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        if (_spectatorEnabled == 1 && playerRef.SlotIndex() == _spectatorTarget)
        {
            if (playerRef.IsAltForm()
                && playerRef.AltForm() == Entities::PlayerAltFormValues::Sylux)
            {
                return false;
            }
            CameraInfo& cameraInfo = RequireReference(playerRef.CameraInfo());
            if (!cameraInfo.IsThirdPerson())
            {
                return false;
            }
        }
        if (playerRef.Health() == 0 && GameState::MatchState() == MatchStateValues::Playing)
        {
            return false;
        }
        if (GameState::MatchState() != MatchStateValues::Playing
            && playerRef.SlotIndex() != GameState::MainPlayerIndex())
        {
            return false;
        }
        if (!TestFlag(playerRef.LoadFlags(), LoadFlags::Spawned))
        {
            return false;
        }
        if (!GameState::Teams() && playerRef.Cloaking() > 0)
        {
            return false;
        }
        return true;
    }

    void SpectatorMode::DrawIceOverlay(const std::shared_ptr<PlayerEntity>& player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        const bool active = playerRef.Health() > 0 && playerRef.FrozenGfxTimer() > 0;
        const bool noTarget = _spectatorEnabled != 0 && _spectatorTarget == -1;
        if (active || noTarget)
        {
            float pct = noTarget
                ? 1.0F
                : static_cast<float>(playerRef.FrozenGfxTimer()) / static_cast<float>(30 * 2);
            if (pct > 1.0F)
            {
                pct = 1.0F;
            }
            else
            {
                pct *= pct;
            }
            const float scaleX = 2.0F * (1.0F - pct * 0.35F);
            const float scaleY = 2.0F * (1.0F - pct * 0.2F);

            using namespace OpenTK::Graphics::OpenGL;
            Gl::Enable(EnableCap::Blend);
            Gl::BlendFunc(BlendingFactor::SrcAlpha, BlendingFactor::OneMinusSrcAlpha);
            Gl::MatrixMode(MatrixMode::Modelview);
            Gl::LoadIdentity();
            Gl::MatrixMode(MatrixMode::Projection);
            Gl::PushMatrix();
            Matrix4 proj = CreateOrthographicOffCenter(0.0F, 1.0F, 0.0F, 1.0F, -1.0F, 1.0F);
            Gl::LoadMatrix(proj);
            Gl::BindTexture(TextureTarget::Texture2D, Textures::IceLayerTextureId());
            Gl::Disable(EnableCap::CullFace);
            Gl::Disable(EnableCap::DepthTest);
            Gl::PolygonMode(MaterialFace::FrontAndBack, PolygonMode::Fill);
            Gl::Begin(PrimitiveType::Quads);
            Gl::Color3(1.0F, 1.0F, 1.0F);
            Gl::TexCoord2(0.5F - scaleX / 2.0F, 0.5F + scaleY / 2.0F);
            Gl::Vertex3(0.0F, 0.0F, 0.0F);
            Gl::TexCoord2(0.5F + scaleX / 2.0F, 0.5F + scaleY / 2.0F);
            Gl::Vertex3(1.0F, 0.0F, 0.0F);
            Gl::TexCoord2(0.5F + scaleX / 2.0F, 0.5F - scaleY / 2.0F);
            Gl::Vertex3(1.0F, 1.0F, 0.0F);
            Gl::TexCoord2(0.5F - scaleX / 2.0F, 0.5F - scaleY / 2.0F);
            Gl::Vertex3(0.0F, 1.0F, 0.0F);
            Gl::End();
            Gl::Enable(EnableCap::CullFace);
            Gl::Enable(EnableCap::DepthTest);
            Gl::MatrixMode(MatrixMode::Projection);
            Gl::PopMatrix();
            Gl::MatrixMode(MatrixMode::Modelview);
            Gl::BindTexture(TextureTarget::Texture2D, 0);
            Gl::Color4(1.0F, 1.0F, 1.0F, 1.0F);
        }
    }

    void SpectatorMode::DrawHud()
    {
        if (_spectatorEnabled == 1 && !UiHud::DebugCamera())
        {
            if (_spectatorTarget == -1)
            {
                DrawFusionBanned();
            }
            else
            {
                std::shared_ptr<PlayerEntity> target
                    = PlayerEntity::Players()[static_cast<std::size_t>(_spectatorTarget)];
                PlayerEntity& targetRef = RequireReference(target);
                std::shared_ptr<PlayerEntity> origMain = PlayerEntity::Main();
                PlayerEntity::SetMain(target);
                if (targetRef.IsAltForm())
                {
                    Entities::PlayerHud::DrawAltHud(target);
                }
                else
                {
                    Entities::PlayerHud::DrawHud(target);
                    if (targetRef.Health() > 0 && targetRef.Zoomed())
                    {
                        Entities::PlayerHud::DrawZoomHud(target);
                    }
                }
                PlayerEntity::SetMain(origMain);
            }
        }
    }

    void SpectatorMode::DrawFusionBanned()
    {
        std::shared_ptr<CameraInfo> cameraInfo
            = std::make_shared<CameraInfo>(_fbPosition, _fbPosition + _fbFacing, _fbUp, 78.0F);
        static const std::vector<std::shared_ptr<EntityBase>> emptyEntities{};
        Scene::RenderRoom(0, cameraInfo, false,
            OpenTK::Graphics::OpenGL::PolygonMode::Fill, emptyEntities,
            MphRead::Formats::Culling::NodeRef::None, _fbCollision, false);

        using namespace OpenTK::Graphics::OpenGL;
        Gl::Disable(EnableCap::CullFace);
        Gl::Disable(EnableCap::DepthTest);
        Gl::MatrixMode(MatrixMode::Projection);
        Gl::LoadIdentity();
        Gl::Ortho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
        Gl::MatrixMode(MatrixMode::Modelview);
        Gl::LoadIdentity();
        Gl::PolygonMode(MaterialFace::FrontAndBack, PolygonMode::Fill);
        Gl::Begin(PrimitiveType::Quads);
        Gl::Color3(0.0F, 0.0F, 0.0F);
        Gl::Vertex3(-1.0F, -1.0F, 0.0F);
        Gl::Vertex3(1.0F, -1.0F, 0.0F);
        Gl::Vertex3(1.0F, 1.0F, 0.0F);
        Gl::Vertex3(-1.0F, 1.0F, 0.0F);
        Gl::End();
        Gl::Enable(EnableCap::CullFace);
        Gl::Enable(EnableCap::DepthTest);
        Menu::DrawLogo(-210, 140, 1.5F);
    }
}

#pragma once

#include "Formats/Enums.hpp"
#include "Formats/Types.hpp"
#include "Selection.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace OpenTK::Mathematics
{
    struct Vector2i final
    {
        std::int32_t X = 0;
        std::int32_t Y = 0;

        constexpr Vector2i() noexcept = default;
        constexpr Vector2i(std::int32_t x, std::int32_t y) noexcept : X(x), Y(y) {}

        friend constexpr bool operator==(Vector2i left, Vector2i right) noexcept
        {
            return left.X == right.X && left.Y == right.Y;
        }
        friend constexpr bool operator!=(Vector2i left, Vector2i right) noexcept
        {
            return !(left == right);
        }
    };


}


namespace OpenTK::Graphics::OpenGL
{
    enum class FramebufferErrorCode : std::int32_t
    {
        FramebufferUndefined = 0x8219,
        FramebufferComplete = 0x8CD5,
        FramebufferIncompleteAttachment = 0x8CD6,
        FramebufferIncompleteMissingAttachment = 0x8CD7,
        FramebufferIncompleteDrawBuffer = 0x8CDB,
        FramebufferIncompleteReadBuffer = 0x8CDC,
        FramebufferUnsupported = 0x8CDD,
        FramebufferIncompleteMultisample = 0x8D56,
        FramebufferIncompleteLayerTargets = 0x8DA8
    };

    enum class ErrorCode : std::int32_t
    {
        NoError = 0,
        InvalidEnum = 0x0500,
        InvalidValue = 0x0501,
        InvalidOperation = 0x0502,
        StackOverflow = 0x0503,
        StackUnderflow = 0x0504,
        OutOfMemory = 0x0505,
        InvalidFramebufferOperation = 0x0506,
        ContextLost = 0x0507,
        TableTooLarge = 0x8031
    };
}

namespace MphRead::NativeRuntime
{
    // Narrow runtime boundaries for BCL/runtime operations directly observed by
    // Renderer.cs. Implementations belong to the shared native runtime provider.
    void ForceFullGc();
    [[nodiscard]] bool IsAndroid();
    void SetSustainedLowLatencyGc();
    [[nodiscard]] bool DebuggerAttached();
    void DebuggerBreak();
    void ReturnToSharedArrayPool(
        const std::shared_ptr<ManagedArray<OpenTK::Mathematics::Vector3>>& array);
}

namespace MphRead::RendererDetail
{
    [[nodiscard]] constexpr OpenTK::Mathematics::Matrix4 IdentityMatrix() noexcept
    {
        return OpenTK::Mathematics::Matrix4(
            OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }
}

namespace MphRead
{
    class Scene;
    class RenderWindow;
    class TextureMap;
    namespace Formats::Culling { struct NodeRef; struct FrustumPlane; class FrustumInfo; }
    class RoomMetadata;
    class Model;
    class ModelInstance;
    class Material;
    class Mesh;
    namespace Formats::Collision { class EntityCollision; struct CollisionResult; }
    class ShaderLocations;
    namespace Hud { class LayerInfo; class HudObjectInstance; }
    class Effect;
    class EffectElement;
    namespace Effects
    {
        class EffectEntry;
        class EffectElementEntry;
        class EffectParticle;
        class SingleParticle;
    }
    class BeamEffectEntityData;
    class LightInfo;
    class Node;

    namespace Entities
    {
        class EntityBase;
        class PlayerEntity;
        class RoomEntity;
        class BeamEffectEntity;
        class BombEntity;
        class PlatformEntity;
        class EnemyInstanceEntity;
        class PointModuleEntity;
    }

    enum class VolumeDisplay : std::int32_t
    {
        None,
        LightColor1,
        LightColor2,
        TriggerParent,
        TriggerChild,
        AreaInside,
        AreaExit,
        MorphCamera,
        JumpPad,
        Teleporter,
        EnemyHurt,
        Object,
        FlagBase,
        DefenseNode,
        KillPlane,
        PlayerLimit,
        CameraLimit,
        NodeBounds,
        NodeData,
        Portal
    };

    enum class CollisionType : std::int32_t
    {
        Any,
        Player,
        Beam,
        Both
    };

    enum class CollisionColor : std::int32_t
    {
        None,
        Entity,
        Terrain,
        Type
    };

    enum class CameraMode : std::int32_t
    {
        Pivot,
        Roam,
        Player
    };

    enum class AfterFade : std::int32_t
    {
        None,
        Exit,
        LoadRoom,
        PlayMovie,
        StopMovie,
        EnterShip
    };

    namespace RendererPlatform
    {
        using Key = ::OpenTK::Windowing::GraphicsLibraryFramework::Keys;
        using MouseButton = ::OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;
        using KeyboardState = ::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState;
        using MouseState = ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState;
        using KeyboardKeyEventArgs = ::OpenTK::Windowing::Common::KeyboardKeyEventArgs;

        // PlayerInput.hpp owns the canonical native OpenTK key enum but its current
        // source slice does not yet name every OpenTK key Renderer.cs observes.
        // These are the exact OpenTK/GLFW numeric key values, kept as constants
        // rather than extending or duplicating that provider-owned enum.
        inline constexpr Key PeriodKey = static_cast<Key>(46);
        inline constexpr Key LeftControlKey = static_cast<Key>(341);
        inline constexpr Key LeftAltKey = static_cast<Key>(342);
        inline constexpr Key RightShiftKey = static_cast<Key>(344);
        inline constexpr Key RightControlKey = static_cast<Key>(345);
        inline constexpr Key RightAltKey = static_cast<Key>(346);
        enum class CursorState : std::int32_t { Normal, Grabbed };
        enum class VSyncMode : std::int32_t { Off, On };

        class GLFWException final : public std::runtime_error
        {
        public:
            GLFWException(std::string description, std::int32_t errorCode)
                : std::runtime_error(std::move(description)), _errorCode(errorCode)
            {
            }

            [[nodiscard]] std::int32_t ErrorCode() const noexcept { return _errorCode; }

        private:
            std::int32_t _errorCode;
        };

        struct FrameEventArgs final { double Time = 0.0; };
        struct ResizeEventArgs final { OpenTK::Mathematics::Vector2i Size{}; };
        struct MouseButtonEventArgs final { MouseButton Button{}; };
        struct MouseMoveEventArgs final { float DeltaX = 0.0F; float DeltaY = 0.0F; };
        struct MouseWheelEventArgs final { float OffsetY = 0.0F; };
        struct TextInputEventArgs final { std::uint32_t Unicode = 0; };

        struct WindowSettings final
        {
            OpenTK::Mathematics::Vector2i ClientSize{1280, 768};
            std::string Title{};
            double UpdateFrequency = 0.0;
            bool StartVisible = false;
            enum class ContextProfile : std::int32_t { Compatability };
            enum class ContextFlags : std::int32_t { Default };
            ContextProfile Profile = ContextProfile::Compatability;
            ContextFlags Flags = ContextFlags::Default;
            std::int32_t ApiMajor = 3;
            std::int32_t ApiMinor = 2;
        };

        class Window
        {
        public:
            virtual ~Window() = default;
            [[nodiscard]] virtual OpenTK::Mathematics::Vector2i Size() const = 0;
            [[nodiscard]] virtual KeyboardState& Keyboard() = 0;
            [[nodiscard]] virtual MouseState& Mouse() = 0;
            virtual void Title(std::string value) = 0;
            virtual void MinimumSize(OpenTK::Mathematics::Vector2i value) = 0;
            virtual void Cursor(RendererPlatform::CursorState value) = 0;
            virtual void VSync(RendererPlatform::VSyncMode value) = 0;
            virtual void UpdateFrequency(double value) = 0;
            virtual void Visible(bool value) = 0;
            virtual void Close() = 0;
            virtual void SwapBuffers() = 0;
            virtual void BaseOnClosing() = 0;
            virtual void BaseOnLoad() = 0;
            virtual void BaseOnRenderFrame(const FrameEventArgs& args) = 0;
            virtual void BaseOnResize(const ResizeEventArgs& e) = 0;
            virtual void BaseOnMouseDown(const MouseButtonEventArgs& e) = 0;
            virtual void BaseOnMouseUp(const MouseButtonEventArgs& e) = 0;
            virtual void BaseOnMouseMove(const MouseMoveEventArgs& e) = 0;
            virtual void BaseOnMouseWheel(const MouseWheelEventArgs& e) = 0;
            virtual void BaseOnTextInput(const TextInputEventArgs& e) = 0;
            virtual void BaseOnKeyDown(const KeyboardKeyEventArgs& e) = 0;
        };

        [[nodiscard]] std::shared_ptr<Window> CreateWindow(const WindowSettings& settings);
        [[nodiscard]] bool IsLinux();
        [[nodiscard]] std::optional<std::string> EnvironmentVariable(std::string_view name);
        [[nodiscard]] OpenTK::Mathematics::Vector2i WorkAreaForWindow(Window& window);
        void InstallGlfwErrorCallback(std::function<void(std::int32_t, std::string)> callback);
        [[nodiscard]] std::int32_t GlfwFeatureUnavailableCode();
        void ConsoleClear();
        void ConsoleWrite(std::string_view text);
        void ConsoleWriteLine(std::string_view text);
        [[nodiscard]] std::optional<std::string> ConsoleReadLine();
    }

    template <typename T>
    class RendererConcurrentQueue final
    {
    public:
        RendererConcurrentQueue() = default;
        RendererConcurrentQueue(const RendererConcurrentQueue&) = delete;
        RendererConcurrentQueue& operator=(const RendererConcurrentQueue&) = delete;

        void Enqueue(T value)
        {
            std::scoped_lock lock(_mutex);
            _items.push_back(std::move(value));
        }

        [[nodiscard]] bool TryDequeue(T& value)
        {
            std::scoped_lock lock(_mutex);
            if (_items.empty())
            {
                return false;
            }
            value = std::move(_items.front());
            _items.pop_front();
            return true;
        }

        [[nodiscard]] std::int32_t Count() const
        {
            std::scoped_lock lock(_mutex);
            return static_cast<std::int32_t>(_items.size());
        }

    private:
        mutable std::mutex _mutex{};
        std::deque<T> _items{};
    };

    struct TextureMapValue final
    {
        std::int32_t BindingId = 0;
        bool OnlyOpaque = false;
    };

    class TextureMap final
    {
    public:
        [[nodiscard]] TextureMapValue Get(std::int32_t textureId, std::int32_t paletteId,
            std::int32_t recolorId) const;
        void Add(std::int32_t textureId, std::int32_t paletteId, std::int32_t recolorId,
            std::int32_t bindingId, bool onlyOpaque);

    private:
        friend class Scene;

        [[nodiscard]] static std::int32_t GetKey(std::int32_t textureId,
            std::int32_t paletteId, std::int32_t recolorId);
        std::vector<std::pair<std::int32_t, TextureMapValue>> _items{};
    };

    class RenderWindow final
    {
    public:
        static void LogCreatingWindow();
        RenderWindow();
        RenderWindow(const RenderWindow&) = delete;
        RenderWindow& operator=(const RenderWindow&) = delete;
        RenderWindow(RenderWindow&&) = delete;
        RenderWindow& operator=(RenderWindow&&) = delete;
        ~RenderWindow();

        [[nodiscard]] MphRead::Scene& Scene() const;
        void AddRoom(std::int32_t id, GameMode mode = GameMode::None,
            std::int32_t playerCount = 0, BossFlags bossFlags = BossFlags::Unspecified,
            std::int32_t nodeLayerMask = 0, std::int32_t entityLayerId = -1);
        void AddRoom(std::string name, GameMode mode = GameMode::None,
            std::int32_t playerCount = 0, BossFlags bossFlags = BossFlags::Unspecified,
            std::int32_t nodeLayerMask = 0, std::int32_t entityLayerId = -1);
        void AddModel(std::string name, std::int32_t recolor = 0, bool firstHunt = false,
            MetaDir dir = MetaDir::Models,
            std::optional<OpenTK::Mathematics::Vector3> pos = std::nullopt);
        void AddPlayer(Hunter hunter, std::int32_t recolor = 0, std::int32_t team = -1,
            std::optional<OpenTK::Mathematics::Vector3> position = std::nullopt);
        void QueueMovie(std::int32_t movieId);

        void OnClosing();
        void OnLoad();
        void OnRenderFrame(const RendererPlatform::FrameEventArgs& args);
        void OnResize(const RendererPlatform::ResizeEventArgs& e);
        void OnMouseDown(const RendererPlatform::MouseButtonEventArgs& e);
        void OnMouseUp(const RendererPlatform::MouseButtonEventArgs& e);
        void OnMouseMove(const RendererPlatform::MouseMoveEventArgs& e);
        void OnMouseWheel(const RendererPlatform::MouseWheelEventArgs& e);
        void OnTextInput(const RendererPlatform::TextInputEventArgs& e);
        void OnKeyDown(const RendererPlatform::KeyboardKeyEventArgs& e);

    private:
        static RendererPlatform::WindowSettings MakeSettings();
        static bool OnWayland();
        static void IgnoreUnavailableGlfwFeatures();
        void FitToScreen();
        void ApplyFrameRateSettings();

        static std::function<void(std::int32_t, std::string)> _glfwErrorCallback;
        static constexpr OpenTK::Mathematics::Vector2i _minimumSize{1024, 720};
        std::shared_ptr<RendererPlatform::Window> _window{};
        std::shared_ptr<MphRead::Scene> _scene{};
        bool _startedHidden = true;
        std::int32_t _applyStartupIn = 0;
        bool _sceneReady = false;
        std::int32_t _appliedFrameRateCap = -1;
    };


}

#define MPHREAD_SCENE_RENDERER_MEMBERS \
public: \
    Scene(OpenTK::Mathematics::Vector2i size, \
        MphRead::RendererPlatform::KeyboardState& keyboardState, \
        MphRead::RendererPlatform::MouseState& mouseState, \
        std::function<void(std::string)> setTitle, std::function<void()> close); \
    [[nodiscard]] OpenTK::Mathematics::Vector2i Size() const noexcept; \
    void Size(OpenTK::Mathematics::Vector2i value) noexcept; \
    [[nodiscard]] OpenTK::Mathematics::Matrix4 PerspectiveMatrix() const noexcept; \
    [[nodiscard]] MphRead::CameraMode CameraMode() const noexcept; \
    [[nodiscard]] bool ShowCursor() const; \
    [[nodiscard]] MphRead::Formats::Culling::FrustumInfo& FrustumInfo() const; \
    [[nodiscard]] bool FrameAdvance() const noexcept; \
    [[nodiscard]] bool FrameAdvanceLastFrame() const noexcept; \
    [[nodiscard]] bool ProcessFrame() const noexcept; \
    [[nodiscard]] bool Exiting() const noexcept; \
    [[nodiscard]] std::int32_t RoomId() const noexcept; \
    void RoomId(std::int32_t value) noexcept; \
    [[nodiscard]] std::int32_t AreaId() const noexcept; \
    void AreaId(std::int32_t value) noexcept; \
    static void Language(MphRead::Language value); \
    [[nodiscard]] OpenTK::Mathematics::Matrix4 ViewMatrix() const noexcept; \
    [[nodiscard]] OpenTK::Mathematics::Matrix4 ViewInvRotMatrix() const noexcept; \
    [[nodiscard]] OpenTK::Mathematics::Matrix4 ViewInvRotYMatrix() const noexcept; \
    [[nodiscard]] OpenTK::Mathematics::Vector3 CameraPosition() const noexcept; \
    [[nodiscard]] bool ShowNodeData() const noexcept; \
    [[nodiscard]] bool ShowInvisibleEntities() const noexcept; \
    [[nodiscard]] bool ShowAllEntities() const noexcept; \
    [[nodiscard]] bool TransformRoomNodes() const noexcept; \
    [[nodiscard]] bool ShowAllNodes() const noexcept; \
    void ShowAllNodes(bool value) noexcept; \
    [[nodiscard]] float FrameTime() const noexcept; \
    [[nodiscard]] std::uint64_t FrameCount() const noexcept; \
    [[nodiscard]] std::uint64_t LiveFrames() const noexcept; \
    [[nodiscard]] float ElapsedTime() const noexcept; \
    [[nodiscard]] float GlobalElapsedTime() const noexcept; \
    [[nodiscard]] MphRead::VolumeDisplay ShowVolumes() const noexcept; \
    [[nodiscard]] bool ShowForceFields() const noexcept; \
    [[nodiscard]] float KillHeight() const noexcept; \
    [[nodiscard]] bool ScanVisor() const; \
    [[nodiscard]] OpenTK::Mathematics::Vector3 Light1Vector() const noexcept; \
    [[nodiscard]] OpenTK::Mathematics::Vector3 Light1Color() const noexcept; \
    [[nodiscard]] OpenTK::Mathematics::Vector3 Light2Vector() const noexcept; \
    [[nodiscard]] OpenTK::Mathematics::Vector3 Light2Color() const noexcept; \
    [[nodiscard]] std::shared_ptr<MphRead::Entities::RoomEntity> Room() const noexcept; \
    [[nodiscard]] std::int32_t ActiveCutscene() const noexcept; \
    [[nodiscard]] bool AllowCameraMovement() const noexcept; \
    static constexpr std::int32_t DisplaySphereStacks = 16; \
    static constexpr std::int32_t DisplaySphereSectors = 24; \
    void AddRoom(std::string name, MphRead::GameMode mode = MphRead::GameMode::None, \
        std::int32_t playerCount = 0, MphRead::BossFlags bossFlags = MphRead::BossFlags::Unspecified, \
        std::int32_t nodeLayerMask = 0, std::int32_t entityLayerId = -1); \
    void SetRoomValues(const MphRead::RoomMetadata& meta); \
    std::shared_ptr<MphRead::Entities::EntityBase> AddModel(std::string name, std::int32_t recolor = 0, \
        bool firstHunt = false, MphRead::MetaDir dir = MphRead::MetaDir::Models, \
        std::optional<OpenTK::Mathematics::Vector3> pos = std::nullopt); \
    void AddPlayer(MphRead::Hunter hunter, std::int32_t recolor = 0, std::int32_t team = -1, \
        std::optional<OpenTK::Mathematics::Vector3> position = std::nullopt); \
    [[nodiscard]] MphRead::Formats::Culling::NodeRef UpdateNodeRef(MphRead::Formats::Culling::NodeRef current, \
        OpenTK::Mathematics::Vector3 prevPos, OpenTK::Mathematics::Vector3 curPos); \
    [[nodiscard]] MphRead::Formats::Culling::NodeRef GetNodeRefByName(std::string nodeName); \
    [[nodiscard]] bool PartCouldContain(std::int32_t partIndex, OpenTK::Mathematics::Vector3 position); \
    [[nodiscard]] MphRead::Formats::Culling::NodeRef GetNodeRefByPosition(OpenTK::Mathematics::Vector3 position); \
    [[nodiscard]] bool IsNodeRefVisible(MphRead::Formats::Culling::NodeRef nodeRef); \
    [[nodiscard]] bool IsNodeRefAudible(MphRead::Formats::Culling::NodeRef nodeRef); \
    void OnLoad(); \
    [[nodiscard]] OpenTK::Mathematics::Vector2i RenderSize() const; \
    void OnResize(); \
    void LoadModel(std::string name, bool firstHunt = false); \
    void LoadModel(const std::shared_ptr<MphRead::Model>& model, bool isRoom = false); \
    [[nodiscard]] std::int32_t BindGetTexture(const std::shared_ptr<MphRead::Model>& model, \
        std::int32_t textureId, std::int32_t paletteId, std::int32_t recolorId); \
    [[nodiscard]] std::int32_t BindGetTexture(const std::vector<MphRead::ColorRgba>& data, \
        std::int32_t width, std::int32_t height); \
    void BindTexture(const std::vector<MphRead::ColorRgba>& data, std::int32_t width, \
        std::int32_t height, std::int32_t bindingId); \
    void UpdateMaterials(const std::shared_ptr<MphRead::Model>& model, std::int32_t recolorId); \
    [[nodiscard]] static bool BreakNextFrame() noexcept; \
    static void BreakNextFrame(bool value) noexcept; \
    void OnUpdateFrame(); \
    void OnSimulationFrame(); \
    void OnDrawFrame(); \
    [[nodiscard]] OpenTK::Mathematics::Matrix4 GetPerspectiveMatrix(float fov) const; \
    [[nodiscard]] static MphRead::Formats::Culling::FrustumPlane SetBoundsIndices(OpenTK::Mathematics::Vector4 plane); \
    [[nodiscard]] OpenTK::Graphics::OpenGL::FramebufferErrorCode FramebufferStatus() const noexcept; \
    [[nodiscard]] OpenTK::Graphics::OpenGL::ErrorCode DrainGlError(); \
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> ReadWindowBuffer(std::int32_t& width, std::int32_t& height); \
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> ReadSceneTarget(std::int32_t& width, std::int32_t& height); \
    void AfterRenderFrame(); \
    [[nodiscard]] float FramesPerSecond() const noexcept; \
    [[nodiscard]] bool OnRenderFrame(); \
    void UnloadModel(const std::shared_ptr<MphRead::Model>& model); \
    void StartCutscene(std::int32_t id); \
    void EndCutscene(bool resetFade = false); \
    void ResetFrameCount(); \
    std::shared_ptr<MphRead::Entities::BeamEffectEntity> InitBeamEffect(const MphRead::BeamEffectEntityData& data); \
    void UnlinkBeamEffect(const std::shared_ptr<MphRead::Entities::BeamEffectEntity>& entry); \
    std::shared_ptr<MphRead::Entities::BombEntity> InitBomb(); \
    void UnlinkBomb(const std::shared_ptr<MphRead::Entities::BombEntity>& entry); \
    void AddSingleParticle(MphRead::SingleType type, OpenTK::Mathematics::Vector3 position, \
        OpenTK::Mathematics::Vector3 color, float alpha, float scale); \
    void UnlinkEffectEntry(const std::shared_ptr<MphRead::Effects::EffectEntry>& entry); \
    void DetachEffectEntry(const std::shared_ptr<MphRead::Effects::EffectEntry>& entry, bool setExpired); \
    void LoadEffect(std::int32_t effectId, bool persistent); \
    std::shared_ptr<MphRead::Effects::EffectEntry> SpawnEffectGetEntry(std::int32_t effectId, \
        OpenTK::Mathematics::Vector3 facing, OpenTK::Mathematics::Vector3 up, \
        OpenTK::Mathematics::Vector3 position, std::shared_ptr<MphRead::Formats::Collision::EntityCollision> entCol = nullptr); \
    std::shared_ptr<MphRead::Effects::EffectEntry> SpawnEffectGetEntry(std::int32_t effectId, \
        OpenTK::Mathematics::Matrix4 transform, std::shared_ptr<MphRead::Formats::Collision::EntityCollision> entCol = nullptr); \
    void SpawnEffect(std::int32_t effectId, OpenTK::Mathematics::Vector3 facing, \
        OpenTK::Mathematics::Vector3 up, OpenTK::Mathematics::Vector3 position, bool child = false, \
        std::shared_ptr<MphRead::Formats::Collision::EntityCollision> entCol = nullptr); \
    void SpawnEffect(std::int32_t effectId, OpenTK::Mathematics::Matrix4 transform, bool child = false, \
        std::shared_ptr<MphRead::Formats::Collision::EntityCollision> entCol = nullptr); \
    [[nodiscard]] std::int32_t CountElements(std::int32_t effectId); \
    void ClearEffects(); \
    void ClearNonPersistentEffects(); \
    [[nodiscard]] std::int64_t ModEffectParticles() const noexcept; \
    void AddRenderItem(const MphRead::Material& material, std::int32_t polygonId, float alphaScale, \
        OpenTK::Mathematics::Vector3 emission, const MphRead::LightInfo& lightInfo, \
        OpenTK::Mathematics::Matrix4 texcoordMatrix, OpenTK::Mathematics::Matrix4 transform, \
        std::int32_t listId, std::int32_t matrixStackCount, const std::vector<float>& matrixStack, \
        std::optional<OpenTK::Mathematics::Vector4> overrideColor, \
        std::optional<OpenTK::Mathematics::Vector4> paletteOverride, MphRead::SelectionType selectionType, \
        MphRead::BillboardMode billboardMode, float scaleFactor = 1.0F, \
        std::optional<std::int32_t> bindingOverride = std::nullopt); \
    void AddRenderItem(MphRead::CullingMode cullingMode, std::int32_t polygonId, \
        OpenTK::Mathematics::Vector4 overrideColor, MphRead::RenderItemType type, \
        std::shared_ptr<MphRead::ManagedArray<OpenTK::Mathematics::Vector3>> vertices, std::int32_t vertexCount = 0, bool noLines = false); \
    void AddRenderItem(MphRead::RenderItemType type, float alpha, std::int32_t polygonId, \
        OpenTK::Mathematics::Vector3 color, MphRead::RepeatMode xRepeat, MphRead::RepeatMode yRepeat, \
        float scaleS, float scaleT, OpenTK::Mathematics::Matrix4 transform, \
        std::shared_ptr<MphRead::ManagedArray<OpenTK::Mathematics::Vector3>> uvsAndVerts, std::int32_t bindingId, \
        MphRead::BillboardMode billboardMode = MphRead::BillboardMode::None, std::int32_t trailCount = 8); \
    void AddRenderItem(MphRead::RenderItemType type, std::int32_t polygonId, \
        OpenTK::Mathematics::Vector3 color, MphRead::RepeatMode xRepeat, MphRead::RepeatMode yRepeat, \
        float scaleS, float scaleT, std::int32_t matrixStackCount, const std::vector<float>& matrixStack, \
        std::shared_ptr<MphRead::ManagedArray<OpenTK::Mathematics::Vector3>> uvsAndVerts, std::int32_t segmentCount, std::int32_t bindingId); \
    [[nodiscard]] std::int32_t GetNextPolygonId(); \
    void InitLoadedEntity(std::int32_t count); \
    [[nodiscard]] MphRead::FadeType FadeType() const noexcept; \
    void SetFade(MphRead::FadeType type, float length, bool overwrite, \
        MphRead::AfterFade afterFade = MphRead::AfterFade::None, float delay = 0.0F); \
    void DoCleanup(); \
    [[nodiscard]] std::shared_ptr<MphRead::Hud::LayerInfo> Layer1Info() const noexcept; \
    [[nodiscard]] std::shared_ptr<MphRead::Hud::LayerInfo> Layer2Info() const noexcept; \
    [[nodiscard]] std::shared_ptr<MphRead::Hud::LayerInfo> Layer3Info() const noexcept; \
    [[nodiscard]] std::shared_ptr<MphRead::Hud::LayerInfo> Layer4Info() const noexcept; \
    [[nodiscard]] std::shared_ptr<MphRead::Hud::LayerInfo> Layer5Info() const noexcept; \
    void DrawCustomCrosshair(OpenTK::Mathematics::Vector3 color, float posX = 0.5F, float posY = 0.5F); \
    void DrawHitMarker(OpenTK::Mathematics::Vector4 color, float posX = 0.5F, float posY = 0.5F); \
    void DrawHudFlatBox(float left, float top, float right, float bottom, OpenTK::Mathematics::Vector4 color); \
    void DrawHudObject(const std::shared_ptr<MphRead::Hud::HudObjectInstance>& inst, std::int32_t mode = 0, float scale = 1.0F); \
    void DrawIconModel(OpenTK::Mathematics::Vector2 position, float angle, \
        const std::shared_ptr<MphRead::ModelInstance>& inst, MphRead::ColorRgb color, float alpha); \
    void DrawHudFilterModel(const std::shared_ptr<MphRead::ModelInstance>& inst, float alpha = 1.0F); \
    void DrawHudDamageModel(const std::shared_ptr<MphRead::ModelInstance>& inst); \
    void LookAt(OpenTK::Mathematics::Vector3 target); \
    [[nodiscard]] bool IsFreeCam() const noexcept; \
    void SetFreeCamera(bool on); \
    void ToggleFreeCamera(); \
    void OnMouseClick(bool down); \
    void OnMouseMove(float deltaX, float deltaY); \
    void OnMouseWheel(float offsetY); \
    [[nodiscard]] bool ShowCollision() const noexcept; \
    [[nodiscard]] MphRead::EntityType ColEntDisplay() const noexcept; \
    [[nodiscard]] MphRead::Terrain ColTerDisplay() const noexcept; \
    [[nodiscard]] MphRead::CollisionType ColTypeDisplay() const noexcept; \
    [[nodiscard]] MphRead::CollisionColor ColDisplayColor() const noexcept; \
    [[nodiscard]] float ColDisplayAlpha() const noexcept; \
    void OnKeyDown(const MphRead::RendererPlatform::KeyboardKeyEventArgs& e); \
    MphRead::RendererConcurrentQueue<std::shared_ptr<MphRead::Entities::EntityBase>>& LoadedEntities() noexcept; \
    [[nodiscard]] bool InitEntities() const noexcept; \
    void InitEntities(bool value) noexcept; \
private: \
    struct FlatColor final \
    { \
        float _red = 0.0F; \
        float _green = 0.0F; \
        float _blue = 0.0F; \
        float _weight = 0.0F; \
        float _count = 0.0F; \
        float _plainRed = 0.0F; \
        float _plainGreen = 0.0F; \
        float _plainBlue = 0.0F; \
        void Add(MphRead::ColorRgba pixel); \
        [[nodiscard]] OpenTK::Mathematics::Vector3 Result() const; \
    }; \
    class MovieFadeSettings final \
    { \
    public: \
        MphRead::Movie MovieId{}; \
        std::optional<MphRead::Movie> AfterMovieId{}; \
        MphRead::FadeType AfterFadeType{}; \
        float AfterFadeLength = 0.0F; \
        std::optional<OpenTK::Mathematics::Vector3> AfterPosition{}; \
        std::optional<OpenTK::Mathematics::Vector3> AfterFacing{}; \
        MphRead::AfterMovie AfterMovieAction{}; \
    }; \
    enum class InputMode : std::int32_t \
    { \
        All, \
        PlayerOnly, \
        CameraOnly \
    }; \
    enum class PromptState : std::int32_t \
    { \
        None, \
        Load, \
        CameraPos \
    }; \
    void SetShaderFog(); \
    void InitShaders(); \
    void GenerateLists(const std::shared_ptr<MphRead::Model>& model, bool isRoom); \
    void DoDlist(const std::shared_ptr<MphRead::Model>& model, const MphRead::Mesh& mesh, \
        std::int32_t textureWidth, std::int32_t textureHeight, bool texgen, bool isRoom); \
    void InitTextures(const std::shared_ptr<MphRead::Model>& model); \
    bool BindTexture(const std::shared_ptr<MphRead::Model>& model, std::int32_t textureId, \
        std::int32_t paletteId, std::int32_t recolorId); \
    static OpenTK::Mathematics::Vector3 AverageOf(const std::vector<MphRead::ColorRgba>& data); \
    void UpdateMaterial(MphRead::Material& material, bool onlyOpaque); \
    void ModStepDrawPassTimers(); \
    void UpdateProjection(); \
    void UpdateDepthAttachment(OpenTK::Mathematics::Vector2i target); \
    float MeasureDepthQuantum(); \
    void DrawCelOutline(); \
    std::int32_t CelFrameBuffer(); \
    void DrawCelQuad(OpenTK::Mathematics::Vector2i target, bool probe); \
    void CalibrateInk(OpenTK::Mathematics::Vector2i target); \
    void CountFrame(); \
    void LoadAndUnload(); \
    void UnloadEntity(const std::shared_ptr<MphRead::Entities::EntityBase>& entity); \
    void TransformCamera(); \
    void UpdateCameraPosition(); \
    void ResetCamera(); \
    void UpdateCameraRotation(float stepH, float stepV); \
    void AllocateEffects(); \
    std::shared_ptr<MphRead::Effects::EffectEntry> InitEffectEntry(); \
    std::shared_ptr<MphRead::Effects::EffectElementEntry> InitEffectElement(const std::shared_ptr<MphRead::Effect>& effect, \
        const std::shared_ptr<MphRead::EffectElement>& element, std::shared_ptr<MphRead::Formats::Collision::EntityCollision> entCol, bool child); \
    void UnlinkEffectElement(const std::shared_ptr<MphRead::Effects::EffectElementEntry>& element); \
    std::shared_ptr<MphRead::Effects::EffectParticle> InitEffectParticle(); \
    void UnlinkEffectParticle(const std::shared_ptr<MphRead::Effects::EffectParticle>& particle); \
    void SpawnEffect(std::int32_t effectId, OpenTK::Mathematics::Matrix4 transform, bool child, \
        const std::shared_ptr<MphRead::Effects::EffectEntry>& entry, std::shared_ptr<MphRead::Formats::Collision::EntityCollision> entCol); \
    void ProcessEffects(std::uint64_t effectFrame); \
    std::shared_ptr<MphRead::RenderItem> GetRenderItem(); \
    void AddRenderItem(const std::shared_ptr<MphRead::RenderItem>& item); \
    void UpdateScene(); \
    void GetDrawItems(); \
    void UpdateUniforms(); \
    void UseRoomLights(); \
    void UseLight1(OpenTK::Mathematics::Vector3 vector, OpenTK::Mathematics::Vector3 color); \
    void UseLight2(OpenTK::Mathematics::Vector3 vector, OpenTK::Mathematics::Vector3 color); \
    void UpdateFade(); \
    void QuitGame(bool enteringShip); \
    void EndFade(); \
    void RenderItem(const std::shared_ptr<MphRead::RenderItem>& item); \
    void RenderBox(const MphRead::ManagedArray<OpenTK::Mathematics::Vector3>& verts); \
    void RenderCylinder(const MphRead::ManagedArray<OpenTK::Mathematics::Vector3>& verts); \
    void RenderSphere(const MphRead::ManagedArray<OpenTK::Mathematics::Vector3>& verts); \
    void RenderQuad(const MphRead::ManagedArray<OpenTK::Mathematics::Vector3>& verts); \
    void RenderNgon(const MphRead::ManagedArray<OpenTK::Mathematics::Vector3>& verts, std::int32_t count); \
    void RenderNgonLines(const MphRead::ManagedArray<OpenTK::Mathematics::Vector3>& verts, std::int32_t count); \
    void RenderParticle(const MphRead::RenderItem& item); \
    void RenderTrailSingle(const MphRead::RenderItem& item); \
    void RenderTrailMulti(const MphRead::RenderItem& item); \
    void RenderTrailStack(const MphRead::RenderItem& item); \
    void SetPauseMenuUniforms(); \
    void SetHudLayerUniforms(); \
    void UnsetHudLayerUniforms(); \
    void DrawHudLayer(const std::shared_ptr<MphRead::Hud::LayerInfo>& info); \
    void DoMaterial(const MphRead::RenderItem& item); \
    void DoTexture(const MphRead::RenderItem& item); \
    void SetFlatColor(std::int32_t bindingId); \
    [[nodiscard]] bool ScoreboardOverFreeCamera() const; \
    void OnKeyHeld(); \
    void UpdatePointModule(); \
    void OutputStart(); \
    void OutputStop(); \
    void OutputUpdate(std::stop_token token); \
    void OutputLoadPrompt(); \
    void OutputCameraPrompt(); \
    std::string OutputGetAll(); \
    void OutputGetBotAi(); \
    void OutputGetCollisionMenu(); \
    void OutputGetMenu(); \
    void OutputGetEntityInfo(); \
    void OutputGetModel(); \
    void OutputGetNode(); \
    void OutputGetMesh(); \
    std::string OnOff(bool setting); \
    std::string YesNo(bool setting); \
    [[nodiscard]] bool FilteringOn() const; \
    void FilteringOn(bool value); \
    [[nodiscard]] bool LightingOn() const; \
    void LightingOn(bool value); \
    [[nodiscard]] bool FogOn() const; \
    void FogOn(bool value); \
    OpenTK::Mathematics::Vector2i _rendererSize{}; \
    OpenTK::Mathematics::Matrix4 _viewMatrix = MphRead::RendererDetail::IdentityMatrix(); \
    OpenTK::Mathematics::Matrix4 _viewInvRotMatrix = MphRead::RendererDetail::IdentityMatrix(); \
    OpenTK::Mathematics::Matrix4 _viewInvRotYMatrix = MphRead::RendererDetail::IdentityMatrix(); \
    OpenTK::Mathematics::Matrix4 _perspectiveMatrix = MphRead::RendererDetail::IdentityMatrix(); \
    MphRead::CameraMode _cameraMode = MphRead::CameraMode::Pivot; \
    float _pivotAngleY = 0.0F; \
    float _pivotAngleX = 0.0F; \
    float _pivotDistance = 5.0F; \
    OpenTK::Mathematics::Vector3 _cameraPosition{}; \
    OpenTK::Mathematics::Vector3 _cameraFacing{0.0F, 0.0F, -1.0F}; \
    OpenTK::Mathematics::Vector3 _cameraUp{0.0F, 1.0F, 0.0F}; \
    OpenTK::Mathematics::Vector3 _cameraRight{1.0F, 0.0F, 0.0F}; \
    float _cameraFov = 1.361356816555577F; \
    bool _leftMouse = false; \
    std::int32_t _activeCutscene = -1; \
    OpenTK::Mathematics::Vector3 _priorCameraPos{}; \
    OpenTK::Mathematics::Vector3 _priorCameraFacing{0.0F, 0.0F, -1.0F}; \
    float _priorCameraFov = 1.361356816555577F; \
    std::shared_ptr<MphRead::Formats::Culling::FrustumInfo> _frustumInfo{}; \
    bool _showTextures = true; \
    bool _showColors = true; \
    bool _wireframe = false; \
    std::int32_t _volumeEdges = 0; \
    bool _faceCulling = true; \
    bool _scanVisor = false; \
    std::int32_t _showInvisible = 0; \
    bool _showNodeData = false; \
    MphRead::VolumeDisplay _showVolumes = MphRead::VolumeDisplay::None; \
    std::int32_t _showBotAiSlot = -1; \
    bool _showCollision = false; \
    bool _showAllNodes = false; \
    bool _transformRoomNodes = false; \
    bool _outputCameraPos = false; \
    std::int32_t _textureCount = 0; \
    std::unordered_map<std::int32_t, MphRead::TextureMap> _texPalMap{}; \
    std::int32_t _shaderProgramId = 0; \
    std::int32_t _rttShaderProgramId = 0; \
    std::int32_t _shiftShaderProgramId = 0; \
    std::int32_t _celShaderProgramId = 0; \
    std::shared_ptr<MphRead::ShaderLocations> _shaderLocations{}; \
    OpenTK::Mathematics::Vector3 _light1Vector{}; \
    OpenTK::Mathematics::Vector3 _light1Color{}; \
    OpenTK::Mathematics::Vector3 _light2Vector{}; \
    OpenTK::Mathematics::Vector3 _light2Color{}; \
    bool _hasFog = false; \
    OpenTK::Mathematics::Vector4 _fogColor{}; \
    std::int32_t _fogOffset = 0; \
    std::int32_t _fogSlope = 0; \
    OpenTK::Mathematics::Vector4 _clearColor{0.0F, 0.0F, 0.0F, 1.0F}; \
    const float _nearClip = 0.0625F; \
    float _farClip = 0.0F; \
    bool _useClip = false; \
    float _killHeight = 0.0F; \
    float _frameTime = 0.0F; \
    float _elapsedTime = 0.0F; \
    float _globalElapsedTime = 0.0F; \
    std::uint64_t _frameCount = 0; \
    std::uint64_t _liveFrames = 0; \
    bool _frameAdvanceOn = false; \
    bool _frameAdvanceLastFrame = false; \
    bool _advanceOneFrame = false; \
    bool _recording = false; \
    std::int32_t _framesRecorded = 0; \
    bool _exiting = false; \
    bool _roomLoaded = false; \
    std::shared_ptr<MphRead::Entities::RoomEntity> _room{}; \
    std::int32_t _roomId = -1; \
    std::int32_t _areaId = -1; \
    inline static MphRead::Language _language = MphRead::Language::English; \
    MphRead::RendererPlatform::KeyboardState* _keyboardState = nullptr; \
    MphRead::RendererPlatform::MouseState* _mouseState = nullptr; \
    std::function<void(std::string)> _setTitle{}; \
    std::function<void()> _close{}; \
    std::int32_t _frameBuffer = 0; \
    std::int32_t _screenTexture = 0; \
    std::int32_t _renderBuffer = 0; \
    std::int32_t _celTexture = 0; \
    std::int32_t _depthTexture = 0; \
    bool _depthTextureRefused = false; \
    std::int32_t _celFrameBuffer = 0; \
    std::int32_t _celFrameBufferColor = 0; \
    OpenTK::Mathematics::Vector2i _targetSize{}; \
    std::unordered_map<std::int32_t, OpenTK::Mathematics::Vector3> _flatColors{}; \
    inline static bool _breakNextFrame = false; \
    OpenTK::Graphics::OpenGL::FramebufferErrorCode _framebufferStatus = \
        OpenTK::Graphics::OpenGL::FramebufferErrorCode::FramebufferComplete; \
    inline static bool _saidDepthSize = false; \
    float _claimedQuantum = 5.960464832810452E-8F; \
    float _depthQuantum = 5.960464832810452E-8F; \
    bool _calibrateInk = true; \
    float _framesPerSecond = 0.0F; \
    std::chrono::steady_clock::time_point _fpsClock = std::chrono::steady_clock::now(); \
    std::int32_t _fpsFrames = 0; \
    static constexpr float _almostHalfPi = 1.5707953267948966F; \
    static constexpr std::int32_t _effectEntryMax = 64; \
    static constexpr std::int32_t _effectElementMax = 96; \
    static constexpr std::int32_t _effectParticleMax = 200; \
    static constexpr std::int32_t _singleParticleMax = 200; \
    static constexpr std::int32_t _beamEffectMax = 100; \
    static constexpr std::int32_t _bombMax = 32; \
    std::queue<std::shared_ptr<MphRead::Effects::EffectEntry>> _inactiveEffects{}; \
    std::queue<std::shared_ptr<MphRead::Effects::EffectElementEntry>> _inactiveElements{}; \
    std::vector<std::shared_ptr<MphRead::Effects::EffectElementEntry>> _activeElements{}; \
    std::queue<std::shared_ptr<MphRead::Effects::EffectParticle>> _inactiveParticles{}; \
    std::int32_t _singleParticleCount = 0; \
    std::vector<std::shared_ptr<MphRead::Effects::SingleParticle>> _singleParticles{}; \
    std::queue<std::shared_ptr<MphRead::Entities::BeamEffectEntity>> _inactiveBeamEffects{}; \
    std::vector<std::shared_ptr<MphRead::Entities::BeamEffectEntity>> _activeBeamEffects{}; \
    std::queue<std::shared_ptr<MphRead::Entities::BombEntity>> _inactiveBombs{}; \
    std::vector<std::shared_ptr<MphRead::Entities::BombEntity>> _activeBombs{}; \
    static constexpr std::int32_t _renderItemAlloc = 200; \
    std::queue<std::shared_ptr<MphRead::RenderItem>> _freeRenderItems{}; \
    std::queue<std::shared_ptr<MphRead::RenderItem>> _usedRenderItems{}; \
    std::int32_t _pendingEffectSteps = 0; \
    std::uint64_t _effectFrame = 0; \
    std::int64_t _modEffectParticles = 0; \
    std::int32_t _pendingFadeSteps = 0; \
    std::vector<std::shared_ptr<MphRead::RenderItem>> _decalItems{}; \
    std::vector<std::shared_ptr<MphRead::RenderItem>> _nonDecalItems{}; \
    std::vector<std::shared_ptr<MphRead::RenderItem>> _translucentItems{}; \
    std::array<float, 16> _scaleFactors{}; \
    std::int32_t _nextPolygonId = 1; \
    MphRead::RendererConcurrentQueue<std::shared_ptr<MphRead::Entities::EntityBase>> _loadedEntities{}; \
    bool _initEntities = false; \
    MovieFadeSettings _movieSettings{}; \
    MphRead::FadeType _fadeType = MphRead::FadeType::None; \
    float _fadeColor = 0.0F; \
    bool _fadeIn = false; \
    float _fadeStart = 0.0F; \
    float _fadeLength = 0.0F; \
    float _fadePercent = 0.0F; \
    float _fadeDelay = 0.0F; \
    bool _fadeEnded = false; \
    MphRead::AfterFade _afterFade = MphRead::AfterFade::None; \
    std::shared_ptr<MphRead::Hud::LayerInfo> _layer1Info{}; \
    std::shared_ptr<MphRead::Hud::LayerInfo> _layer2Info{}; \
    std::shared_ptr<MphRead::Hud::LayerInfo> _layer3Info{}; \
    std::shared_ptr<MphRead::Hud::LayerInfo> _layer4Info{}; \
    std::shared_ptr<MphRead::Hud::LayerInfo> _layer5Info{}; \
    std::array<float, 16 * 31> _hudMatrixStack{}; \
    bool _freeCam = false; \
    MphRead::EntityType _colEntDisplay = MphRead::EntityType::Room; \
    MphRead::Terrain _colTerDisplay = MphRead::Terrain::All; \
    MphRead::CollisionType _colTypeDisplay = MphRead::CollisionType::Any; \
    MphRead::CollisionColor _colDisplayColor = MphRead::CollisionColor::None; \
    float _colDisplayAlpha = 0.5F; \
    std::int32_t _colMenuSelect = 0; \
    InputMode _inputMode = InputMode::All; \
    PromptState _promptState = PromptState::None; \
    MphRead::RendererConcurrentQueue<std::tuple<std::string, std::int32_t, bool>> _loadQueue{}; \
    MphRead::RendererConcurrentQueue<std::shared_ptr<MphRead::Entities::EntityBase>> _unloadQueue{}; \
    std::jthread _outputThread{}; \
    std::string _currentOutput{}; \
    std::string _outputBuffer{};

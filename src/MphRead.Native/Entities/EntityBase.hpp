#pragma once

#include "../Formats/Culling.hpp"
#include "../Formats/Enums.hpp"
#include "../Formats/Model.hpp"
#include "../Formats/Types.hpp"
#include "../Sound/Sfx.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace MphRead
{
    class CollisionVolume;
    class Material;
    class MessageInfo;
    class Node;
    class Scene;
    struct DamageResult;

    namespace Formats::Collision
    {
        class CollisionInstance;
        class EntityCollision;
    }
}

namespace MphRead::Entities
{
    class EntityBase
    {
    private:
        class MatrixProperty final
        {
        public:
            explicit MatrixProperty(EntityBase* owner) noexcept;
            MatrixProperty& operator=(::OpenTK::Mathematics::Matrix4 value);
            [[nodiscard]] operator ::OpenTK::Mathematics::Matrix4() const noexcept;
            [[nodiscard]] ::OpenTK::Mathematics::Vector4 Row0() const noexcept;
            [[nodiscard]] ::OpenTK::Mathematics::Vector4 Row1() const noexcept;
            [[nodiscard]] ::OpenTK::Mathematics::Vector4 Row2() const noexcept;
            [[nodiscard]] ::OpenTK::Mathematics::Vector4 Row3() const noexcept;

            const float& M11; const float& M12; const float& M13; const float& M14;
            const float& M21; const float& M22; const float& M23; const float& M24;
            const float& M31; const float& M32; const float& M33; const float& M34;
            const float& M41; const float& M42; const float& M43; const float& M44;

        private:
            EntityBase* _owner;
        };

        enum class VectorPropertyKind : std::uint8_t
        {
            Scale,
            Rotation,
            Position
        };

        class VectorProperty final
        {
        public:
            VectorProperty(EntityBase* owner, VectorPropertyKind kind) noexcept;
            VectorProperty& operator=(::OpenTK::Mathematics::Vector3 value);
            [[nodiscard]] operator ::OpenTK::Mathematics::Vector3() const noexcept;
            [[nodiscard]] ::OpenTK::Mathematics::Vector3 Normalized() const;

            const float& X;
            const float& Y;
            const float& Z;

        private:
            EntityBase* _owner;
            VectorPropertyKind _kind;
        };

    public:
        std::int32_t Id = -1;
        virtual std::int32_t Recolor() const;
        virtual void SetRecolor(std::int32_t value);
        const EntityType Type;
        bool ShouldDraw = true;
        bool Initialized = true;
        bool Active = true;
        bool Hidden = false;
        float Alpha = 1.0F;

        MphRead::Formats::Culling::NodeRef NodeRef = MphRead::Formats::Culling::NodeRef::None;

    protected:
        Scene* _scene;
        const std::optional<std::string> _nodeName{};
        std::int32_t _scanId = 0;

        float _drawScale = 1.0F;
        ::OpenTK::Mathematics::Matrix4 _transform = ::OpenTK::Mathematics::Matrix4(
            ::OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            ::OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            ::OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            ::OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        ::OpenTK::Mathematics::Vector3 _scale{1.0F, 1.0F, 1.0F};
        ::OpenTK::Mathematics::Vector3 _rotation = ::OpenTK::Mathematics::Vector3::Zero;
        ::OpenTK::Mathematics::Vector3 _position = ::OpenTK::Mathematics::Vector3::Zero;

        Node* _colAttachNode = nullptr;

    public:
        MatrixProperty Transform;
        VectorProperty Scale;
        VectorProperty Rotation;
        VectorProperty Position;

        std::array<std::shared_ptr<MphRead::Formats::Collision::EntityCollision>, 2> EntityCollision{};

        EntityBase(const EntityBase&) = delete;
        EntityBase& operator=(const EntityBase&) = delete;
        EntityBase(EntityBase&&) = delete;
        EntityBase& operator=(EntityBase&&) = delete;
        virtual ~EntityBase();

        [[nodiscard]] ::OpenTK::Mathematics::Matrix4 CollisionTransform() const;
        [[nodiscard]] virtual ::OpenTK::Mathematics::Vector3 RightVector() const;
        [[nodiscard]] virtual ::OpenTK::Mathematics::Vector3 UpVector() const;
        [[nodiscard]] virtual ::OpenTK::Mathematics::Vector3 FacingVector() const;

        virtual void Initialize();
        virtual void Destroy();
        virtual void GetPosition(::OpenTK::Mathematics::Vector3& position);
        virtual void GetVectors(::OpenTK::Mathematics::Vector3& position,
            ::OpenTK::Mathematics::Vector3& up, ::OpenTK::Mathematics::Vector3& facing);
        [[nodiscard]] virtual bool GetTargetable();
        [[nodiscard]] virtual std::int32_t GetScanId(bool alternate = false);
        virtual void OnScanned();
        [[nodiscard]] virtual bool Process();
        [[nodiscard]] const std::vector<std::shared_ptr<ModelInstance>>& GetModels() const;
        [[nodiscard]] virtual bool ScanVisible();
        virtual void GetDrawInfo();

        [[nodiscard]] static ::OpenTK::Mathematics::Matrix4 GetTransformMatrix(
            ::OpenTK::Mathematics::Vector3 facing, ::OpenTK::Mathematics::Vector3 up);
        [[nodiscard]] static ::OpenTK::Mathematics::Matrix4 GetTransformMatrix(
            ::OpenTK::Mathematics::Vector3 facing, ::OpenTK::Mathematics::Vector3 up,
            ::OpenTK::Mathematics::Vector3 position);

        virtual void GetDisplayVolumes();
        virtual void SetActive(bool active);
        virtual void SetScanId(std::int32_t scanId);
        [[nodiscard]] virtual EntityBase* GetParent();
        [[nodiscard]] virtual EntityBase* GetChild();
        virtual void HandleMessage(MessageInfo info);
        virtual void CheckContactDamage(DamageResult& result);
        virtual void CheckBeamReflection(bool& result);

    protected:
        class ModelList final
        {
        public:
            [[nodiscard]] std::size_t Size() const noexcept;
            [[nodiscard]] ModelInstance& operator[](std::size_t index);
            [[nodiscard]] const ModelInstance& operator[](std::size_t index) const;
            void Add(std::shared_ptr<ModelInstance> value);
            [[nodiscard]] const std::vector<std::shared_ptr<ModelInstance>>& Items() const noexcept;

        private:
            std::vector<std::shared_ptr<ModelInstance>> _items{};
        };

        explicit EntityBase(EntityType type, Scene* scene);
        EntityBase(EntityType type, std::string nodeName, Scene* scene);
        EntityBase(EntityType type, MphRead::Formats::Culling::NodeRef nodeRef, Scene* scene);

        MphRead::Sound::SoundSource _soundSource{};

        bool _anyLighting = false;
        ModelList _models{};

        [[nodiscard]] virtual bool UseNodeTransform() const;
        [[nodiscard]] virtual std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const;
        [[nodiscard]] virtual std::optional<::OpenTK::Mathematics::Vector4> PaletteOverride() const;
        virtual void SetPaletteOverride(std::optional<::OpenTK::Mathematics::Vector4> value);

        ModelInstance& SetUpModel(std::string name, std::int32_t animIndex = 0,
            AnimFlags animFlags = AnimFlags::None, bool firstHunt = false);
        void SetCollision(MphRead::Formats::Collision::CollisionInstance* collision,
            std::int32_t slot = 0, ModelInstance* attach = nullptr);
        void UpdateCollisionTransform(std::int32_t slot, ::OpenTK::Mathematics::Matrix4 transform);
        void UpdateLinkedInverse(std::int32_t slot);

        [[nodiscard]] virtual ::OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst, std::int32_t index);
        void UpdateAnimFrames(ModelInstance& inst);
        [[nodiscard]] virtual std::int32_t GetModelRecolor(ModelInstance& inst, std::int32_t index);
        void AddPlaceholderModel();
        [[nodiscard]] virtual std::optional<::OpenTK::Mathematics::Vector4> GetOverrideColor(
            ModelInstance& inst, std::int32_t index);
        [[nodiscard]] virtual LightInfo GetLightInfo();
        [[nodiscard]] virtual std::optional<std::int32_t> GetBindingOverride(
            ModelInstance& inst, Material& material, std::int32_t index);
        virtual void UpdateTransforms(ModelInstance& inst, std::int32_t index);
        void UpdateTransforms(ModelInstance& inst, ::OpenTK::Mathematics::Matrix4 transform,
            std::int32_t recolor);
        void UpdateMaterials(ModelInstance& inst, std::int32_t recolor);
        void GetDrawItems(ModelInstance& inst, std::int32_t i,
            std::optional<LightInfo> lightInfo = std::nullopt);
        void UpdateNodeRefVolume();
        [[nodiscard]] bool IsAudible(MphRead::Formats::Culling::NodeRef nodeRef);
        [[nodiscard]] bool IsVisible(MphRead::Formats::Culling::NodeRef nodeRef);
        virtual void GetCollisionDrawInfo();
        [[nodiscard]] virtual ::OpenTK::Mathematics::Vector3 GetEmission(
            ModelInstance& inst, Material& material, std::int32_t index);
        [[nodiscard]] virtual ::OpenTK::Mathematics::Matrix4 GetTexcoordMatrix(
            ModelInstance& inst, Material& material, std::int32_t materialId, Node& node,
            std::int32_t recolor = -1);

        void SetTransform(Vector3Fx facing, Vector3Fx up, Vector3Fx position);
        void SetTransform(::OpenTK::Mathematics::Vector3 facing,
            ::OpenTK::Mathematics::Vector3 up, ::OpenTK::Mathematics::Vector3 position);

        void AddDotItem(::OpenTK::Mathematics::Vector3 position,
            ::OpenTK::Mathematics::Vector3 color);
        void AddVolumeItem(CollisionVolume volume, ::OpenTK::Mathematics::Vector3 color,
            float alpha = 0.5F);
        void AddVectorItem(::OpenTK::Mathematics::Vector3 point,
            ::OpenTK::Mathematics::Vector3 vector, ::OpenTK::Mathematics::Vector3 color);

        [[nodiscard]] std::tuple<float, float> ConstantAcceleration(float step, float velocity,
            float minVelocity = std::numeric_limits<float>::lowest(),
            float maxVelocity = std::numeric_limits<float>::max());
        [[nodiscard]] std::tuple<float, float> Drag(float step, float velocity);
        [[nodiscard]] float ExponentialDecay(float step, float value);

    private:
        std::int32_t _recolor = 0;
        bool _drawColUpdated = true;
        std::optional<::OpenTK::Mathematics::Vector4> _paletteOverride{};

        void SetTransformProperty(::OpenTK::Mathematics::Matrix4 value);
        void SetScaleProperty(::OpenTK::Mathematics::Vector3 value);
        void SetRotationProperty(::OpenTK::Mathematics::Vector3 value);
        void SetPositionProperty(::OpenTK::Mathematics::Vector3 value);
        void SetCollisionMaxAvg(MphRead::Formats::Collision::EntityCollision& entCol);
        void UpdateDrawCollision();
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 GetDiscVertices(float radius,
            std::int32_t index);
    };

    class ModelEntity : public EntityBase
    {
    public:
        ModelEntity(std::shared_ptr<ModelInstance> model, Scene* scene, std::int32_t recolor = 0);
    };
}

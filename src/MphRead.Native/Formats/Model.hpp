#pragma once

#include "Formats.hpp"

#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace System
{
    class DivideByZeroException final : public std::runtime_error
    {
    public:
        DivideByZeroException()
            : std::runtime_error("Attempted to divide by zero.")
        {
        }
    };
}

namespace MphRead
{
    class AnimationResults;
    class Model;
    class Recolor;

    enum class AnimFlags : std::uint16_t
    {
        None = 0x0,
        PingPong = 0x1,
        Reverse = 0x2,
        Paused = 0x4,
        NoLoop = 0x8,
        Ended = 0x10
    };

    [[nodiscard]] constexpr AnimFlags operator|(AnimFlags left, AnimFlags right) noexcept
    {
        return static_cast<AnimFlags>(
            static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr AnimFlags operator&(AnimFlags left, AnimFlags right) noexcept
    {
        return static_cast<AnimFlags>(
            static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr AnimFlags operator^(AnimFlags left, AnimFlags right) noexcept
    {
        return static_cast<AnimFlags>(
            static_cast<std::uint16_t>(left) ^ static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr AnimFlags operator~(AnimFlags value) noexcept
    {
        return static_cast<AnimFlags>(
            static_cast<std::uint16_t>(
                ~static_cast<std::uint16_t>(value)));
    }

    constexpr AnimFlags& operator|=(AnimFlags& left, AnimFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr AnimFlags& operator&=(AnimFlags& left, AnimFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr AnimFlags& operator^=(AnimFlags& left, AnimFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    enum class SetFlags : std::uint16_t
    {
        None = 0x0,
        Node = 0x2,
        Unused = 0x4,
        Material = 0x8,
        Texcoord = 0x10,
        Texture = 0x20,
        All = 0x3E
    };

    [[nodiscard]] constexpr SetFlags operator|(SetFlags left, SetFlags right) noexcept
    {
        return static_cast<SetFlags>(
            static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr SetFlags operator&(SetFlags left, SetFlags right) noexcept
    {
        return static_cast<SetFlags>(
            static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr SetFlags operator^(SetFlags left, SetFlags right) noexcept
    {
        return static_cast<SetFlags>(
            static_cast<std::uint16_t>(left) ^ static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr SetFlags operator~(SetFlags value) noexcept
    {
        return static_cast<SetFlags>(
            static_cast<std::uint16_t>(
                ~static_cast<std::uint16_t>(value)));
    }

    constexpr SetFlags& operator|=(SetFlags& left, SetFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr SetFlags& operator&=(SetFlags& left, SetFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr SetFlags& operator^=(SetFlags& left, SetFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class NodeAnimationInfo
    {
    public:
        std::int32_t Slot = 0;
        std::shared_ptr<NodeAnimationGroup> Group{};

        NodeAnimationInfo() = default;
        NodeAnimationInfo(const NodeAnimationInfo&) = delete;
        NodeAnimationInfo& operator=(const NodeAnimationInfo&) = delete;
        NodeAnimationInfo(NodeAnimationInfo&&) = delete;
        NodeAnimationInfo& operator=(NodeAnimationInfo&&) = delete;
    };

    class MaterialAnimationInfo
    {
    public:
        std::int32_t Slot = 0;
        std::shared_ptr<MaterialAnimationGroup> Group{};

        MaterialAnimationInfo() = default;
        MaterialAnimationInfo(const MaterialAnimationInfo&) = delete;
        MaterialAnimationInfo& operator=(const MaterialAnimationInfo&) = delete;
        MaterialAnimationInfo(MaterialAnimationInfo&&) = delete;
        MaterialAnimationInfo& operator=(MaterialAnimationInfo&&) = delete;
    };

    class TexcoordAnimationInfo
    {
    public:
        std::int32_t Slot = 0;
        std::shared_ptr<TexcoordAnimationGroup> Group{};

        TexcoordAnimationInfo() = default;
        TexcoordAnimationInfo(const TexcoordAnimationInfo&) = delete;
        TexcoordAnimationInfo& operator=(const TexcoordAnimationInfo&) = delete;
        TexcoordAnimationInfo(TexcoordAnimationInfo&&) = delete;
        TexcoordAnimationInfo& operator=(TexcoordAnimationInfo&&) = delete;
    };

    class TextureAnimationInfo
    {
    public:
        std::int32_t Slot = 0;
        std::shared_ptr<TextureAnimationGroup> Group{};

        TextureAnimationInfo() = default;
        TextureAnimationInfo(const TextureAnimationInfo&) = delete;
        TextureAnimationInfo& operator=(const TextureAnimationInfo&) = delete;
        TextureAnimationInfo(TextureAnimationInfo&&) = delete;
        TextureAnimationInfo& operator=(TextureAnimationInfo&&) = delete;
    };

    class AnimationInfo
    {
    public:
        const std::shared_ptr<ManagedArray<std::int32_t>> Index;
        const std::shared_ptr<ManagedArray<std::int32_t>> PrevIndex;
        const std::shared_ptr<ManagedArray<std::int32_t>> Frame;
        const std::shared_ptr<ManagedArray<std::int32_t>> FrameCount;
        const std::shared_ptr<ManagedArray<AnimFlags>> Flags;
        const std::shared_ptr<ManagedArray<std::int32_t>> Step;
        const std::shared_ptr<NodeAnimationInfo> Node;
        const std::shared_ptr<MaterialAnimationInfo> Material;
        const std::shared_ptr<TexcoordAnimationInfo> Texcoord;
        const std::shared_ptr<TextureAnimationInfo> Texture;

        AnimationInfo();
        AnimationInfo(const AnimationInfo&) = delete;
        AnimationInfo& operator=(const AnimationInfo&) = delete;
        AnimationInfo(AnimationInfo&&) = delete;
        AnimationInfo& operator=(AnimationInfo&&) = delete;

        [[nodiscard]] std::int32_t NodeFrame() const;
        [[nodiscard]] std::int32_t MaterialFrame() const;
        [[nodiscard]] std::int32_t TextureFrame() const;
        [[nodiscard]] std::int32_t TexcoordFrame() const;
        [[nodiscard]] std::int32_t NodeIndex() const;
        [[nodiscard]] std::int32_t MaterialIndex() const;
        [[nodiscard]] std::int32_t TextureIndex() const;
        [[nodiscard]] std::int32_t TexcoordIndex() const;
    };

    class AnimationOffsets
    {
    public:
        const std::shared_ptr<const std::vector<std::uint32_t>> Node;
        const std::shared_ptr<const std::vector<std::uint32_t>> Material;
        const std::shared_ptr<const std::vector<std::uint32_t>> Texcoord;
        const std::shared_ptr<const std::vector<std::uint32_t>> Texture;

        template <typename TAnimationResults>
        requires std::derived_from<std::remove_cv_t<TAnimationResults>, AnimationResults>
        explicit AnimationOffsets(const std::shared_ptr<TAnimationResults>& animations)
            : AnimationOffsets(BuildInit(animations))
        {
        }

        AnimationOffsets(const AnimationOffsets&) = delete;
        AnimationOffsets& operator=(const AnimationOffsets&) = delete;
        AnimationOffsets(AnimationOffsets&&) = delete;
        AnimationOffsets& operator=(AnimationOffsets&&) = delete;

    private:
        struct Init
        {
            std::shared_ptr<const std::vector<std::uint32_t>> Node;
            std::shared_ptr<const std::vector<std::uint32_t>> Material;
            std::shared_ptr<const std::vector<std::uint32_t>> Texcoord;
            std::shared_ptr<const std::vector<std::uint32_t>> Texture;
        };

        explicit AnimationOffsets(Init init);

        template <typename TAnimationResults>
        requires std::derived_from<std::remove_cv_t<TAnimationResults>, AnimationResults>
        [[nodiscard]] static Init BuildInit(
            const std::shared_ptr<TAnimationResults>& animations)
        {
            if (!animations)
            {
                throw System::NullReferenceException();
            }
            return Init{
                animations->NodeGroupOffsets,
                animations->MaterialGroupOffsets,
                animations->TexcoordGroupOffsets,
                animations->TextureGroupOffsets};
        }
    };

    class AnimationGroups
    {
    public:
        const bool Any;
        const std::shared_ptr<const std::vector<std::shared_ptr<NodeAnimationGroup>>> Node;
        const std::shared_ptr<const std::vector<std::shared_ptr<MaterialAnimationGroup>>> Material;
        const std::shared_ptr<const std::vector<std::shared_ptr<TexcoordAnimationGroup>>> Texcoord;
        const std::shared_ptr<const std::vector<std::shared_ptr<TextureAnimationGroup>>> Texture;
        const std::shared_ptr<AnimationOffsets> Offsets;

        template <typename TAnimationResults>
        requires std::derived_from<std::remove_cv_t<TAnimationResults>, AnimationResults>
        explicit AnimationGroups(const std::shared_ptr<TAnimationResults>& animations)
            : AnimationGroups(BuildInit(animations))
        {
        }

        AnimationGroups(const AnimationGroups&) = delete;
        AnimationGroups& operator=(const AnimationGroups&) = delete;
        AnimationGroups(AnimationGroups&&) = delete;
        AnimationGroups& operator=(AnimationGroups&&) = delete;

    private:
        struct Init
        {
            bool Any;
            std::shared_ptr<const std::vector<std::shared_ptr<NodeAnimationGroup>>> Node;
            std::shared_ptr<const std::vector<std::shared_ptr<MaterialAnimationGroup>>> Material;
            std::shared_ptr<const std::vector<std::shared_ptr<TexcoordAnimationGroup>>> Texcoord;
            std::shared_ptr<const std::vector<std::shared_ptr<TextureAnimationGroup>>> Texture;
            std::shared_ptr<AnimationOffsets> Offsets;
        };

        explicit AnimationGroups(Init init);

        template <typename T>
        [[nodiscard]] static const T& Require(const std::shared_ptr<const T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        template <typename TAnimationResults>
        requires std::derived_from<std::remove_cv_t<TAnimationResults>, AnimationResults>
        [[nodiscard]] static Init BuildInit(
            const std::shared_ptr<TAnimationResults>& animations)
        {
            if (!animations)
            {
                throw System::NullReferenceException();
            }

            std::shared_ptr<const std::vector<std::shared_ptr<NodeAnimationGroup>>> node
                = animations->NodeAnimationGroups;
            std::shared_ptr<const std::vector<std::shared_ptr<MaterialAnimationGroup>>> material
                = animations->MaterialAnimationGroups;
            std::shared_ptr<const std::vector<std::shared_ptr<TexcoordAnimationGroup>>> texcoord
                = animations->TexcoordAnimationGroups;
            std::shared_ptr<const std::vector<std::shared_ptr<TextureAnimationGroup>>> texture
                = animations->TextureAnimationGroups;

            const bool any = !Require(node).empty()
                || !Require(material).empty()
                || !Require(texcoord).empty()
                || !Require(texture).empty();

            auto offsets = std::make_shared<AnimationOffsets>(animations);
#ifndef NDEBUG
            assert(Require(offsets->Node).size() >= Require(node).size());
            assert(Require(offsets->Material).size() >= Require(material).size());
            assert(Require(offsets->Texcoord).size() >= Require(texcoord).size());
            assert(Require(offsets->Texture).size() >= Require(texture).size());
#endif

            return Init{
                any,
                std::move(node),
                std::move(material),
                std::move(texcoord),
                std::move(texture),
                std::move(offsets)};
        }
    };

    class ModelInstance
    {
    private:
        std::shared_ptr<MphRead::Model> _model;

    public:
        const std::shared_ptr<AnimationInfo> AnimInfo;
        bool IsPlaceholder = false;
        bool Active = true;
        bool NodeAnimIgnoreRoot = false;

        explicit ModelInstance(std::shared_ptr<MphRead::Model> model);
        [[nodiscard]] std::shared_ptr<MphRead::Model> Model() const noexcept;
        ModelInstance(const ModelInstance&) = delete;
        ModelInstance& operator=(const ModelInstance&) = delete;
        ModelInstance(ModelInstance&&) = delete;
        ModelInstance& operator=(ModelInstance&&) = delete;

        void SetModel(std::shared_ptr<MphRead::Model> model);
        void SetAnimation(
            std::int32_t index,
            AnimFlags animFlags = AnimFlags::None);
        void SetAnimation(
            std::int32_t index,
            std::int32_t slot,
            SetFlags setFlags,
            AnimFlags animFlags = AnimFlags::None);
        void UpdateAnimFrames();
        void SetNodeAnim(std::int32_t index);
        void SetMaterialAnim(std::int32_t index);

    private:
        void UpdateAnimFrames(std::int32_t slot);
    };

    class Model
    {
    public:
        const std::int32_t Id;
        const std::string Name;
        const bool FirstHunt;
        const MphRead::Header Header;
        const std::shared_ptr<const std::vector<std::shared_ptr<Node>>> Nodes;
        const std::shared_ptr<const std::vector<std::shared_ptr<Mesh>>> Meshes;
        const std::shared_ptr<const std::vector<std::shared_ptr<Material>>> Materials;
        const std::shared_ptr<const std::vector<DisplayList>> DisplayLists;
        const std::shared_ptr<const std::vector<OpenTK::Mathematics::Matrix4>> TextureMatrices;
        const std::shared_ptr<const std::vector<
            std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>> RenderInstructionLists;
        const std::shared_ptr<const std::vector<std::shared_ptr<Recolor>>> Recolors;
        const std::shared_ptr<const std::vector<std::int32_t>> NodeMatrixIds;
        const std::shared_ptr<const ManagedArray<float>> MatrixStackValues;
        const std::shared_ptr<MphRead::AnimationGroups> AnimationGroups;

        const std::shared_ptr<const std::vector<RawNode>> RawNodes;
        const std::shared_ptr<const std::vector<Vector3Fx>> NodePos;
        const std::shared_ptr<const std::vector<Vector3Fx>> NodeInitPos;
        const std::shared_ptr<const std::vector<std::int32_t>> NodePosCounts;
        const std::shared_ptr<const std::vector<Fixed>> NodePosScales;

        const OpenTK::Mathematics::Vector3 Scale;

        template <
            typename TNodeEnumerable,
            typename TMeshEnumerable,
            typename TMaterialEnumerable,
            typename TAnimationResults>
        requires std::derived_from<std::remove_cv_t<TAnimationResults>, AnimationResults>
        Model(
            std::string name,
            bool firstHunt,
            MphRead::Header header,
            std::shared_ptr<TNodeEnumerable> nodes,
            std::shared_ptr<TMeshEnumerable> meshes,
            std::shared_ptr<TMaterialEnumerable> materials,
            std::shared_ptr<const std::vector<DisplayList>> dlists,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>> renderInstructions,
            const std::shared_ptr<TAnimationResults>& animations,
            std::shared_ptr<const std::vector<OpenTK::Mathematics::Matrix4>> textureMatrices,
            std::shared_ptr<const std::vector<std::shared_ptr<Recolor>>> recolors,
            std::shared_ptr<const std::vector<std::int32_t>> nodeWeights,
            std::shared_ptr<const std::vector<Vector3Fx>> nodePos,
            std::shared_ptr<const std::vector<Vector3Fx>> nodeInitPos,
            std::shared_ptr<const std::vector<std::int32_t>> posCounts,
            std::shared_ptr<const std::vector<Fixed>> posScales)
            : Model(BuildInit(
                std::move(name),
                firstHunt,
                header,
                std::move(nodes),
                std::move(meshes),
                std::move(materials),
                std::move(dlists),
                std::move(renderInstructions),
                animations,
                std::move(textureMatrices),
                std::move(recolors),
                std::move(nodeWeights),
                std::move(nodePos),
                std::move(nodeInitPos),
                std::move(posCounts),
                std::move(posScales)))
        {
        }

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = delete;
        Model& operator=(Model&&) = delete;

        void FilterNodes(std::int32_t layerMask);
        void ComputeNodeMatrices(std::int32_t index);
        void AnimateNodes(
            std::int32_t index,
            bool useNodeTransform,
            OpenTK::Mathematics::Matrix4 parentTansform,
            OpenTK::Mathematics::Vector3 scale,
            const std::shared_ptr<AnimationInfo>& info);
        void AnimateNodes2(
            std::int32_t index,
            bool useNodeTransform,
            OpenTK::Mathematics::Matrix4 parentTansform,
            OpenTK::Mathematics::Vector3 scale,
            const std::shared_ptr<AnimationInfo>& info);
        void AnimateMaterials(const std::shared_ptr<AnimationInfo>& info);
        [[nodiscard]] OpenTK::Mathematics::Matrix4 AnimateTexcoords(
            const std::shared_ptr<TexcoordAnimationGroup>& group,
            TexcoordAnimation animation,
            std::int32_t currentFrame);
        void AnimateTextures(const std::shared_ptr<AnimationInfo>& info);
        [[nodiscard]] float InterpolateAnimation(
            const std::shared_ptr<const std::vector<float>>& values,
            std::int32_t start,
            std::int32_t frame,
            std::int32_t blend,
            std::int32_t lutLength,
            std::int32_t frameCount,
            bool isRotation = false) const;
        void UpdateMatrixStack();
        [[nodiscard]] bool NodeParentsEnabled(const std::shared_ptr<Node>& node) const;
        [[nodiscard]] std::shared_ptr<Node> GetNodeByName(const std::string& name) const;
        [[nodiscard]] std::int32_t GetNodeIndexByName(const std::string& name) const;
        [[nodiscard]] std::shared_ptr<Material> GetMaterialByName(const std::string& name) const;
        [[nodiscard]] std::vector<ColorRgba> GetPixels(
            std::int32_t textureId,
            std::int32_t paletteId,
            std::int32_t recolorId) const;

    private:
        static std::int32_t _nextId;
        std::shared_ptr<ManagedArray<float>> _matrixStackValues;

        struct Init
        {
            std::int32_t Id;
            std::string Name;
            bool FirstHunt;
            MphRead::Header Header;
            std::shared_ptr<const std::vector<std::shared_ptr<Node>>> Nodes;
            std::shared_ptr<const std::vector<std::shared_ptr<Mesh>>> Meshes;
            std::shared_ptr<const std::vector<std::shared_ptr<Material>>> Materials;
            std::shared_ptr<const std::vector<DisplayList>> DisplayLists;
            std::shared_ptr<const std::vector<OpenTK::Mathematics::Matrix4>> TextureMatrices;
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>> RenderInstructionLists;
            std::shared_ptr<const std::vector<std::shared_ptr<Recolor>>> Recolors;
            std::shared_ptr<const std::vector<std::int32_t>> NodeMatrixIds;
            std::shared_ptr<ManagedArray<float>> MatrixStackValues;
            std::shared_ptr<MphRead::AnimationGroups> AnimationGroups;
            std::shared_ptr<const std::vector<RawNode>> RawNodes;
            std::shared_ptr<const std::vector<Vector3Fx>> NodePos;
            std::shared_ptr<const std::vector<Vector3Fx>> NodeInitPos;
            std::shared_ptr<const std::vector<std::int32_t>> NodePosCounts;
            std::shared_ptr<const std::vector<Fixed>> NodePosScales;
            OpenTK::Mathematics::Vector3 Scale;
        };

        explicit Model(Init init);

        [[nodiscard]] static std::int32_t NextId() noexcept;
        void SetMatrixStackValues(
            std::int32_t index,
            OpenTK::Mathematics::Matrix4 matrix);
        [[nodiscard]] OpenTK::Mathematics::Matrix4 ComputeNodeTransforms(
            OpenTK::Mathematics::Vector3 scale,
            OpenTK::Mathematics::Vector3 angle,
            OpenTK::Mathematics::Vector3 position) const;
        [[nodiscard]] OpenTK::Mathematics::Matrix4 AnimateNode(
            const std::shared_ptr<NodeAnimationGroup>& group,
            NodeAnimation animation,
            OpenTK::Mathematics::Vector3 modelScale,
            std::int32_t currentFrame) const;

        template <
            typename TNodeEnumerable,
            typename TMeshEnumerable,
            typename TMaterialEnumerable,
            typename TAnimationResults>
        requires std::derived_from<std::remove_cv_t<TAnimationResults>, AnimationResults>
        [[nodiscard]] static Init BuildInit(
            std::string name,
            bool firstHunt,
            MphRead::Header header,
            std::shared_ptr<TNodeEnumerable> nodes,
            std::shared_ptr<TMeshEnumerable> meshes,
            std::shared_ptr<TMaterialEnumerable> materials,
            std::shared_ptr<const std::vector<DisplayList>> dlists,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>> renderInstructions,
            const std::shared_ptr<TAnimationResults>& animations,
            std::shared_ptr<const std::vector<OpenTK::Mathematics::Matrix4>> textureMatrices,
            std::shared_ptr<const std::vector<std::shared_ptr<Recolor>>> recolors,
            std::shared_ptr<const std::vector<std::int32_t>> nodeWeights,
            std::shared_ptr<const std::vector<Vector3Fx>> nodePos,
            std::shared_ptr<const std::vector<Vector3Fx>> nodeInitPos,
            std::shared_ptr<const std::vector<std::int32_t>> posCounts,
            std::shared_ptr<const std::vector<Fixed>> posScales)
        {
            // Id is a C# instance field initializer and therefore executes
            // before any constructor-body assignment or validation.
            const std::int32_t id = NextId();

            if (!nodes)
            {
                throw System::ArgumentNullException("source");
            }
            auto nativeNodes = std::make_shared<std::vector<std::shared_ptr<Node>>>();
            for (const auto& raw : *nodes)
            {
                nativeNodes->push_back(std::make_shared<Node>(RawNode(raw)));
            }

            // Model.cs enumerates the node IEnumerable a second time for RawNodes.
            auto rawNodes = std::make_shared<std::vector<RawNode>>();
            for (const auto& raw : *nodes)
            {
                rawNodes->push_back(RawNode(raw));
            }

            if (!meshes)
            {
                throw System::ArgumentNullException("source");
            }
            auto nativeMeshes = std::make_shared<std::vector<std::shared_ptr<Mesh>>>();
            for (const auto& raw : *meshes)
            {
                nativeMeshes->push_back(std::make_shared<Mesh>(RawMesh(raw)));
            }

            if (!materials)
            {
                throw System::ArgumentNullException("source");
            }
            auto nativeMaterials = std::make_shared<std::vector<std::shared_ptr<Material>>>();
            for (const auto& raw : *materials)
            {
                nativeMaterials->push_back(std::make_shared<Material>(RawMaterial(raw)));
            }

#ifndef NDEBUG
            if (!nodeWeights)
            {
                throw System::NullReferenceException();
            }
            assert(
                static_cast<std::size_t>(header.NodeWeightCount) == nodeWeights->size()
                || name == "doubleDamage_img");
            assert(nodeWeights->size() <= 31);
#endif

            std::shared_ptr<ManagedArray<float>> matrixStackValues;
            if (header.NodeWeightCount > 0)
            {
                matrixStackValues = std::make_shared<ManagedArray<float>>(
                    static_cast<std::size_t>(header.NodeWeightCount) * 16U);
                const OpenTK::Mathematics::Matrix4 identity(
                    OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
                    OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
                    OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
                    OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F));
                for (std::int32_t i = 0;
                    i < static_cast<std::int32_t>(header.NodeWeightCount);
                    ++i)
                {
                    const std::size_t offset = static_cast<std::size_t>(i) * 16U;
                    (*matrixStackValues)[offset] = identity.M11;
                    (*matrixStackValues)[offset + 1U] = identity.M12;
                    (*matrixStackValues)[offset + 2U] = identity.M13;
                    (*matrixStackValues)[offset + 3U] = identity.M14;
                    (*matrixStackValues)[offset + 4U] = identity.M21;
                    (*matrixStackValues)[offset + 5U] = identity.M22;
                    (*matrixStackValues)[offset + 6U] = identity.M23;
                    (*matrixStackValues)[offset + 7U] = identity.M24;
                    (*matrixStackValues)[offset + 8U] = identity.M31;
                    (*matrixStackValues)[offset + 9U] = identity.M32;
                    (*matrixStackValues)[offset + 10U] = identity.M33;
                    (*matrixStackValues)[offset + 11U] = identity.M34;
                    (*matrixStackValues)[offset + 12U] = identity.M41;
                    (*matrixStackValues)[offset + 13U] = identity.M42;
                    (*matrixStackValues)[offset + 14U] = identity.M43;
                    (*matrixStackValues)[offset + 15U] = identity.M44;
                }
            }
            else
            {
                matrixStackValues = ManagedArray<float>::Empty();
            }

            auto animationGroups = std::make_shared<MphRead::AnimationGroups>(animations);

            const std::uint32_t shift = header.ScaleFactor & 31U;
            const std::int32_t scaleFactor = std::bit_cast<std::int32_t>(1U << shift);
            const float scale = header.ScaleBase.FloatValue()
                * static_cast<float>(scaleFactor);

            return Init{
                id,
                std::move(name),
                firstHunt,
                header,
                std::move(nativeNodes),
                std::move(nativeMeshes),
                std::move(nativeMaterials),
                std::move(dlists),
                std::move(textureMatrices),
                std::move(renderInstructions),
                std::move(recolors),
                std::move(nodeWeights),
                std::move(matrixStackValues),
                std::move(animationGroups),
                std::move(rawNodes),
                std::move(nodePos),
                std::move(nodeInitPos),
                std::move(posCounts),
                std::move(posScales),
                OpenTK::Mathematics::Vector3(scale, scale, scale)};
        }
    };

    class Recolor
    {
    public:
        const std::string Name;
        const std::shared_ptr<const std::vector<Texture>> Textures;
        const std::shared_ptr<const std::vector<Palette>> Palettes;
        const std::shared_ptr<const std::vector<
            std::shared_ptr<const std::vector<MphRead::TextureData>>>> TextureData;
        const std::shared_ptr<const std::vector<
            std::shared_ptr<const std::vector<MphRead::PaletteData>>>> PaletteData;

        Recolor(
            std::string name,
            std::shared_ptr<const std::vector<Texture>> textures,
            std::shared_ptr<const std::vector<Palette>> palettes,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<MphRead::TextureData>>>> textureData,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<MphRead::PaletteData>>>> paletteData);
        Recolor(const Recolor&) = delete;
        Recolor& operator=(const Recolor&) = delete;
        Recolor(Recolor&&) = delete;
        Recolor& operator=(Recolor&&) = delete;

        [[nodiscard]] std::vector<ColorRgba> GetPixels(
            std::int32_t textureId,
            std::int32_t palettteId) const;
        [[nodiscard]] std::vector<ColorRgba> GetPalettePixels(
            std::int32_t palettteId) const;

    private:
        struct Init
        {
            std::string Name;
            std::shared_ptr<const std::vector<Texture>> Textures;
            std::shared_ptr<const std::vector<Palette>> Palettes;
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<MphRead::TextureData>>>> TextureData;
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<MphRead::PaletteData>>>> PaletteData;
        };

        explicit Recolor(Init init);
        [[nodiscard]] static Init BuildInit(
            std::string name,
            std::shared_ptr<const std::vector<Texture>> textures,
            std::shared_ptr<const std::vector<Palette>> palettes,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<MphRead::TextureData>>>> textureData,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<MphRead::PaletteData>>>> paletteData);

        [[nodiscard]] ColorRgba ColorFromShort(
            std::uint32_t value,
            std::uint8_t alpha) const noexcept;
        static void ThrowIfInvalidEnums(
            const std::shared_ptr<const std::vector<Texture>>& textures);
    };
}

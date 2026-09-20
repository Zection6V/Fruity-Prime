#include "Selection.hpp"

#include "Entities/EntityBase.hpp"
#include "Formats/Model.hpp"
#include "Mods/Chat/ChatBox.hpp"
#include "Scene.hpp"

#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

namespace MphRead
{
    namespace
    {
        using Keys = ::OpenTK::Windowing::GraphicsLibraryFramework::Keys;

        // PlayerInput.hpp owns the native OpenTK Keys provider but does not yet list
        // these additional OpenTK/GLFW values observed by Selection.cs.
        constexpr Keys KeyMinus = static_cast<Keys>(45);
        constexpr Keys KeyEqual = static_cast<Keys>(61);
        constexpr Keys KeyPad0 = static_cast<Keys>(320);
        constexpr Keys KeyPad1 = static_cast<Keys>(321);
        constexpr Keys KeyPad2 = static_cast<Keys>(322);
        constexpr Keys KeyPadSubtract = static_cast<Keys>(333);
        constexpr Keys KeyPadEqual = static_cast<Keys>(336);

        constexpr float TwoPi = std::numbers::pi_v<float> * 2.0F;

        constexpr ::OpenTK::Mathematics::Vector3 EntityColor{1.0F, 1.0F, 1.0F};
        constexpr ::OpenTK::Mathematics::Vector3 ModelColor{
            255.0F / 255.0F, 255.0F / 255.0F, 200.0F / 255.0F};
        constexpr ::OpenTK::Mathematics::Vector3 NodeColor{
            200.0F / 255.0F, 255.0F / 255.0F, 200.0F / 255.0F};
        constexpr ::OpenTK::Mathematics::Vector3 MeshColor{
            255.0F / 255.0F, 200.0F / 255.0F, 255.0F / 255.0F};
        constexpr ::OpenTK::Mathematics::Vector3 ParentColor{1.0F, 0.0F, 0.0F};
        constexpr ::OpenTK::Mathematics::Vector3 ChildColor{0.0F, 0.0F, 1.0F};

        [[nodiscard]] ::OpenTK::Mathematics::Vector4 ScaleColor(
            ::OpenTK::Mathematics::Vector3 color, float factor) noexcept
        {
            return ::OpenTK::Mathematics::Vector4(
                color.X * factor,
                color.Y * factor,
                color.Z * factor,
                1.0F);
        }

        [[nodiscard]] std::int64_t TickCount64() noexcept
        {
#if defined(_WIN32)
            return static_cast<std::int64_t>(::GetTickCount64());
#elif defined(CLOCK_MONOTONIC)
            timespec value{};
            if (::clock_gettime(CLOCK_MONOTONIC, &value) == 0)
            {
                return static_cast<std::int64_t>(value.tv_sec) * 1000
                    + static_cast<std::int64_t>(value.tv_nsec / 1000000);
            }
#endif
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        [[nodiscard]] float GetFactor() noexcept
        {
            const std::int64_t ms = TickCount64();
            float percentage = static_cast<float>(ms % 1000) / 1000.0F;
            if (ms / 1000 % 10 % 2 == 0)
            {
                percentage = 1.0F - percentage;
            }
            return percentage;
        }

        [[nodiscard]] constexpr std::int32_t UncheckedAdd(
            std::int32_t value, std::int32_t delta) noexcept
        {
            const std::uint32_t sum = static_cast<std::uint32_t>(value)
                + static_cast<std::uint32_t>(delta);
            return std::bit_cast<std::int32_t>(sum);
        }

        template <typename T>
        [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        template <typename T>
        [[nodiscard]] std::int32_t IndexOfIdentity(
            const std::vector<std::shared_ptr<T>>& values,
            const std::shared_ptr<T>& value) noexcept
        {
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (values[i] == value)
                {
                    return static_cast<std::int32_t>(i);
                }
            }
            return -1;
        }

        template <typename T>
        [[nodiscard]] std::shared_ptr<T> AtNativeCollection(
            const std::vector<std::shared_ptr<T>>& values,
            std::int32_t index)
        {
            return values.at(static_cast<std::size_t>(index));
        }
    }

    std::shared_ptr<LinkedListNode<Entities::EntityBase>> Selection::_entityNode{};
    std::shared_ptr<ModelInstance> Selection::_instance{};
    std::shared_ptr<::MphRead::Node> Selection::_node{};
    std::shared_ptr<::MphRead::Mesh> Selection::_mesh{};
    bool Selection::_showSelection = true;
    bool Selection::_hideUnselectedVolumes = false;
    std::vector<std::shared_ptr<::MphRead::Mesh>> Selection::_meshBuffer{};

    std::shared_ptr<LinkedListNode<Entities::EntityBase>> Selection::EntityNode() noexcept
    {
        return _entityNode;
    }

    std::shared_ptr<Entities::EntityBase> Selection::Entity() noexcept
    {
        return _entityNode ? _entityNode->Value() : nullptr;
    }

    std::shared_ptr<ModelInstance> Selection::Instance() noexcept
    {
        return _instance;
    }

    std::shared_ptr<::MphRead::Node> Selection::Node() noexcept
    {
        return _node;
    }

    std::shared_ptr<::MphRead::Mesh> Selection::Mesh() noexcept
    {
        return _mesh;
    }

    bool Selection::Any() noexcept
    {
        return _mesh != nullptr || _node != nullptr || _instance != nullptr || Entity() != nullptr;
    }

    bool Selection::CheckVolume(const std::shared_ptr<Entities::EntityBase>& entity)
    {
        return CheckVolume(entity.get());
    }

    bool Selection::CheckVolume(const Entities::EntityBase* entity)
    {
        return !_hideUnselectedVolumes || Entity() == nullptr || entity == Entity().get();
    }

    void Selection::Clear() noexcept
    {
        _mesh.reset();
        _node.reset();
        _instance.reset();
        _entityNode.reset();
    }

    SelectionType Selection::CheckSelection(
        const std::shared_ptr<Entities::EntityBase>& entity,
        const std::shared_ptr<ModelInstance>& inst,
        const std::shared_ptr<::MphRead::Node>& node,
        const std::shared_ptr<::MphRead::Mesh>& mesh)
    {
        return CheckSelectionIdentity(entity.get(), inst.get(), node.get(), mesh.get());
    }

    SelectionType Selection::CheckSelection(
        const Entities::EntityBase* entity,
        const ModelInstance& inst,
        const ::MphRead::Node& node,
        const ::MphRead::Mesh& mesh)
    {
        return CheckSelectionIdentity(entity, &inst, &node, &mesh);
    }

    SelectionType Selection::CheckSelectionIdentity(
        const Entities::EntityBase* entity,
        const ModelInstance* inst,
        const ::MphRead::Node* node,
        const ::MphRead::Mesh* mesh)
    {
        if (_mesh != nullptr)
        {
            if (mesh == _mesh.get() && node == _node.get() && inst == _instance.get()
                && entity == Entity().get())
            {
                return SelectionType::Selected;
            }
        }
        else if (_node != nullptr)
        {
            if (node == _node.get() && inst == _instance.get() && entity == Entity().get())
            {
                return SelectionType::Selected;
            }
        }
        else if (_instance != nullptr)
        {
            if (inst == _instance.get() && entity == Entity().get())
            {
                return SelectionType::Selected;
            }
        }
        else if (Entity() != nullptr)
        {
            if (entity == Entity().get())
            {
                return SelectionType::Selected;
            }
        }
        if (Entity() != nullptr)
        {
            if (Entity()->GetParent() == entity)
            {
                return SelectionType::Parent;
            }
            if (Entity()->GetChild() == entity)
            {
                return SelectionType::Child;
            }
        }
        return SelectionType::None;
    }

    void Selection::ToggleShowSelection() noexcept
    {
        _showSelection = !_showSelection;
    }

    void Selection::ToggleUnselectedVolumes() noexcept
    {
        _hideUnselectedVolumes = !_hideUnselectedVolumes;
    }

    std::optional<::OpenTK::Mathematics::Vector4>
        Selection::GetSelectionColor(SelectionType type)
    {
        if (_showSelection)
        {
            if (type == SelectionType::Selected)
            {
                const float factor = GetFactor();
                if (_mesh != nullptr)
                {
                    return ScaleColor(MeshColor, factor);
                }
                if (_node != nullptr)
                {
                    return ScaleColor(NodeColor, factor);
                }
                if (_instance != nullptr)
                {
                    return ScaleColor(ModelColor, factor);
                }
                return ScaleColor(EntityColor, factor);
            }
            if (type == SelectionType::Parent)
            {
                const float factor = GetFactor();
                return ScaleColor(ParentColor, factor);
            }
            if (type == SelectionType::Child)
            {
                const float factor = GetFactor();
                return ScaleColor(ChildColor, factor);
            }
        }
        return std::nullopt;
    }

    bool Selection::OnKeyDown(
        const ::OpenTK::Windowing::Common::KeyboardKeyEventArgs& e,
        Scene& scene)
    {
        if (e.Key == Keys::M)
        {
            UpdateSelection(e.Control, e.Shift, scene);
            return true;
        }
        if (!Any())
        {
            return false;
        }
        if (e.Key == KeyEqual || e.Key == KeyPadEqual)
        {
            if (e.Alt)
            {
                NextAnimation(e.Control);
            }
            else
            {
                SelectNext(scene, e.Control);
            }
            return true;
        }
        if (e.Key == KeyMinus || e.Key == KeyPadSubtract)
        {
            if (e.Alt)
            {
                PrevAnimation(e.Control);
            }
            else
            {
                SelectPrev(scene, e.Control);
            }
            return true;
        }
        if (e.Key == Keys::X
            && scene.AllowCameraMovement()
            && !(scene.CameraMode() == CameraMode::Player))
        {
            LookAtSelection(scene, e.Control, e.Shift);
            return true;
        }
        if (e.Key == Keys::D0 || e.Key == KeyPad0)
        {
            if (_mesh != nullptr)
            {
                _mesh->Visible = !_mesh->Visible;
            }
            else if (_node != nullptr)
            {
                _node->Enabled = !_node->Enabled;
            }
            else if (_instance != nullptr)
            {
                _instance->Active = !_instance->Active;
            }
            else if (Entity() != nullptr)
            {
                if (e.Control)
                {
                    Entity()->SetActive(!e.Shift);
                }
                else
                {
                    const std::shared_ptr<Entities::EntityBase> target = Entity();
                    const bool hidden = Entity()->Hidden;
                    target->Hidden = !hidden;
                }
            }
            return true;
        }
        if (e.Key == Keys::D1 || e.Key == KeyPad1)
        {
            PrevRecolor();
            return true;
        }
        if (e.Key == Keys::D2 || e.Key == KeyPad2)
        {
            NextRecolor();
            return true;
        }
        return false;
    }

    void Selection::OnKeyHeld(
        ::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState keyboardState)
    {
        if (Entity() != nullptr && Entity()->Type != EntityType::Room)
        {
            float step = 0.1F;
            ::OpenTK::Mathematics::Vector3 position = Entity()->Position;
            if (keyboardState.IsKeyDown(Keys::W))
            {
                position.Z -= step;
            }
            else if (keyboardState.IsKeyDown(Keys::S))
            {
                position.Z += step;
            }
            if (keyboardState.IsKeyDown(Keys::Space))
            {
                position.Y += step;
            }
            else if (keyboardState.IsKeyDown(Keys::V))
            {
                position.Y -= step;
            }
            if (keyboardState.IsKeyDown(Keys::A))
            {
                position.X -= step;
            }
            else if (keyboardState.IsKeyDown(Keys::D))
            {
                position.X += step;
            }

            step = 0.0436332F;
            ::OpenTK::Mathematics::Vector3 rotation = Entity()->Rotation;
            if (keyboardState.IsKeyDown(Keys::Up))
            {
                rotation.X += step;
            }
            else if (keyboardState.IsKeyDown(Keys::Down))
            {
                rotation.X -= step;
            }
            if (keyboardState.IsKeyDown(Keys::Left))
            {
                rotation.Y += step;
            }
            else if (keyboardState.IsKeyDown(Keys::Right))
            {
                rotation.Y -= step;
            }
            while (rotation.X < 0.0F)
            {
                rotation.X += TwoPi;
            }
            while (rotation.X > TwoPi)
            {
                rotation.X -= TwoPi;
            }
            while (rotation.Y < 0.0F)
            {
                rotation.Y += TwoPi;
            }
            while (rotation.Y > TwoPi)
            {
                rotation.Y -= TwoPi;
            }
            while (rotation.Z < 0.0F)
            {
                rotation.Z += TwoPi;
            }
            while (rotation.Z > TwoPi)
            {
                rotation.Z -= TwoPi;
            }
            Entity()->Position = position;
            if (Entity()->Type != EntityType::Player)
            {
                Entity()->Rotation = rotation;
            }
        }
    }

    void Selection::UpdateSelection(bool control, bool shift, Scene& scene)
    {
        if (control && shift)
        {
            _entityNode.reset();
            _instance.reset();
            _node.reset();
            _mesh.reset();
        }
        else if (_mesh != nullptr)
        {
            if (shift)
            {
                _mesh.reset();
            }
            else
            {
                _entityNode.reset();
                _instance.reset();
                _node.reset();
                _mesh.reset();
            }
        }
        else if (_node != nullptr)
        {
            if (shift)
            {
                _node.reset();
            }
            else if (_instance != nullptr && _node->MeshCount > 0)
            {
                const std::shared_ptr<Model> model = _instance->Model();
                const auto& meshes = Require(model).Meshes;
                if (!meshes)
                {
                    throw System::NullReferenceException();
                }
                _mesh = AtNativeCollection(*meshes, _node->MeshId / 2);
            }
        }
        else if (_instance != nullptr)
        {
            if (shift)
            {
                _instance.reset();
            }
            else
            {
                const std::shared_ptr<Model> model = _instance->Model();
                const auto& nodes = Require(model).Nodes;
                if (!nodes)
                {
                    throw System::NullReferenceException();
                }
                _node = nodes->empty() ? nullptr : (*nodes)[0];
            }
        }
        else if (Entity() != nullptr)
        {
            if (shift)
            {
                _entityNode.reset();
            }
            else
            {
                bool anyNonPlaceholder = false;
                for (const std::shared_ptr<ModelInstance>& model : Entity()->GetModels())
                {
                    if (!Require(model).IsPlaceholder)
                    {
                        anyNonPlaceholder = true;
                        break;
                    }
                }
                if (anyNonPlaceholder)
                {
                    const auto& models = Entity()->GetModels();
                    _instance = models.empty() ? nullptr : models[0];
                }
            }
        }
        else if (!shift)
        {
            _entityNode = scene.Entities().FirstNode();
        }
    }

    void Selection::NextAnimation(bool control)
    {
        std::shared_ptr<ModelInstance> inst{};
        if (_instance != nullptr)
        {
            inst = _instance;
        }
        else if (Entity() != nullptr)
        {
            const auto& models = Entity()->GetModels();
            inst = models.empty() ? nullptr : models[0];
        }
        if (inst != nullptr)
        {
            AnimationInfo& animInfo = Require(inst->AnimInfo);
            if (control)
            {
                std::int32_t index = UncheckedAdd(animInfo.MaterialIndex(), 1);
                while (true)
                {
                    inst->SetMaterialAnim(index);
                    index = UncheckedAdd(index, 1);
                    if (animInfo.MaterialIndex() == -1)
                    {
                        break;
                    }
                    MaterialAnimationInfo& material = Require(animInfo.Material);
                    if (material.Group == nullptr || material.Group->Count != 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                std::int32_t index = UncheckedAdd(animInfo.NodeIndex(), 1);
                while (true)
                {
                    inst->SetNodeAnim(index);
                    index = UncheckedAdd(index, 1);
                    if (animInfo.NodeIndex() == -1)
                    {
                        break;
                    }
                    NodeAnimationInfo& node = Require(animInfo.Node);
                    if (node.Group == nullptr || node.Group->Count != 0)
                    {
                        break;
                    }
                }
            }
        }
    }

    void Selection::PrevAnimation(bool control)
    {
        std::shared_ptr<ModelInstance> inst{};
        if (_instance != nullptr)
        {
            inst = _instance;
        }
        else if (Entity() != nullptr)
        {
            const auto& models = Entity()->GetModels();
            inst = models.empty() ? nullptr : models[0];
        }
        if (inst != nullptr)
        {
            AnimationInfo& animInfo = Require(inst->AnimInfo);
            if (control)
            {
                std::int32_t index = UncheckedAdd(animInfo.MaterialIndex(), -1);
                if (index < -1)
                {
                    const std::shared_ptr<Model> model = inst->Model();
                    const auto& groups = Require(model).AnimationGroups;
                    if (!groups || !groups->Material)
                    {
                        throw System::NullReferenceException();
                    }
                    index = static_cast<std::int32_t>(groups->Material->size()) - 1;
                }
                while (true)
                {
                    inst->SetMaterialAnim(index);
                    index = UncheckedAdd(index, -1);
                    if (animInfo.MaterialIndex() == -1)
                    {
                        break;
                    }
                    MaterialAnimationInfo& material = Require(animInfo.Material);
                    if (material.Group == nullptr || material.Group->Count != 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                std::int32_t index = UncheckedAdd(animInfo.NodeIndex(), -1);
                if (index < -1)
                {
                    const std::shared_ptr<Model> model = inst->Model();
                    const auto& groups = Require(model).AnimationGroups;
                    if (!groups || !groups->Node)
                    {
                        throw System::NullReferenceException();
                    }
                    index = static_cast<std::int32_t>(groups->Node->size()) - 1;
                }
                while (true)
                {
                    inst->SetNodeAnim(index);
                    index = UncheckedAdd(index, -1);
                    if (animInfo.NodeIndex() == -1)
                    {
                        break;
                    }
                    NodeAnimationInfo& node = Require(animInfo.Node);
                    if (node.Group == nullptr || node.Group->Count != 0)
                    {
                        break;
                    }
                }
            }
        }
    }

    void Selection::NextRecolor()
    {
        if (Entity() != nullptr)
        {
            const auto& models = Entity()->GetModels();
            const std::shared_ptr<ModelInstance> instance = models.empty() ? nullptr : models[0];
            if (instance != nullptr)
            {
                const std::shared_ptr<Model> model = instance->Model();
                const auto& recolors = Require(model).Recolors;
                if (!recolors)
                {
                    throw System::NullReferenceException();
                }
                std::int32_t recolor = UncheckedAdd(Entity()->Recolor(), 1);
                if (recolor >= static_cast<std::int32_t>(recolors->size()))
                {
                    recolor = 0;
                }
                Entity()->SetRecolor(recolor);
            }
        }
    }

    void Selection::PrevRecolor()
    {
        if (Entity() != nullptr)
        {
            const auto& models = Entity()->GetModels();
            const std::shared_ptr<ModelInstance> instance = models.empty() ? nullptr : models[0];
            if (instance != nullptr)
            {
                const std::shared_ptr<Model> model = instance->Model();
                const auto& recolors = Require(model).Recolors;
                if (!recolors)
                {
                    throw System::NullReferenceException();
                }
                std::int32_t recolor = UncheckedAdd(Entity()->Recolor(), -1);
                if (recolor < 0)
                {
                    recolor = static_cast<std::int32_t>(recolors->size()) - 1;
                }
                Entity()->SetRecolor(recolor);
            }
        }
    }

    void Selection::SelectNext(Scene& scene, bool control)
    {
        if (_mesh != nullptr)
        {
            SelectMesh(1);
        }
        else if (_node != nullptr)
        {
            SelectNode(1, control);
        }
        else if (_instance != nullptr)
        {
            SelectInstance(1);
        }
        else if (Entity() != nullptr)
        {
            SelectEntity(1, scene);
        }
    }

    void Selection::SelectPrev(Scene& scene, bool control)
    {
        if (_mesh != nullptr)
        {
            SelectMesh(-1);
        }
        else if (_node != nullptr)
        {
            SelectNode(-1, control);
        }
        else if (_instance != nullptr)
        {
            SelectInstance(-1);
        }
        else if (Entity() != nullptr)
        {
            SelectEntity(-1, scene);
        }
    }

    void Selection::SelectMesh(std::int32_t direction)
    {
        if (_instance != nullptr && _node != nullptr)
        {
            std::shared_ptr<::MphRead::Mesh> mesh{};
            _meshBuffer.clear();
            const std::shared_ptr<Model> model = _instance->Model();
            const auto& meshes = Require(model).Meshes;
            if (!meshes)
            {
                throw System::NullReferenceException();
            }
            const std::int32_t start = _node->MeshId / 2;
            for (std::int32_t i = 0; i < _node->MeshCount; ++i)
            {
                _meshBuffer.push_back(AtNativeCollection(*meshes, UncheckedAdd(start, i)));
            }
            std::int32_t index = IndexOfIdentity(_meshBuffer, _mesh);
            while (mesh != _mesh)
            {
                index = UncheckedAdd(index, direction);
                if (index < 0)
                {
                    index = static_cast<std::int32_t>(_meshBuffer.size()) - 1;
                }
                else if (index >= static_cast<std::int32_t>(_meshBuffer.size()))
                {
                    index = 0;
                }
                mesh = AtNativeCollection(_meshBuffer, index);
                if (FilterMesh())
                {
                    _mesh = mesh;
                    break;
                }
            }
        }
    }

    bool Selection::FilterMesh()
    {
        return true;
    }

    void Selection::SelectNode(std::int32_t direction, bool control)
    {
        if (_instance != nullptr)
        {
            std::shared_ptr<::MphRead::Node> node{};
            const std::shared_ptr<Model> model = _instance->Model();
            const auto& nodesPtr = Require(model).Nodes;
            if (!nodesPtr)
            {
                throw System::NullReferenceException();
            }
            const auto& nodes = *nodesPtr;
            std::int32_t index = IndexOfIdentity(nodes, _node);
            while (node != _node)
            {
                index = UncheckedAdd(index, direction);
                if (index < 0)
                {
                    index = static_cast<std::int32_t>(nodes.size()) - 1;
                }
                else if (index >= static_cast<std::int32_t>(nodes.size()))
                {
                    index = 0;
                }
                node = AtNativeCollection(nodes, index);
                if (FilterNode(Require(node), control))
                {
                    _node = node;
                    break;
                }
            }
        }
    }

    bool Selection::FilterNode(const ::MphRead::Node& node, bool roomOnly)
    {
        return !roomOnly || node.RoomPartId >= 0;
    }

    void Selection::SelectInstance(std::int32_t direction)
    {
        if (Entity() != nullptr)
        {
            std::shared_ptr<ModelInstance> inst{};
            const auto& insts = Entity()->GetModels();
            std::int32_t index = IndexOfIdentity(insts, _instance);
            while (inst != _instance)
            {
                index = UncheckedAdd(index, direction);
                if (index < 0)
                {
                    index = static_cast<std::int32_t>(insts.size()) - 1;
                }
                else if (index >= static_cast<std::int32_t>(insts.size()))
                {
                    index = 0;
                }
                inst = AtNativeCollection(insts, index);
                if (FilterInstance())
                {
                    _instance = inst;
                    break;
                }
            }
        }
    }

    bool Selection::FilterInstance()
    {
        return true;
    }

    void Selection::SelectEntity(std::int32_t direction, Scene& scene)
    {
        std::shared_ptr<LinkedListNode<Entities::EntityBase>> entity = _entityNode;
        while (entity != nullptr)
        {
            entity = direction == -1 ? entity->Previous() : entity->Next();
            if (entity == nullptr)
            {
                entity = direction == -1
                    ? scene.Entities().LastNode()
                    : scene.Entities().FirstNode();
            }
            else if (entity == _entityNode)
            {
                break;
            }
            if (FilterEntity(Require(entity).Value(), scene))
            {
                _entityNode = entity;
                break;
            }
        }
    }

    bool Selection::FilterEntity(
        const std::shared_ptr<Entities::EntityBase>& entity,
        const Scene& scene)
    {
        Entities::EntityBase& value = Require(entity);
        if (value.Type == EntityType::BeamEffect || value.Type == EntityType::BeamProjectile)
        {
            return true;
        }
        for (const std::shared_ptr<ModelInstance>& model : value.GetModels())
        {
            if ((scene.ShowAllEntities() || Require(model).Active)
                && (scene.ShowAllEntities()
                    || scene.ShowInvisibleEntities()
                    || !Require(model).IsPlaceholder))
            {
                return true;
            }
        }
        return false;
    }

    void Selection::LookAtSelection(Scene& scene, bool control, bool shift)
    {
        if (control)
        {
            const std::shared_ptr<Entities::EntityBase> entity = Entity();
            Entities::EntityBase* target = entity == nullptr
                ? nullptr
                : (shift ? entity->GetChild() : entity->GetParent());
            if (target != nullptr)
            {
                scene.LookAt(target->Position);
            }
        }
        else if (_node != nullptr)
        {
            scene.LookAt(_node->Animation.Row3().Xyz());
        }
        else if (Entity() != nullptr)
        {
            scene.LookAt(Entity()->Position);
        }
    }
}

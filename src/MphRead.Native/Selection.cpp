#include "Selection.hpp"

#include "Entities/EntityBase.hpp"
#include "Formats/Model.hpp"
#include "Scene.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>

namespace MphRead
{
    namespace SelectionExternal
    {
        // Renderer.cs owns these Scene members in C#. Their native provider is not
        // present yet. These declarations are the exact unresolved boundary used by
        // Selection.cs and intentionally have no substitute implementation here.
        [[nodiscard]] bool AllowCameraMovement(const Scene& scene);
        [[nodiscard]] bool CameraModeIsPlayer(const Scene& scene);
        [[nodiscard]] bool ShowAllEntities(const Scene& scene);
        [[nodiscard]] bool ShowInvisibleEntities(const Scene& scene);
        void LookAt(Scene& scene, ::OpenTK::Mathematics::Vector3 position);
    }

    namespace
    {
        using Keys = ::OpenTK::Windowing::GraphicsLibraryFramework::Keys;

        // PlayerInput.hpp currently exposes only the subset of OpenTK Keys used by
        // player input. Selection.cs also observes these GLFW/OpenTK key values.
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
        [[nodiscard]] std::shared_ptr<T> AtManaged(
            const std::vector<std::shared_ptr<T>>& values,
            std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw std::out_of_range("Index was out of range.");
            }
            return values[static_cast<std::size_t>(index)];
        }
    }

    std::shared_ptr<LinkedListNode<Entities::EntityBase>> Selection::_entityNode{};
    std::shared_ptr<ModelInstance> Selection::_instance{};
    std::shared_ptr<::MphRead::Node> Selection::_node{};
    std::shared_ptr<::MphRead::Mesh> Selection::_mesh{};
    bool Selection::_showSelection = true;
    bool Selection::_hideUnselectedVolumes = false;
    std::vector<std::shared_ptr<::MphRead::Mesh>> Selection::_meshBuffer{};

    const std::shared_ptr<LinkedListNode<Entities::EntityBase>>& Selection::EntityNode() noexcept
    {
        return _entityNode;
    }

    std::shared_ptr<Entities::EntityBase> Selection::Entity() noexcept
    {
        return _entityNode ? _entityNode->Value() : nullptr;
    }

    const std::shared_ptr<ModelInstance>& Selection::Instance() noexcept
    {
        return _instance;
    }

    const std::shared_ptr<::MphRead::Node>& Selection::Node() noexcept
    {
        return _node;
    }

    const std::shared_ptr<::MphRead::Mesh>& Selection::Mesh() noexcept
    {
        return _mesh;
    }

    bool Selection::Any() noexcept
    {
        return _mesh != nullptr || _node != nullptr || _instance != nullptr || Entity() != nullptr;
    }

    bool Selection::CheckVolume(const std::shared_ptr<Entities::EntityBase>& entity)
    {
        const std::shared_ptr<Entities::EntityBase> selected = Entity();
        return !_hideUnselectedVolumes || selected == nullptr || entity == selected;
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
        const std::shared_ptr<Entities::EntityBase> selected = Entity();
        if (_mesh != nullptr)
        {
            if (mesh == _mesh && node == _node && inst == _instance && entity == selected)
            {
                return SelectionType::Selected;
            }
        }
        else if (_node != nullptr)
        {
            if (node == _node && inst == _instance && entity == selected)
            {
                return SelectionType::Selected;
            }
        }
        else if (_instance != nullptr)
        {
            if (inst == _instance && entity == selected)
            {
                return SelectionType::Selected;
            }
        }
        else if (selected != nullptr)
        {
            if (entity == selected)
            {
                return SelectionType::Selected;
            }
        }
        if (selected != nullptr)
        {
            if (selected->GetParent() == entity.get())
            {
                return SelectionType::Parent;
            }
            if (selected->GetChild() == entity.get())
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

    float Selection::GetFactor()
    {
        using namespace std::chrono;
        const std::int64_t ms = duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()).count();
        float percentage = static_cast<float>(ms % 1000) / 1000.0F;
        if (ms / 1000 % 10 % 2 == 0)
        {
            percentage = 1.0F - percentage;
        }
        return percentage;
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
            && SelectionExternal::AllowCameraMovement(scene)
            && !SelectionExternal::CameraModeIsPlayer(scene))
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
            else
            {
                const std::shared_ptr<Entities::EntityBase> entity = Entity();
                if (entity != nullptr)
                {
                    if (e.Control)
                    {
                        entity->SetActive(!e.Shift);
                    }
                    else
                    {
                        entity->Hidden = !entity->Hidden;
                    }
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
        const std::shared_ptr<Entities::EntityBase> entity = Entity();
        if (entity != nullptr && entity->Type != EntityType::Room)
        {
            float step = 0.1F;
            ::OpenTK::Mathematics::Vector3 position = entity->Position;
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
            ::OpenTK::Mathematics::Vector3 rotation = entity->Rotation;
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
            entity->Position = position;
            if (entity->Type != EntityType::Player)
            {
                entity->Rotation = rotation;
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
                _mesh = AtManaged(*meshes, _node->MeshId / 2);
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
        else
        {
            const std::shared_ptr<Entities::EntityBase> entity = Entity();
            if (entity != nullptr)
            {
                if (shift)
                {
                    _entityNode.reset();
                }
                else
                {
                    bool anyNonPlaceholder = false;
                    for (const std::shared_ptr<ModelInstance>& model : entity->GetModels())
                    {
                        if (!Require(model).IsPlaceholder)
                        {
                            anyNonPlaceholder = true;
                            break;
                        }
                    }
                    if (anyNonPlaceholder)
                    {
                        const auto& models = entity->GetModels();
                        _instance = models.empty() ? nullptr : models[0];
                    }
                }
            }
            else if (!shift)
            {
                _entityNode = scene.Entities().FirstNode();
            }
        }
    }

    void Selection::NextAnimation(bool control)
    {
        std::shared_ptr<ModelInstance> inst{};
        if (_instance != nullptr)
        {
            inst = _instance;
        }
        else
        {
            const std::shared_ptr<Entities::EntityBase> entity = Entity();
            if (entity != nullptr)
            {
                const auto& models = entity->GetModels();
                inst = models.empty() ? nullptr : models[0];
            }
        }
        if (inst != nullptr)
        {
            AnimationInfo& animInfo = Require(inst->AnimInfo);
            if (control)
            {
                std::int32_t index = animInfo.MaterialIndex() + 1;
                do
                {
                    inst->SetMaterialAnim(index);
                    ++index;
                }
                while (animInfo.MaterialIndex() != -1
                    && Require(animInfo.Material).Group != nullptr
                    && Require(animInfo.Material).Group->Count == 0);
            }
            else
            {
                std::int32_t index = animInfo.NodeIndex() + 1;
                do
                {
                    inst->SetNodeAnim(index);
                    ++index;
                }
                while (animInfo.NodeIndex() != -1
                    && Require(animInfo.Node).Group != nullptr
                    && Require(animInfo.Node).Group->Count == 0);
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
        else
        {
            const std::shared_ptr<Entities::EntityBase> entity = Entity();
            if (entity != nullptr)
            {
                const auto& models = entity->GetModels();
                inst = models.empty() ? nullptr : models[0];
            }
        }
        if (inst != nullptr)
        {
            AnimationInfo& animInfo = Require(inst->AnimInfo);
            const std::shared_ptr<Model> model = inst->Model();
            if (control)
            {
                std::int32_t index = animInfo.MaterialIndex() - 1;
                if (index < -1)
                {
                    const auto& groups = Require(model).AnimationGroups;
                    if (!groups || !groups->Material)
                    {
                        throw System::NullReferenceException();
                    }
                    index = static_cast<std::int32_t>(groups->Material->size()) - 1;
                }
                do
                {
                    inst->SetMaterialAnim(index);
                    --index;
                }
                while (animInfo.MaterialIndex() != -1
                    && Require(animInfo.Material).Group != nullptr
                    && Require(animInfo.Material).Group->Count == 0);
            }
            else
            {
                std::int32_t index = animInfo.NodeIndex() - 1;
                if (index < -1)
                {
                    const auto& groups = Require(model).AnimationGroups;
                    if (!groups || !groups->Node)
                    {
                        throw System::NullReferenceException();
                    }
                    index = static_cast<std::int32_t>(groups->Node->size()) - 1;
                }
                do
                {
                    inst->SetNodeAnim(index);
                    --index;
                }
                while (animInfo.NodeIndex() != -1
                    && Require(animInfo.Node).Group != nullptr
                    && Require(animInfo.Node).Group->Count == 0);
            }
        }
    }

    void Selection::NextRecolor()
    {
        const std::shared_ptr<Entities::EntityBase> entity = Entity();
        if (entity != nullptr)
        {
            const auto& models = entity->GetModels();
            const std::shared_ptr<ModelInstance> instance = models.empty() ? nullptr : models[0];
            if (instance != nullptr)
            {
                const std::shared_ptr<Model> model = instance->Model();
                const auto& recolors = Require(model).Recolors;
                if (!recolors)
                {
                    throw System::NullReferenceException();
                }
                std::int32_t recolor = entity->Recolor() + 1;
                if (recolor >= static_cast<std::int32_t>(recolors->size()))
                {
                    recolor = 0;
                }
                entity->SetRecolor(recolor);
            }
        }
    }

    void Selection::PrevRecolor()
    {
        const std::shared_ptr<Entities::EntityBase> entity = Entity();
        if (entity != nullptr)
        {
            const auto& models = entity->GetModels();
            const std::shared_ptr<ModelInstance> instance = models.empty() ? nullptr : models[0];
            if (instance != nullptr)
            {
                const std::shared_ptr<Model> model = instance->Model();
                const auto& recolors = Require(model).Recolors;
                if (!recolors)
                {
                    throw System::NullReferenceException();
                }
                std::int32_t recolor = entity->Recolor() - 1;
                if (recolor < 0)
                {
                    recolor = static_cast<std::int32_t>(recolors->size()) - 1;
                }
                entity->SetRecolor(recolor);
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
                _meshBuffer.push_back(AtManaged(*meshes, start + i));
            }
            std::int32_t index = IndexOfIdentity(_meshBuffer, _mesh);
            while (mesh != _mesh)
            {
                index += direction;
                if (index < 0)
                {
                    index = static_cast<std::int32_t>(_meshBuffer.size()) - 1;
                }
                else if (index >= static_cast<std::int32_t>(_meshBuffer.size()))
                {
                    index = 0;
                }
                mesh = AtManaged(_meshBuffer, index);
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
                index += direction;
                if (index < 0)
                {
                    index = static_cast<std::int32_t>(nodes.size()) - 1;
                }
                else if (index >= static_cast<std::int32_t>(nodes.size()))
                {
                    index = 0;
                }
                node = AtManaged(nodes, index);
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
        const std::shared_ptr<Entities::EntityBase> entity = Entity();
        if (entity != nullptr)
        {
            std::shared_ptr<ModelInstance> inst{};
            const auto& insts = entity->GetModels();
            std::int32_t index = IndexOfIdentity(insts, _instance);
            while (inst != _instance)
            {
                index += direction;
                if (index < 0)
                {
                    index = static_cast<std::int32_t>(insts.size()) - 1;
                }
                else if (index >= static_cast<std::int32_t>(insts.size()))
                {
                    index = 0;
                }
                inst = AtManaged(insts, index);
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
            if (entity != nullptr && FilterEntity(entity->Value(), scene))
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
        const bool showAllEntities = SelectionExternal::ShowAllEntities(scene);
        const bool showInvisibleEntities = SelectionExternal::ShowInvisibleEntities(scene);
        for (const std::shared_ptr<ModelInstance>& model : value.GetModels())
        {
            ModelInstance& instance = Require(model);
            if ((showAllEntities || instance.Active)
                && (showAllEntities || showInvisibleEntities || !instance.IsPlaceholder))
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
            Entities::EntityBase* target = nullptr;
            if (entity != nullptr)
            {
                target = shift ? entity->GetChild() : entity->GetParent();
            }
            if (target != nullptr)
            {
                SelectionExternal::LookAt(scene, target->Position);
            }
        }
        else if (_node != nullptr)
        {
            SelectionExternal::LookAt(scene, _node->Animation.Row3().Xyz());
        }
        else
        {
            const std::shared_ptr<Entities::EntityBase> entity = Entity();
            if (entity != nullptr)
            {
                SelectionExternal::LookAt(scene, entity->Position);
            }
        }
    }
}

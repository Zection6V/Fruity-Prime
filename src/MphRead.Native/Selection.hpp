#pragma once

#include "Entities/Players/PlayerInput.hpp"
#include "Formats/Formats.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace OpenTK::Windowing::Common
{
    struct KeyboardKeyEventArgs;
}

namespace MphRead
{
    class ModelInstance;
    class Scene;

    namespace Entities
    {
        class EntityBase;
    }

    template <typename T>
    class LinkedListNode;

    class Selection final
    {
    public:
        [[nodiscard]] static std::shared_ptr<LinkedListNode<Entities::EntityBase>> EntityNode() noexcept;
        [[nodiscard]] static std::shared_ptr<Entities::EntityBase> Entity() noexcept;
        [[nodiscard]] static std::shared_ptr<ModelInstance> Instance() noexcept;
        [[nodiscard]] static std::shared_ptr<::MphRead::Node> Node() noexcept;
        [[nodiscard]] static std::shared_ptr<::MphRead::Mesh> Mesh() noexcept;

        [[nodiscard]] static bool CheckVolume(const std::shared_ptr<Entities::EntityBase>& entity);
        [[nodiscard]] static bool CheckVolume(const Entities::EntityBase* entity);
        static void Clear() noexcept;
        [[nodiscard]] static SelectionType CheckSelection(
            const std::shared_ptr<Entities::EntityBase>& entity,
            const std::shared_ptr<ModelInstance>& inst,
            const std::shared_ptr<::MphRead::Node>& node,
            const std::shared_ptr<::MphRead::Mesh>& mesh);
        [[nodiscard]] static SelectionType CheckSelection(
            const Entities::EntityBase* entity,
            const ModelInstance& inst,
            const ::MphRead::Node& node,
            const ::MphRead::Mesh& mesh);

        static void ToggleShowSelection() noexcept;
        static void ToggleUnselectedVolumes() noexcept;
        [[nodiscard]] static std::optional<::OpenTK::Mathematics::Vector4>
            GetSelectionColor(SelectionType type);

        [[nodiscard]] static bool OnKeyDown(
            const ::OpenTK::Windowing::Common::KeyboardKeyEventArgs& e,
            Scene& scene);
        static void OnKeyHeld(
            ::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState keyboardState);

    private:
        Selection() = delete;

        [[nodiscard]] static bool Any() noexcept;
        [[nodiscard]] static SelectionType CheckSelectionIdentity(
            const Entities::EntityBase* entity,
            const ModelInstance* inst,
            const ::MphRead::Node* node,
            const ::MphRead::Mesh* mesh);
        static void UpdateSelection(bool control, bool shift, Scene& scene);
        static void NextAnimation(bool control);
        static void PrevAnimation(bool control);
        static void NextRecolor();
        static void PrevRecolor();
        static void SelectNext(Scene& scene, bool control);
        static void SelectPrev(Scene& scene, bool control);
        static void SelectMesh(std::int32_t direction);
        [[nodiscard]] static bool FilterMesh();
        static void SelectNode(std::int32_t direction, bool control);
        [[nodiscard]] static bool FilterNode(const ::MphRead::Node& node, bool roomOnly);
        static void SelectInstance(std::int32_t direction);
        [[nodiscard]] static bool FilterInstance();
        static void SelectEntity(std::int32_t direction, Scene& scene);
        [[nodiscard]] static bool FilterEntity(
            const std::shared_ptr<Entities::EntityBase>& entity,
            const Scene& scene);
        static void LookAtSelection(Scene& scene, bool control, bool shift);

        static std::shared_ptr<LinkedListNode<Entities::EntityBase>> _entityNode;
        static std::shared_ptr<ModelInstance> _instance;
        static std::shared_ptr<::MphRead::Node> _node;
        static std::shared_ptr<::MphRead::Mesh> _mesh;
        static bool _showSelection;
        static bool _hideUnselectedVolumes;
        static std::vector<std::shared_ptr<::MphRead::Mesh>> _meshBuffer;
    };
}

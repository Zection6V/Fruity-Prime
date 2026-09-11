#pragma once

#include "../Formats/Entity.hpp"
#include "EntityBase.hpp"

#include <cstdint>

namespace MphRead::Entities
{
    class PointModuleEntity : public EntityBase
    {
    public:
        PointModuleEntity(PointModuleEntityData data, Scene* scene);

        PointModuleEntity(const PointModuleEntity&) = delete;
        PointModuleEntity& operator=(const PointModuleEntity&) = delete;
        PointModuleEntity(PointModuleEntity&&) = delete;
        PointModuleEntity& operator=(PointModuleEntity&&) = delete;

        [[nodiscard]] PointModuleEntity* Next() const;
        [[nodiscard]] PointModuleEntity* Prev() const;
        [[nodiscard]] static PointModuleEntity* Current();

        static constexpr std::int32_t StartId = 50;

        void Initialize() override;
        bool Process() override;
        void SetCurrent();
        void SetActive(bool active) override;
        EntityBase* GetParent() override;
        EntityBase* GetChild() override;

    private:
        void UpdateChain(PointModuleEntity* entity, bool state);

        const PointModuleEntityData _data;
        PointModuleEntity* _next = nullptr;
        PointModuleEntity* _prev = nullptr;

        static PointModuleEntity* _current;
    };
}

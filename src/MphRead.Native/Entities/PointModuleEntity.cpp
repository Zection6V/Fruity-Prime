#include "PointModuleEntity.hpp"

namespace MphRead::Entities
{
    PointModuleEntity* PointModuleEntity::_current = nullptr;

    PointModuleEntity::PointModuleEntity(PointModuleEntityData data, Scene* scene)
        : EntityBase(EntityType::PointModule, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        auto& inst = SetUpModel("pick_morphball", 0, {}, true); // firstHunt: true
        Active = false;
        inst.Active = false;
    }

    PointModuleEntity* PointModuleEntity::Next() const
    {
        return _next;
    }

    PointModuleEntity* PointModuleEntity::Prev() const
    {
        return _prev;
    }

    PointModuleEntity* PointModuleEntity::Current()
    {
        return _current;
    }

    void PointModuleEntity::Initialize()
    {
        EntityBase::Initialize();
        EntityBase* entity = nullptr;
        if (_data.NextId != 0 && _scene->TryGetEntity(_data.NextId, entity))
        {
            _next = &dynamic_cast<PointModuleEntity&>(*entity);
        }
        if (_data.PrevId != 0 && _scene->TryGetEntity(_data.PrevId, entity))
        {
            _prev = &dynamic_cast<PointModuleEntity&>(*entity);
        }
    }

    bool PointModuleEntity::Process()
    {
        if (_current == nullptr && Id == StartId)
        {
            SetCurrent();
        }
        return EntityBase::Process();
    }

    void PointModuleEntity::SetCurrent()
    {
        if (_current != this)
        {
            UpdateChain(_current, false);
            _current = this;
            UpdateChain(_current, true);
        }
    }

    void PointModuleEntity::UpdateChain(PointModuleEntity* entity, bool state)
    {
        std::int32_t i = 0;
        while (entity != nullptr && i < 5)
        {
            entity->SetActive(state);
            entity = entity->Next();
            i++;
        }
    }

    void PointModuleEntity::SetActive(bool active)
    {
        EntityBase::SetActive(active);
        _models[0].Active = Active;
    }

    EntityBase* PointModuleEntity::GetParent()
    {
        return Prev();
    }

    EntityBase* PointModuleEntity::GetChild()
    {
        return Next();
    }
}

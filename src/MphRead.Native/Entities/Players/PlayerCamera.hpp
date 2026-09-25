#pragma once

#include <string>

#include "../../Formats/Culling.hpp"
#include "../../Formats/Types.hpp"

#include <cstdint>
#include <memory>

namespace MphRead::Entities
{
    class PlayerEntity;

    enum class CameraType : std::int32_t
    {
        First = 0,
        Third1 = 1,
        Third2 = 2,
        Free = 3,
        Spectator = 4
    };

    // CameraType.ToString().
    [[nodiscard]] inline std::string ToString(CameraType value)
    {
        switch (value)
        {
        case CameraType::First: return "First";
        case CameraType::Third1: return "Third1";
        case CameraType::Third2: return "Third2";
        case CameraType::Free: return "Free";
        case CameraType::Spectator: return "Spectator";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    class CameraInfo
    {
    public:
        ::OpenTK::Mathematics::Vector3 Position{};
        ::OpenTK::Mathematics::Vector3 PrevPosition{};
        ::OpenTK::Mathematics::Vector3 Target{};
        ::OpenTK::Mathematics::Vector3 UpVector{};
        ::OpenTK::Mathematics::Vector3 TrueUp{};
        ::OpenTK::Mathematics::Vector3 Facing{};
        float Fov = 0.0F;
        float Shake = 0.0F;
        ::OpenTK::Mathematics::Matrix4 ViewMatrix{};
        float Field48 = 0.0F;
        float Field4C = 0.0F;
        float Field50 = 0.0F;
        float Field54 = 0.0F;
        ::MphRead::Formats::Culling::NodeRef NodeRef
            = ::MphRead::Formats::Culling::NodeRef::None;

        CameraInfo() = default;
        CameraInfo(const CameraInfo&) = delete;
        CameraInfo& operator=(const CameraInfo&) = delete;
        CameraInfo(CameraInfo&&) = delete;
        CameraInfo& operator=(CameraInfo&&) = delete;

        void Reset();
        void Update();
        void SetShake(float value);

    private:
        bool _shake = true;
    };
}

#define MPHREAD_PLAYER_CAMERA_MEMBERS \
public: \
    [[nodiscard]] std::shared_ptr<::MphRead::Entities::CameraInfo> CameraInfo() const noexcept; \
    [[nodiscard]] ::MphRead::Entities::CameraType CameraType() const noexcept; \
    void SetUpMatchEndCamera(); \
    void UpdateMatchEndCamera(std::shared_ptr<::MphRead::Entities::PlayerEntity> winner, \
        float timeSinceMatchEnd); \
    void RefreshExternalCamera(); \
    void ResumeOwnCamera(); \
private: \
    void SwitchCamera(::MphRead::Entities::CameraType type, \
        ::OpenTK::Mathematics::Vector3 facing); \
    void UpdateCamera(); \
    void UpdateCameraFirst(); \
    void UpdateCameraThird1(); \
    void UpdateCameraThird2(); \
    void UpdateCameraFree(); \
    void UpdateCameraSpectator(); \
    const std::shared_ptr<::MphRead::Entities::CameraInfo> _cameraInfo \
        = std::make_shared<::MphRead::Entities::CameraInfo>(); \
    ::MphRead::Entities::CameraType _cameraType = ::MphRead::Entities::CameraType::First; \
    ::OpenTK::Mathematics::Vector3 _field544{}; \
    float _field554 = 0.0F; \
    float _field558 = 0.0F; \
    float _field68C = 0.0F; \
    float _field690 = 0.0F;

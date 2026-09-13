#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace OpenTK::Mathematics
{
    struct Vector3;
    struct Vector4;
}

namespace MphRead
{
    class CameraSequenceKeyframe;
    struct CameraSequenceHeader;
    struct RawCameraSequenceKeyframe;
    class Scene;

    namespace Entities
    {
        class CameraInfo;
        class EntityBase;
    }
}

namespace MphRead::Formats
{
    namespace CameraSequenceDetail
    {
        enum class IndexSemantics : std::uint8_t
        {
            Array,
            List
        };

        [[noreturn]] void ThrowArrayIndexOutOfRange();
        [[noreturn]] void ThrowListIndexOutOfRange();
    }

    template <typename T, std::size_t N>
    class CameraSequenceReadOnlyArray final
    {
    public:
        constexpr explicit CameraSequenceReadOnlyArray(
            std::array<T, N> values,
            CameraSequenceDetail::IndexSemantics indexSemantics
                = CameraSequenceDetail::IndexSemantics::Array)
            : _values(std::move(values)),
              _indexSemantics(indexSemantics)
        {
        }

        [[nodiscard]] constexpr std::size_t Count() const noexcept
        {
            return N;
        }

        template <typename TIndex>
        [[nodiscard]] const T& operator[](TIndex index) const
        {
            static_assert(std::is_integral_v<TIndex>);
            if constexpr (std::is_signed_v<TIndex>)
            {
                if (index < 0)
                {
                    ThrowIndexOutOfRange();
                }
            }
            if (static_cast<std::uintmax_t>(index)
                >= static_cast<std::uintmax_t>(N))
            {
                ThrowIndexOutOfRange();
            }
            return _values[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] constexpr auto begin() const noexcept
        {
            return _values.begin();
        }

        [[nodiscard]] constexpr auto end() const noexcept
        {
            return _values.end();
        }

    private:
        [[noreturn]] void ThrowIndexOutOfRange() const
        {
            if (_indexSemantics == CameraSequenceDetail::IndexSemantics::List)
            {
                CameraSequenceDetail::ThrowListIndexOutOfRange();
            }
            CameraSequenceDetail::ThrowArrayIndexOutOfRange();
        }

        std::array<T, N> _values;
        CameraSequenceDetail::IndexSemantics _indexSemantics;
    };

    enum class CamSeqFlags : std::int32_t
    {
        None = 0,
        Complete = 1,
        CanEnd = 2,
        BlockInput = 4,
        ForceAlt = 8,
        ForceBiped = 0x10,
        Loop = 0x20
    };

    class CameraSequence final : public std::enable_shared_from_this<CameraSequence>
    {
    public:
        CameraSequence(const CameraSequence&) = delete;
        CameraSequence& operator=(const CameraSequence&) = delete;
        CameraSequence(CameraSequence&&) = delete;
        CameraSequence& operator=(CameraSequence&&) = delete;
        ~CameraSequence();

        [[nodiscard]] std::int32_t SequenceId() const noexcept;
        [[nodiscard]] const std::string& Name() const noexcept;
        [[nodiscard]] std::uint8_t Version() const noexcept;
        [[nodiscard]] const std::vector<std::shared_ptr<::MphRead::CameraSequenceKeyframe>>&
            Keyframes() const noexcept;

        [[nodiscard]] CamSeqFlags Flags() const noexcept;
        void Flags(CamSeqFlags value) noexcept;
        [[nodiscard]] bool BlockInput() const noexcept;
        [[nodiscard]] bool ForceAlt() const noexcept;
        [[nodiscard]] bool ForceBiped() const noexcept;

        [[nodiscard]] std::uint16_t TransitionTimer() const noexcept;
        void TransitionTimer(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t TransitionTime() const noexcept;

        [[nodiscard]] ::MphRead::Entities::CameraInfo& InitialCamInfo() noexcept;
        [[nodiscard]] const ::MphRead::Entities::CameraInfo& InitialCamInfo() const noexcept;
        [[nodiscard]] ::MphRead::Entities::CameraInfo* CamInfoRef() const noexcept;
        void CamInfoRef(::MphRead::Entities::CameraInfo* value) noexcept;

        [[nodiscard]] static CameraSequence* Current() noexcept;
        static void Current(CameraSequence* value) noexcept;
        [[nodiscard]] static CameraSequence* Intro() noexcept;
        static void Intro(CameraSequence* value) noexcept;
        [[nodiscard]] bool IsIntro() const noexcept;

        void Initialize();
        void Process();
        void SetUp(::MphRead::Entities::CameraInfo& camInfo, std::uint16_t transitionTime);
        void Restart(std::uint16_t transitionTimer = 0, std::uint16_t transitionTime = 0);
        void End();

        [[nodiscard]] static std::shared_ptr<CameraSequence> Load(
            std::int32_t id, ::MphRead::Scene* scene);
        [[nodiscard]] static std::shared_ptr<CameraSequence> Load(
            std::string name, ::MphRead::Scene* scene, std::int32_t id = -1);

        static const CameraSequenceReadOnlyArray<std::int32_t, 199> MusicData;
        static const CameraSequenceReadOnlyArray<std::int32_t, 199> SfxData;
        static const CameraSequenceReadOnlyArray<std::string_view, 199> Filenames;

    private:
        CameraSequence(
            std::int32_t id,
            std::string name,
            ::MphRead::Scene* scene,
            ::MphRead::CameraSequenceHeader header,
            const std::vector<::MphRead::RawCameraSequenceKeyframe>& keyframes);

        void CalculateFrameValues();
        void AddEntityPosition(
            ::MphRead::CameraSequenceKeyframe& keyframe,
            ::OpenTK::Mathematics::Vector3& position,
            ::OpenTK::Mathematics::Vector3& toTarget);
        [[nodiscard]] ::OpenTK::Mathematics::Vector4 GetVec4(float percent) const noexcept;
        [[nodiscard]] std::shared_ptr<::MphRead::Entities::EntityBase> GetKeyframeRef(
            std::int16_t type, std::int16_t id);

        std::int32_t _sequenceId;
        std::string _name;
        std::uint8_t _version;
        std::vector<std::shared_ptr<::MphRead::CameraSequenceKeyframe>> _keyframes;

        CamSeqFlags _flags = CamSeqFlags::None;
        std::uint16_t _transitionTimer = 0;
        std::uint16_t _transitionTime = 0;
        std::int32_t _keyframeIndex = 0;
        float _keyframeElapsed = 0.0F;

        std::unique_ptr<::MphRead::Entities::CameraInfo> _initialCamInfo;
        ::MphRead::Entities::CameraInfo* _camInfoRef = nullptr;
        ::MphRead::Scene* _scene;

        static std::shared_ptr<CameraSequence> _current;
        static std::shared_ptr<CameraSequence> _intro;
    };
}

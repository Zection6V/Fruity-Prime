#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <vector>

namespace NCSFCommon::ReplayGain
{
    class FrequencyInfo
    {
    public:
        using DoubleArray = std::shared_ptr<std::vector<double>>;

        FrequencyInfo(
            std::uint32_t sampleRate,
            DoubleArray bYule,
            DoubleArray aYule,
            DoubleArray bButter,
            DoubleArray aButter) noexcept;

        virtual ~FrequencyInfo() = default;

        FrequencyInfo(FrequencyInfo&&) = delete;
        FrequencyInfo& operator=(const FrequencyInfo&) = delete;
        FrequencyInfo& operator=(FrequencyInfo&&) = delete;

        [[nodiscard]] std::uint32_t SampleRate() const noexcept;
        [[nodiscard]] DoubleArray BYule() const noexcept;
        [[nodiscard]] DoubleArray AYule() const noexcept;
        [[nodiscard]] DoubleArray BButter() const noexcept;
        [[nodiscard]] DoubleArray AButter() const noexcept;

        [[nodiscard]] virtual bool Equals(const FrequencyInfo* other) const noexcept;
        [[nodiscard]] virtual std::int32_t GetHashCode() const noexcept;
        [[nodiscard]] virtual std::string ToString() const;
        [[nodiscard]] virtual std::shared_ptr<FrequencyInfo> Clone() const;

        void Deconstruct(
            std::uint32_t& sampleRate,
            DoubleArray& bYule,
            DoubleArray& aYule,
            DoubleArray& bButter,
            DoubleArray& aButter) const noexcept;

        // C++20 adapter for C# with-expressions. std::nullopt means that the
        // corresponding init-only positional property is not assigned.
        [[nodiscard]] std::shared_ptr<FrequencyInfo> With(
            std::optional<std::uint32_t> sampleRate = std::nullopt,
            std::optional<DoubleArray> bYule = std::nullopt,
            std::optional<DoubleArray> aYule = std::nullopt,
            std::optional<DoubleArray> bButter = std::nullopt,
            std::optional<DoubleArray> aButter = std::nullopt) const;

        friend bool operator==(const FrequencyInfo& left, const FrequencyInfo& right) noexcept;
        friend bool operator!=(const FrequencyInfo& left, const FrequencyInfo& right) noexcept;

    protected:
        FrequencyInfo(const FrequencyInfo&) noexcept = default;

        [[nodiscard]] virtual const std::type_info& EqualityContract() const noexcept;
        virtual bool PrintMembers(std::string& builder) const;

    private:
        std::uint32_t sampleRateValue;
        DoubleArray bYuleValue;
        DoubleArray aYuleValue;
        DoubleArray bButterValue;
        DoubleArray aButterValue;
    };

    [[nodiscard]] bool operator==(
        const std::shared_ptr<FrequencyInfo>& left,
        const std::shared_ptr<FrequencyInfo>& right) noexcept;
    [[nodiscard]] bool operator!=(
        const std::shared_ptr<FrequencyInfo>& left,
        const std::shared_ptr<FrequencyInfo>& right) noexcept;
}

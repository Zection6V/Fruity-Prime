#include "FrequencyInfo.hpp"

#include <bit>
#include <functional>
#include <typeindex>
#include <utility>

namespace
{
    constexpr std::uint32_t RecordHashFactor = 0xA5555529U;

    [[nodiscard]] std::uint32_t TruncateHash(std::size_t value) noexcept
    {
        return static_cast<std::uint32_t>(value);
    }

    [[nodiscard]] std::uint32_t ArrayIdentityHash(const NCSFCommon::ReplayGain::FrequencyInfo::DoubleArray& value) noexcept
    {
        if (!value)
        {
            return 0;
        }
        return TruncateHash(std::hash<const void*>{}(value.get()));
    }
}

namespace NCSFCommon::ReplayGain
{
    FrequencyInfo::FrequencyInfo(
        std::uint32_t sampleRate,
        DoubleArray bYule,
        DoubleArray aYule,
        DoubleArray bButter,
        DoubleArray aButter) noexcept
        : sampleRateValue(sampleRate),
          bYuleValue(std::move(bYule)),
          aYuleValue(std::move(aYule)),
          bButterValue(std::move(bButter)),
          aButterValue(std::move(aButter))
    {
    }

    std::uint32_t FrequencyInfo::SampleRate() const noexcept
    {
        return sampleRateValue;
    }

    FrequencyInfo::DoubleArray FrequencyInfo::BYule() const noexcept
    {
        return bYuleValue;
    }

    FrequencyInfo::DoubleArray FrequencyInfo::AYule() const noexcept
    {
        return aYuleValue;
    }

    FrequencyInfo::DoubleArray FrequencyInfo::BButter() const noexcept
    {
        return bButterValue;
    }

    FrequencyInfo::DoubleArray FrequencyInfo::AButter() const noexcept
    {
        return aButterValue;
    }

    const std::type_info& FrequencyInfo::EqualityContract() const noexcept
    {
        return typeid(*this);
    }

    bool FrequencyInfo::Equals(const FrequencyInfo* other) const noexcept
    {
        return this == other
            || (other != nullptr
                && EqualityContract() == other->EqualityContract()
                && sampleRateValue == other->sampleRateValue
                && bYuleValue == other->bYuleValue
                && aYuleValue == other->aYuleValue
                && bButterValue == other->bButterValue
                && aButterValue == other->aButterValue);
    }

    bool FrequencyInfo::Equals(const FrequencyInfo& other) const noexcept
    {
        return Equals(&other);
    }

    std::int32_t FrequencyInfo::GetHashCode() const noexcept
    {
        std::uint32_t hash = TruncateHash(std::type_index(EqualityContract()).hash_code());
        hash = hash * RecordHashFactor + sampleRateValue;
        hash = hash * RecordHashFactor + ArrayIdentityHash(bYuleValue);
        hash = hash * RecordHashFactor + ArrayIdentityHash(aYuleValue);
        hash = hash * RecordHashFactor + ArrayIdentityHash(bButterValue);
        hash = hash * RecordHashFactor + ArrayIdentityHash(aButterValue);
        return std::bit_cast<std::int32_t>(hash);
    }

    bool FrequencyInfo::PrintMembers(std::string& builder) const
    {
        builder += "SampleRate = ";
        builder += std::to_string(sampleRateValue);
        builder += ", BYule = ";
        if (bYuleValue) builder += "System.Double[]";
        builder += ", AYule = ";
        if (aYuleValue) builder += "System.Double[]";
        builder += ", BButter = ";
        if (bButterValue) builder += "System.Double[]";
        builder += ", AButter = ";
        if (aButterValue) builder += "System.Double[]";
        return true;
    }

    std::string FrequencyInfo::ToString() const
    {
        std::string builder = "FrequencyInfo { ";
        if (PrintMembers(builder))
        {
            builder += ' ';
        }
        builder += '}';
        return builder;
    }

    void FrequencyInfo::Deconstruct(
        std::uint32_t& sampleRate,
        DoubleArray& bYule,
        DoubleArray& aYule,
        DoubleArray& bButter,
        DoubleArray& aButter) const noexcept
    {
        sampleRate = sampleRateValue;
        bYule = bYuleValue;
        aYule = aYuleValue;
        bButter = bButterValue;
        aButter = aButterValue;
    }

    FrequencyInfo FrequencyInfo::With(
        std::optional<std::uint32_t> sampleRate,
        std::optional<DoubleArray> bYule,
        std::optional<DoubleArray> aYule,
        std::optional<DoubleArray> bButter,
        std::optional<DoubleArray> aButter) const noexcept
    {
        return FrequencyInfo(
            sampleRate.value_or(sampleRateValue),
            bYule.has_value() ? *bYule : bYuleValue,
            aYule.has_value() ? *aYule : aYuleValue,
            bButter.has_value() ? *bButter : bButterValue,
            aButter.has_value() ? *aButter : aButterValue);
    }

    bool operator==(const FrequencyInfo& left, const FrequencyInfo& right) noexcept
    {
        return left.Equals(&right);
    }

    bool operator!=(const FrequencyInfo& left, const FrequencyInfo& right) noexcept
    {
        return !(left == right);
    }
}

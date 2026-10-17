#include "Mods/Network/Protocol/Packets/RealtimePackets.hpp"

#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    bool Check(bool condition, std::string_view name)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << name << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    using MphRead::Mods::Network::IntentPacket;
    using ::OpenTK::Mathematics::Vector3;

    bool passed = true;
    IntentPacket::ShotEvent event{};
    event.Direction = Vector3(0.0F, 0.0F, -1.0F);
    passed &= Check(event.HasRay(), "a finite unit direction is valid");
    event.Direction = Vector3(0.6F, 0.0F, 0.0F);
    passed &= Check(event.HasRay(), "a finite non-unit direction in range is valid");
    event.Direction = Vector3(0.0F, 0.0F, 0.0F);
    passed &= Check(!event.HasRay(), "a zero direction is invalid");
    event.Direction = Vector3(std::numeric_limits<float>::infinity(), 0.0F, 0.0F);
    passed &= Check(!event.HasRay(), "an infinite direction is invalid");
    event.Direction = Vector3(std::numeric_limits<float>::quiet_NaN(), 0.0F, 1.0F);
    passed &= Check(!event.HasRay(), "a NaN direction is invalid");
    event.Direction = Vector3(0.0F, 0.0F, 2.01F);
    passed &= Check(!event.HasRay(), "an oversized direction is invalid");
    event.Direction = Vector3(0.0F, 0.0F, 2.0F);
    passed &= Check(!event.HasRay(), "the upper direction boundary remains exclusive");
    event.Direction = Vector3(0.5F, 0.0F, 0.0F);
    passed &= Check(!event.HasRay(), "the lower direction boundary remains exclusive");

    const Vector3 shooterPosition(10.0F, 20.0F, 30.0F);
    const Vector3 historicalMuzzle(-3.0F, 20.0F, 30.0F);
    const Vector3 recordedDirection(0.0F, 0.0F, -1.0F);
    passed &= Check(MphRead::Mods::Network::ShotRayValidation::IsRecordedRay(
        historicalMuzzle, recordedDirection)
        && !MphRead::Mods::Network::ShotRayValidation::IsPlausibleRay(
            historicalMuzzle, recordedDirection, shooterPosition),
        "a moving shooter's historical demo ray is valid even outside a live pose's muzzle bound");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsRecordedRay(
        Vector3(std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F), recordedDirection)
        && !MphRead::Mods::Network::ShotRayValidation::IsRecordedRay(
            historicalMuzzle, Vector3(std::numeric_limits<float>::infinity(), 0.0F, 0.0F))
        && !MphRead::Mods::Network::ShotRayValidation::IsRecordedRay(
            historicalMuzzle, Vector3(0.0F, 0.0F, 0.0F)),
        "demo rays still reject non-finite origins and invalid directions");
    passed &= Check(MphRead::Mods::Network::ShotRayValidation::IsPlausibleOrigin(
        Vector3(16.0F, 20.0F, 30.0F), shooterPosition),
        "a shot origin at the maximum muzzle offset is plausible");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsPlausibleOrigin(
        Vector3(16.01F, 20.0F, 30.0F), shooterPosition),
        "a shot origin beyond the muzzle offset is rejected");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsPlausibleOrigin(
        Vector3(std::numeric_limits<float>::quiet_NaN(), 20.0F, 30.0F), shooterPosition),
        "a NaN shot origin is rejected");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsPlausibleOrigin(
        Vector3(10.0F, 20.0F, 30.0F),
        Vector3(10.0F, std::numeric_limits<float>::infinity(), 30.0F)),
        "a non-finite shooter position is rejected");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsPlausibleOrigin(
        Vector3(0.0F, 0.0F, 0.0F), Vector3(0.0F, 0.0F, 0.0F),
        std::numeric_limits<float>::quiet_NaN()),
        "a non-finite muzzle offset is rejected");
    passed &= Check(MphRead::Mods::Network::ShotRayValidation::IsPlausibleRay(
        Vector3(13.0F, 20.0F, 30.0F), Vector3(0.0F, 0.0F, -1.0F), shooterPosition),
        "a valid ray from a small muzzle offset is plausible");
    passed &= Check(MphRead::Mods::Network::ShotRayValidation::IsPlausibleRay(
        Vector3(16.0F, 20.0F, 30.0F), Vector3(0.0F, 0.0F, -1.0F), shooterPosition),
        "a valid ray at the maximum muzzle offset is plausible");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsPlausibleRay(
        Vector3(16.01F, 20.0F, 30.0F), Vector3(0.0F, 0.0F, -1.0F), shooterPosition),
        "a valid direction cannot make a distant origin plausible");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsPlausibleRay(
        shooterPosition, Vector3(0.0F, 0.0F, 0.0F), shooterPosition),
        "a zero direction cannot pass ray validation");
    passed &= Check(!MphRead::Mods::Network::ShotRayValidation::IsPlausibleRay(
        shooterPosition, Vector3(std::numeric_limits<float>::quiet_NaN(), 0.0F, 1.0F), shooterPosition),
        "a non-finite direction cannot pass ray validation");
    return passed ? 0 : 1;
}

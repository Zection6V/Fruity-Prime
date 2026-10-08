#include "HitLocation.hpp"

#include "NetHitPrediction.hpp"
#include "NetSession.hpp"

#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace MphRead::Mods::Network
{
    namespace
    {
        std::string Num(float value)
        {
            char buffer[32];
            std::snprintf(buffer, sizeof buffer, "%.4f", static_cast<double>(value));
            return buffer;
        }

        std::int64_t WallMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

        // The shot key's frame as this machine knows it. The shooter and the
        // authority stamp the same one (the authority frame the shooter was
        // drawing); a remote shot drawn on a third machine carries the ack of
        // the intent that fired it, which is that same frame.
        std::uint32_t LaunchOf(const Entities::BeamProjectileEntity& beam)
        {
            return beam.ModShooterAck != 0 ? beam.ModShooterAck : beam.ModLaunchFrame;
        }
    }

    void HitLocation::Open(const std::string& path)
    {
        auto writer = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::trunc);
        if (!writer->good())
        {
            std::cout << "[hitlog] could not open " << path << '\n';
            return;
        }
        *writer << "ev,wall_ms,frame,snap,auth,local,shooter,victim,beam,launch,splash,hs,dmg,"
                   "px,py,pz,vx,vy,vz,fx,fz,dy,hpct,ang,rad,alt,extra\n";
        _writer = std::move(writer);
        std::cout << "[hitlog] writing hit locations to " << path << '\n';
    }

    void HitLocation::Close()
    {
        if (_writer)
        {
            _writer->flush();
            _writer.reset();
        }
    }

    HitLocation::Body HitLocation::BodyOf(const Entities::PlayerEntity& victim, OpenTK::Mathematics::Vector3 point)
    {
        Body body{};
        const OpenTK::Mathematics::Vector3 position = victim.Position;
        const OpenTK::Mathematics::Vector3 d = point - position;
        const float minY = Fixed::ToFloat(victim.Values().MinPickupHeight);
        const float maxY = Fixed::ToFloat(victim.Values().MaxPickupHeight);
        body.Dy = d.Y;
        body.Pct = maxY > minY ? (d.Y - minY) * 100.0F / (maxY - minY) : 0.0F;
        body.Radius = std::sqrt(d.X * d.X + d.Z * d.Z);
        const OpenTK::Mathematics::Vector3 facing = victim.FacingVector();
        const float flat = std::sqrt(facing.X * facing.X + facing.Z * facing.Z);
        if (flat > 0.0001F && body.Radius > 0.0001F)
        {
            const float fx = facing.X / flat;
            const float fz = facing.Z / flat;
            // Right of a hunter facing +Z is -X in this game's handedness.
            const float ahead = d.X * fx + d.Z * fz;
            const float right = d.X * -fz + d.Z * fx;
            body.Angle = std::atan2(right, ahead) * 57.29578F;
        }
        return body;
    }

    void HitLocation::Row(const char* event, std::int32_t shooter, const Entities::PlayerEntity& victim,
        std::int32_t beam, std::uint32_t launch, bool splash, bool headshot, std::int32_t damage,
        OpenTK::Mathematics::Vector3 point, const std::string& extra)
    {
        if (!_writer)
        {
            return;
        }
        const Body body = BodyOf(victim, point);
        const OpenTK::Mathematics::Vector3 position = victim.Position;
        const OpenTK::Mathematics::Vector3 facing = victim.FacingVector();
        std::string line;
        line.reserve(256);
        line += event;
        line += ',' + std::to_string(WallMs());
        line += ',' + std::to_string(NetSession::NetFrame());
        line += ',' + std::to_string(NetSession::AppliedSnapshotFrame());
        line += NetSession::IsAuthority() || NetSession::IsHost() ? ",1" : ",0";
        line += ',' + std::to_string(NetSession::LocalSlot());
        line += ',' + std::to_string(shooter);
        line += ',' + std::to_string(victim.SlotIndex());
        line += ',' + std::to_string(beam);
        line += ',' + std::to_string(launch);
        line += splash ? ",1" : ",0";
        line += headshot ? ",1" : ",0";
        line += ',' + std::to_string(damage);
        line += ',' + Num(point.X) + ',' + Num(point.Y) + ',' + Num(point.Z);
        line += ',' + Num(position.X) + ',' + Num(position.Y) + ',' + Num(position.Z);
        line += ',' + Num(facing.X) + ',' + Num(facing.Z);
        line += ',' + Num(body.Dy) + ',' + Num(body.Pct) + ',' + Num(body.Angle) + ',' + Num(body.Radius);
        line += victim.IsAltForm() ? ",1" : ",0";
        line += ',' + extra;
        line += '\n';
        *_writer << line;
        const std::uint32_t now = NetSession::NetFrame();
        if (now - _lastFlush >= 60U)
        {
            _lastFlush = now;
            _writer->flush();
        }
    }

    void HitLocation::Contact(const Entities::BeamProjectileEntity& beam, const Entities::PlayerEntity& victim,
        OpenTK::Mathematics::Vector3 point, bool headshot, std::uint32_t damage, bool splash, bool blocked)
    {
        // A body already down on this machine takes no damage (TakeDamage
        // stops at zero health), so nothing is claimed for it either.
        if (!_writer || !NetSession::Active() || victim.Health() <= 0)
        {
            return;
        }
        Entities::PlayerEntity* owner = NetHitPrediction::OwnerOf(
            const_cast<Entities::BeamProjectileEntity*>(&beam));
        if (owner == nullptr || owner == &victim)
        {
            return;
        }
        Row("hit", owner->SlotIndex(), victim, static_cast<std::int32_t>(beam.Beam()), LaunchOf(beam), splash,
            headshot, static_cast<std::int32_t>(damage), point,
            std::to_string(beam.ModLaunchFrame) + (blocked ? ";1" : ";0"));
    }

    void HitLocation::NearMiss(const Entities::BeamProjectileEntity& beam, const Entities::PlayerEntity& victim,
        OpenTK::Mathematics::Vector3 point, float distance)
    {
        if (!_writer || !NetSession::Active())
        {
            return;
        }
        Entities::PlayerEntity* owner = NetHitPrediction::OwnerOf(
            const_cast<Entities::BeamProjectileEntity*>(&beam));
        if (owner == nullptr || owner == &victim)
        {
            return;
        }
        Row("miss", owner->SlotIndex(), victim, static_cast<std::int32_t>(beam.Beam()), LaunchOf(beam), false,
            false, 0, point, Num(distance));
    }

    void HitLocation::Claim(std::int32_t shooter, const Entities::PlayerEntity& victim, std::uint8_t beam,
        std::uint32_t launch, std::int32_t damage, std::uint8_t flags, OpenTK::Mathematics::Vector3 drawn,
        OpenTK::Mathematics::Vector3 history, bool historyKnown, std::int32_t verdict)
    {
        if (!_writer)
        {
            return;
        }
        // The point columns carry where the shooter drew the victim; extra
        // carries the authority's history for that frame and the verdict.
        const std::string extra = (historyKnown
                ? Num(history.X) + ';' + Num(history.Y) + ';' + Num(history.Z)
                : std::string("na;na;na"))
            + ';' + std::to_string(verdict) + ';' + std::to_string(flags);
        Row("claim", shooter, victim, beam, launch, false, (flags & 0x01U) != 0, damage, drawn, extra);
    }

    void HitLocation::Damage(std::int32_t attacker, const Entities::PlayerEntity& victim, std::uint8_t beam,
        std::int32_t damage, bool headshot)
    {
        if (!_writer)
        {
            return;
        }
        Row("dmg", attacker, victim, beam, 0, false, headshot, damage, victim.Position, std::string());
    }
}

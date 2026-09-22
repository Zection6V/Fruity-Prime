#include "Enemies.hpp"
#include "../Scene.hpp"

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <memory>

namespace MphRead::Metadata
{
    namespace
    {
        [[noreturn]] void ThrowIndexOutOfRange()
        {
            throw SceneDetail::IndexOutOfRangeException();
        }

        template <typename T>
        const T& GetAt(const std::vector<T>& values, std::size_t index)
        {
            if (index >= values.size())
            {
                ThrowIndexOutOfRange();
            }
            return values[index];
        }

        // new T[n] { ... } for a table field held as a managed array.
        template <typename T>
        [[nodiscard]] std::shared_ptr<ManagedArray<T>> MakeManagedArray(std::initializer_list<T> values)
        {
            auto array = std::make_shared<ManagedArray<T>>(values.size());
            std::size_t i = 0;
            for (const T& value : values)
            {
                (*array)[i++] = value;
            }
            return array;
        }
    }

    using namespace MphRead::Entities;
    using namespace MphRead::Entities::Enemies;

    namespace
    {
        const std::vector<EnemyBehavior<Enemy00Entity>> Enemy00State0{
            {0, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior02)},
            {1, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy00Entity>> Enemy00State1{
            {1, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior02)},
            {2, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior06)},
            {6, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior07)},
            {6, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy00Entity>> Enemy00State2{
            {3, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior09)},
            {6, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior07)},
            {6, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior10)},
            {6, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy00Entity>> Enemy00State3{
            {4, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy00Entity>> Enemy00State4{
            {5, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior04)},
            {5, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy00Entity>> Enemy00State5{
            {1, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy00Entity>> Enemy00State6{
            {0, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior02)},
            {1, static_cast<bool(*)(Enemy00Entity*)>(&Enemy00Entity::Behavior03)}
        };
    }

    std::vector<EnemySubroutine<Enemy00Entity>> Enemy00Subroutines{
        EnemySubroutine<Enemy00Entity>(Enemy00State0),
        EnemySubroutine<Enemy00Entity>(Enemy00State1),
        EnemySubroutine<Enemy00Entity>(Enemy00State2),
        EnemySubroutine<Enemy00Entity>(Enemy00State3),
        EnemySubroutine<Enemy00Entity>(Enemy00State4),
        EnemySubroutine<Enemy00Entity>(Enemy00State5),
        EnemySubroutine<Enemy00Entity>(Enemy00State6)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State0{
            {2, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State1{
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior09)},
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior10)},
            {10, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior11)},
            {2, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State2{
            {3, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State3{
            {4, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State4{
            {5, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State5{
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior02)},
            {8, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State6{
            {9, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State7{
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior05)},
            {0, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State8{
            {6, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior07)},
            {6, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior02)},
            {6, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State9{
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State10{
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy02Entity>> Enemy02Subroutines{
        EnemySubroutine<Enemy02Entity>(Enemy02State0),
        EnemySubroutine<Enemy02Entity>(Enemy02State1),
        EnemySubroutine<Enemy02Entity>(Enemy02State2),
        EnemySubroutine<Enemy02Entity>(Enemy02State3),
        EnemySubroutine<Enemy02Entity>(Enemy02State4),
        EnemySubroutine<Enemy02Entity>(Enemy02State5),
        EnemySubroutine<Enemy02Entity>(Enemy02State6),
        EnemySubroutine<Enemy02Entity>(Enemy02State7),
        EnemySubroutine<Enemy02Entity>(Enemy02State8),
        EnemySubroutine<Enemy02Entity>(Enemy02State9),
        EnemySubroutine<Enemy02Entity>(Enemy02State10)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy03Entity>> Enemy03State0{
            {1, static_cast<bool(*)(Enemy03Entity*)>(&Enemy03Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy03Entity>> Enemy03State1{
            {2, static_cast<bool(*)(Enemy03Entity*)>(&Enemy03Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy03Entity>> Enemy03State2{
            {0, static_cast<bool(*)(Enemy03Entity*)>(&Enemy03Entity::Behavior00)}
        };
    }

    std::vector<EnemySubroutine<Enemy03Entity>> Enemy03Subroutines{
        EnemySubroutine<Enemy03Entity>(Enemy03State0),
        EnemySubroutine<Enemy03Entity>(Enemy03State1),
        EnemySubroutine<Enemy03Entity>(Enemy03State2)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy04Entity>> Enemy04State0{
            {1, static_cast<bool(*)(Enemy04Entity*)>(&Enemy04Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy04Entity>> Enemy04State1{
            {1, static_cast<bool(*)(Enemy04Entity*)>(&Enemy04Entity::Behavior00)}
        };
    }

    std::vector<EnemySubroutine<Enemy04Entity>> Enemy04Subroutines{
        EnemySubroutine<Enemy04Entity>(Enemy04State0),
        EnemySubroutine<Enemy04Entity>(Enemy04State1)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy05Entity>> Enemy05State0{
            {1, static_cast<bool(*)(Enemy05Entity*)>(&Enemy05Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy05Entity>> Enemy05State1{
            {1, static_cast<bool(*)(Enemy05Entity*)>(&Enemy05Entity::Behavior00)}
        };
    }

    std::vector<EnemySubroutine<Enemy05Entity>> Enemy05Subroutines{
        EnemySubroutine<Enemy05Entity>(Enemy05State0),
        EnemySubroutine<Enemy05Entity>(Enemy05State1)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy06Entity>> Enemy06State0{
            {1, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior02)},
            {2, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy06Entity>> Enemy06State1{
            {2, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy06Entity>> Enemy06State2{
            {3, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior03)},
            {4, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy06Entity>> Enemy06State3{
            {4, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy06Entity>> Enemy06State4{
            {1, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior02)},
            {2, static_cast<bool(*)(Enemy06Entity*)>(&Enemy06Entity::Behavior00)}
        };
    }

    std::vector<EnemySubroutine<Enemy06Entity>> Enemy06Subroutines{
        EnemySubroutine<Enemy06Entity>(Enemy06State0),
        EnemySubroutine<Enemy06Entity>(Enemy06State1),
        EnemySubroutine<Enemy06Entity>(Enemy06State2),
        EnemySubroutine<Enemy06Entity>(Enemy06State3),
        EnemySubroutine<Enemy06Entity>(Enemy06State4)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy10Entity>> Enemy10State0{
            {0, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior01)},
            {1, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy10Entity>> Enemy10State1{
            {1, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior01)},
            {2, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior06)},
            {4, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior07)},
            {4, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior08)},
            {4, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior09)}
        };
        const std::vector<EnemyBehavior<Enemy10Entity>> Enemy10State2{
            {3, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy10Entity>> Enemy10State3{
            {4, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior04)},
            {2, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy10Entity>> Enemy10State4{
            {0, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy10Entity>> Enemy10State5{
            {1, static_cast<bool(*)(Enemy10Entity*)>(&Enemy10Entity::Behavior02)}
        };
    }

    std::vector<EnemySubroutine<Enemy10Entity>> Enemy10Subroutines{
        EnemySubroutine<Enemy10Entity>(Enemy10State0),
        EnemySubroutine<Enemy10Entity>(Enemy10State1),
        EnemySubroutine<Enemy10Entity>(Enemy10State2),
        EnemySubroutine<Enemy10Entity>(Enemy10State3),
        EnemySubroutine<Enemy10Entity>(Enemy10State4),
        EnemySubroutine<Enemy10Entity>(Enemy10State5)
    };

    std::vector<Entities::Enemies::Enemy10Values> Enemy10Values{
        []() {
            Entities::Enemies::Enemy10Values values{};
            values.HealthMax = 50;
            values.BeamDamage = 3;
            values.SplashDamage = 0;
            values.ContactDamage = 15;
            values.StepDistance1 = 1024;
            values.StepDistance2 = 819;
            values.StepDistance3 = 2457;
            values.CircleIncrement = 6144;
            values.Unknown18 = 0x3C001E;
            values.MinShots = 1;
            values.MaxShots = 2;
            values.ScanId = 215;
            values.Effectiveness = 0xEABA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy10Values values{};
            values.HealthMax = 120;
            values.BeamDamage = 10;
            values.SplashDamage = 2;
            values.ContactDamage = 10;
            values.StepDistance1 = 1433;
            values.StepDistance2 = 1638;
            values.StepDistance3 = 2457;
            values.CircleIncrement = 6144;
            values.Unknown18 = 0x3C001E;
            values.MinShots = 1;
            values.MaxShots = 3;
            values.ScanId = 191;
            values.Effectiveness = 0xCEAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy10Values values{};
            values.HealthMax = 120;
            values.BeamDamage = 8;
            values.SplashDamage = 0;
            values.ContactDamage = 10;
            values.StepDistance1 = 614;
            values.StepDistance2 = 409;
            values.StepDistance3 = 1024;
            values.CircleIncrement = 6144;
            values.Unknown18 = 0x3C001E;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.ScanId = 192;
            values.Effectiveness = 0xF2AA;
            return values;
        }()
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy11Entity>> Enemy11State0{
            {1, static_cast<bool(*)(Enemy11Entity*)>(&Enemy11Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy11Entity>> Enemy11State1{
            {2, static_cast<bool(*)(Enemy11Entity*)>(&Enemy11Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy11Entity>> Enemy11State2{
            {3, static_cast<bool(*)(Enemy11Entity*)>(&Enemy11Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy11Entity>> Enemy11State3{
            {4, static_cast<bool(*)(Enemy11Entity*)>(&Enemy11Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy11Entity>> Enemy11State4{
            {0, static_cast<bool(*)(Enemy11Entity*)>(&Enemy11Entity::Behavior00)}
        };
    }

    std::vector<EnemySubroutine<Enemy11Entity>> Enemy11Subroutines{
        EnemySubroutine<Enemy11Entity>(Enemy11State0),
        EnemySubroutine<Enemy11Entity>(Enemy11State1),
        EnemySubroutine<Enemy11Entity>(Enemy11State2),
        EnemySubroutine<Enemy11Entity>(Enemy11State3),
        EnemySubroutine<Enemy11Entity>(Enemy11State4)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy16Entity>> Enemy16State0{
            {1, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior06)},
            {2, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior05)},
            {3, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy16Entity>> Enemy16State1{
            {0, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior04)},
            {2, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior05)},
            {3, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy16Entity>> Enemy16State2{
            {0, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior02)},
            {3, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy16Entity>> Enemy16State3{
            {3, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior00)},
            {0, static_cast<bool(*)(Enemy16Entity*)>(&Enemy16Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy16Entity>> Enemy16Subroutines{
        EnemySubroutine<Enemy16Entity>(Enemy16State0),
        EnemySubroutine<Enemy16Entity>(Enemy16State1),
        EnemySubroutine<Enemy16Entity>(Enemy16State2),
        EnemySubroutine<Enemy16Entity>(Enemy16State3)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy18Entity>> Enemy18State0{
            {1, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy18Entity>> Enemy18State1{
            {2, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior04)},
            {4, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy18Entity>> Enemy18State2{
            {3, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior05)},
            {4, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy18Entity>> Enemy18State3{
            {4, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior02)},
            {2, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy18Entity>> Enemy18State4{
            {0, static_cast<bool(*)(Enemy18Entity*)>(&Enemy18Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy18Entity>> Enemy18Subroutines{
        EnemySubroutine<Enemy18Entity>(Enemy18State0),
        EnemySubroutine<Enemy18Entity>(Enemy18State1),
        EnemySubroutine<Enemy18Entity>(Enemy18State2),
        EnemySubroutine<Enemy18Entity>(Enemy18State3),
        EnemySubroutine<Enemy18Entity>(Enemy18State4)
    };

    std::vector<Entities::Enemies::Enemy18Values> Enemy18Values{
        []() {
            Entities::Enemies::Enemy18Values values{};
            values.HealthMax = 24;
            values.BeamDamage = 2;
            values.SplashDamage = 0;
            values.ContactDamage = 5;
            values.MinAngleY = -184320;
            values.MaxAngleY = 184320;
            values.AngleIncY = 4096;
            values.MinAngleX = 0;
            values.MaxAngleX = 245760;
            values.AngleIncX = 4096;
            values.ShotCooldown = 5;
            values.DelayTime = 40;
            values.MinShots = 1;
            values.MaxShots = 2;
            values.Unused28 = 214;
            values.Unused2C = 4090;
            values.Unused2E = 0;
            values.ShotOffset = 4096;
            values.ScanId = 219;
            values.Effectiveness = 0xEAAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy18Values values{};
            values.HealthMax = 80;
            values.BeamDamage = 4;
            values.SplashDamage = 2;
            values.ContactDamage = 5;
            values.MinAngleY = -184320;
            values.MaxAngleY = 184320;
            values.AngleIncY = 4096;
            values.MinAngleX = 0;
            values.MaxAngleX = 245760;
            values.AngleIncX = 4096;
            values.ShotCooldown = 3;
            values.DelayTime = 30;
            values.MinShots = 3;
            values.MaxShots = 5;
            values.Unused28 = 214;
            values.Unused2C = 4090;
            values.Unused2E = 0;
            values.ShotOffset = 4096;
            values.ScanId = 196;
            values.Effectiveness = 0xEABA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy18Values values{};
            values.HealthMax = 120;
            values.BeamDamage = 50;
            values.SplashDamage = 0;
            values.ContactDamage = 5;
            values.MinAngleY = -184320;
            values.MaxAngleY = 184320;
            values.AngleIncY = 4096;
            values.MinAngleX = 0;
            values.MaxAngleX = 245760;
            values.AngleIncX = 4096;
            values.ShotCooldown = 3;
            values.DelayTime = 90;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.Unused28 = 214;
            values.Unused2C = 4090;
            values.Unused2E = 0;
            values.ShotOffset = 4096;
            values.ScanId = 197;
            values.Effectiveness = 0xEABA;
            return values;
        }()
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy45Entity>> Enemy45State0{
            {1, static_cast<bool(*)(Enemy45Entity*)>(&Enemy45Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy45Entity>> Enemy45State1{
            {2, static_cast<bool(*)(Enemy45Entity*)>(&Enemy45Entity::Behavior04)},
            {0, static_cast<bool(*)(Enemy45Entity*)>(&Enemy45Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy45Entity>> Enemy45State2{
            {0, static_cast<bool(*)(Enemy45Entity*)>(&Enemy45Entity::Behavior02)},
            {1, static_cast<bool(*)(Enemy45Entity*)>(&Enemy45Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy45Entity>> Enemy45State3{
            {0, static_cast<bool(*)(Enemy45Entity*)>(&Enemy45Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy45Entity>> Enemy45Subroutines{
        EnemySubroutine<Enemy45Entity>(Enemy45State0),
        EnemySubroutine<Enemy45Entity>(Enemy45State1),
        EnemySubroutine<Enemy45Entity>(Enemy45State2),
        EnemySubroutine<Enemy45Entity>(Enemy45State3)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State0{
            {1, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State1{
            {4, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State2{
            {3, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior07)},
            {4, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State3{
            {1, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior08)},
            {4, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State4{
            {5, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State5{
            {7, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior09)},
            {9, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior10)},
            {6, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State6{
            {7, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior09)},
            {9, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior10)},
            {5, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior11)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State7{
            {8, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State8{
            {10, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State9{
            {1, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State10{
            {13, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State11{
            {13, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State12{
            {13, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State13{
            {14, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State14{
            {16, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior09)},
            {18, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior10)},
            {15, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State15{
            {16, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior09)},
            {18, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior10)},
            {14, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State16{
            {17, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State17{
            {19, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State18{
            {10, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State19{
            {22, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State20{
            {22, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State21{
            {22, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State22{
            {23, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State23{
            {23, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior09)},
            {25, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior10)},
            {24, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State24{
            {23, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior09)},
            {25, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior10)},
            {23, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State25{
            {19, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy19Entity>> Enemy19State26{
            {23, static_cast<bool(*)(Enemy19Entity*)>(&Enemy19Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy19Entity>> Enemy19Subroutines{
        EnemySubroutine<Enemy19Entity>(Enemy19State0),
        EnemySubroutine<Enemy19Entity>(Enemy19State1),
        EnemySubroutine<Enemy19Entity>(Enemy19State2),
        EnemySubroutine<Enemy19Entity>(Enemy19State3),
        EnemySubroutine<Enemy19Entity>(Enemy19State4),
        EnemySubroutine<Enemy19Entity>(Enemy19State5),
        EnemySubroutine<Enemy19Entity>(Enemy19State6),
        EnemySubroutine<Enemy19Entity>(Enemy19State7),
        EnemySubroutine<Enemy19Entity>(Enemy19State8),
        EnemySubroutine<Enemy19Entity>(Enemy19State9),
        EnemySubroutine<Enemy19Entity>(Enemy19State10),
        EnemySubroutine<Enemy19Entity>(Enemy19State11),
        EnemySubroutine<Enemy19Entity>(Enemy19State12),
        EnemySubroutine<Enemy19Entity>(Enemy19State13),
        EnemySubroutine<Enemy19Entity>(Enemy19State14),
        EnemySubroutine<Enemy19Entity>(Enemy19State15),
        EnemySubroutine<Enemy19Entity>(Enemy19State16),
        EnemySubroutine<Enemy19Entity>(Enemy19State17),
        EnemySubroutine<Enemy19Entity>(Enemy19State18),
        EnemySubroutine<Enemy19Entity>(Enemy19State19),
        EnemySubroutine<Enemy19Entity>(Enemy19State20),
        EnemySubroutine<Enemy19Entity>(Enemy19State21),
        EnemySubroutine<Enemy19Entity>(Enemy19State22),
        EnemySubroutine<Enemy19Entity>(Enemy19State23),
        EnemySubroutine<Enemy19Entity>(Enemy19State24),
        EnemySubroutine<Enemy19Entity>(Enemy19State25),
        EnemySubroutine<Enemy19Entity>(Enemy19State26)
    };

    std::vector<Entities::Enemies::Enemy19Values> Enemy19Values{
        []() {
            Entities::Enemies::Enemy19Values values{};
            values.CrystalHealth = 490;
            values.PhaseFlashTime = 60;
            values.Phase0CrystalHealth = 360;
            values.Phase1CrystalHealth = 200;
            values.Phase2CrystalHealth = 0;
            values.Phase0CrystalShotTime = 30;
            values.Phase1CrystalShotTime = 20;
            values.Phase2CrystalShotTime = 13;
            values.Phase0CrystalShotDelay = 1;
            values.Phase1CrystalShotDelay = 1;
            values.Phase2CrystalShotDelay = 1;
            values.Phase0CrystalUpTime = 150;
            values.Phase1CrystalUpTime = 120;
            values.Phase2CrystalUpTime = 90;
            values.CrystalBeamDamage = MakeManagedArray<std::uint16_t>({ 6, 6, 6 });
            values.EyeBeamDamage = MakeManagedArray<std::uint16_t>({ 3, 3, 3 });
            values.EyeSplashDamage = MakeManagedArray<std::uint16_t>({ 0, 0, 0 });
            values.EyeContactDamage = MakeManagedArray<std::uint16_t>({ 3, 3, 3 });
            values.Unused34 = 3547;
            values.Unused38 = 3547;
            values.Unused3C = 3547;
            values.Seg0AngleStep = 2457;
            values.Seg1AngleStep = 2048;
            values.Seg2AngleStep = 3686;
            values.Seg0BeamStartAngle = 163840;
            values.Seg1BeamStartAngle = 143360;
            values.Seg2BeamStartAngle = 73728;
            values.Seg0BeamAngleMin = 122880;
            values.Seg1BeamAngleMin = 81920;
            values.Seg2BeamAngleMin = -40960;
            values.Seg0BeamAngleMax = 327680;
            values.Seg1BeamAngleMax = 307200;
            values.Seg2BeamAngleMax = 225280;
            values.Seg0BeamAngleStep = 3481;
            values.Seg1BeamAngleStep = 3072;
            values.Seg2BeamAngleStep = 4096;
            values.EyeHealth = 12;
            values.ItemChanceHealth = 8;
            values.ItemChanceMissile = 5;
            values.ItemChanceUa = 0;
            values.ItemChanceNone = 87;
            values.Phase0EyeState = MakeManagedArray<std::uint8_t>({ 5, 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2 });
            values.Phase0BeamType = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 });
            values.Phase0BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 });
            values.Phase0BeamCooldown = MakeManagedArray<std::uint16_t>({ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 });
            values.Phase0EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 });
            values.Phase0EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 });
            values.Phase0EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200 });
            values.Phase0EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 });
            values.Phase1EyeState = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 5, 5, 5, 5, 2, 2, 2, 2, 2 });
            values.Phase1BeamType = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 });
            values.Phase1BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 });
            values.Phase1BeamCooldown = MakeManagedArray<std::uint16_t>({ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 });
            values.Phase1EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 });
            values.Phase1EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 });
            values.Phase1EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280 });
            values.Phase1EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 });
            values.Phase2EyeState = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 5 });
            values.Phase2BeamType = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 });
            values.Phase2BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 });
            values.Phase2BeamCooldown = MakeManagedArray<std::uint16_t>({ 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 });
            values.Phase2EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 });
            values.Phase2EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 });
            values.Phase2EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360, 360 });
            values.Phase2EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 });
            values.ItemChanceA = 0;
            values.ItemChanceB = 0;
            values.ItemChanceC = 0;
            values.ItemChanceD = 0;
            values.Padding27E = 0;
            values.CollisionRadius = 3276;
            values.ScanId = 0;
            values.CrystalScanId = 226;
            values.CrystalEffectiveness = 0xAAAA;
            values.EyeScanId = 0;
            values.EyeEffectiveness = 0xAA9A;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy19Values values{};
            values.CrystalHealth = 540;
            values.PhaseFlashTime = 60;
            values.Phase0CrystalHealth = 400;
            values.Phase1CrystalHealth = 230;
            values.Phase2CrystalHealth = 0;
            values.Phase0CrystalShotTime = 15;
            values.Phase1CrystalShotTime = 12;
            values.Phase2CrystalShotTime = 9;
            values.Phase0CrystalShotDelay = 10;
            values.Phase1CrystalShotDelay = 10;
            values.Phase2CrystalShotDelay = 10;
            values.Phase0CrystalUpTime = 150;
            values.Phase1CrystalUpTime = 120;
            values.Phase2CrystalUpTime = 90;
            values.CrystalBeamDamage = MakeManagedArray<std::uint16_t>({ 10, 10, 10 });
            values.EyeBeamDamage = MakeManagedArray<std::uint16_t>({ 5, 5, 5 });
            values.EyeSplashDamage = MakeManagedArray<std::uint16_t>({ 0, 0, 0 });
            values.EyeContactDamage = MakeManagedArray<std::uint16_t>({ 3, 3, 3 });
            values.Unused34 = 3547;
            values.Unused38 = 3547;
            values.Unused3C = 3547;
            values.Seg0AngleStep = 2457;
            values.Seg1AngleStep = 2048;
            values.Seg2AngleStep = 3686;
            values.Seg0BeamStartAngle = 163840;
            values.Seg1BeamStartAngle = 143360;
            values.Seg2BeamStartAngle = 73728;
            values.Seg0BeamAngleMin = 122880;
            values.Seg1BeamAngleMin = 81920;
            values.Seg2BeamAngleMin = 40960;
            values.Seg0BeamAngleMax = 245760;
            values.Seg1BeamAngleMax = 204800;
            values.Seg2BeamAngleMax = 143360;
            values.Seg0BeamAngleStep = 3072;
            values.Seg1BeamAngleStep = 4096;
            values.Seg2BeamAngleStep = 4096;
            values.EyeHealth = 4;
            values.ItemChanceHealth = 10;
            values.ItemChanceMissile = 5;
            values.ItemChanceUa = 5;
            values.ItemChanceNone = 80;
            values.Phase0EyeState = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4 });
            values.Phase0BeamType = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamCooldown = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase0EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130 });
            values.Phase0EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50 });
            values.Phase0EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase0EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50 });
            values.Phase1EyeState = MakeManagedArray<std::uint8_t>({ 0, 0, 0, 4, 4, 4, 4, 1, 1, 1, 1, 1 });
            values.Phase1BeamType = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamCooldown = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase1EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130 });
            values.Phase1EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45 });
            values.Phase1EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase1EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45 });
            values.Phase2EyeState = MakeManagedArray<std::uint8_t>({ 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1 });
            values.Phase2BeamType = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamCooldown = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase2EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 60, 90, 30, 60, 90, 120, 30, 60, 90, 120, 90, 30 });
            values.Phase2EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45 });
            values.Phase2EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase2EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45 });
            values.ItemChanceA = 0;
            values.ItemChanceB = 0;
            values.ItemChanceC = 0;
            values.ItemChanceD = 0;
            values.Padding27E = 0;
            values.CollisionRadius = 3276;
            values.ScanId = 0;
            values.CrystalScanId = 198;
            values.CrystalEffectiveness = 0xD5EA;
            values.EyeScanId = 0;
            values.EyeEffectiveness = 0xAAAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy19Values values{};
            values.CrystalHealth = 570;
            values.PhaseFlashTime = 60;
            values.Phase0CrystalHealth = 420;
            values.Phase1CrystalHealth = 230;
            values.Phase2CrystalHealth = 0;
            values.Phase0CrystalShotTime = 15;
            values.Phase1CrystalShotTime = 12;
            values.Phase2CrystalShotTime = 9;
            values.Phase0CrystalShotDelay = 10;
            values.Phase1CrystalShotDelay = 10;
            values.Phase2CrystalShotDelay = 10;
            values.Phase0CrystalUpTime = 150;
            values.Phase1CrystalUpTime = 120;
            values.Phase2CrystalUpTime = 90;
            values.CrystalBeamDamage = MakeManagedArray<std::uint16_t>({ 12, 12, 12 });
            values.EyeBeamDamage = MakeManagedArray<std::uint16_t>({ 4, 4, 4 });
            values.EyeSplashDamage = MakeManagedArray<std::uint16_t>({ 1, 1, 1 });
            values.EyeContactDamage = MakeManagedArray<std::uint16_t>({ 8, 8, 8 });
            values.Unused34 = 3547;
            values.Unused38 = 3547;
            values.Unused3C = 3547;
            values.Seg0AngleStep = 2457;
            values.Seg1AngleStep = 2048;
            values.Seg2AngleStep = 3686;
            values.Seg0BeamStartAngle = 163840;
            values.Seg1BeamStartAngle = 143360;
            values.Seg2BeamStartAngle = 73728;
            values.Seg0BeamAngleMin = 122880;
            values.Seg1BeamAngleMin = 81920;
            values.Seg2BeamAngleMin = -40960;
            values.Seg0BeamAngleMax = 327680;
            values.Seg1BeamAngleMax = 307200;
            values.Seg2BeamAngleMax = 225280;
            values.Seg0BeamAngleStep = 3481;
            values.Seg1BeamAngleStep = 3072;
            values.Seg2BeamAngleStep = 4096;
            values.EyeHealth = 4;
            values.ItemChanceHealth = 8;
            values.ItemChanceMissile = 5;
            values.ItemChanceUa = 5;
            values.ItemChanceNone = 82;
            values.Phase0EyeState = MakeManagedArray<std::uint8_t>({ 0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 4, 4 });
            values.Phase0BeamType = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamCooldown = MakeManagedArray<std::uint16_t>({ 5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase0EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 130, 130, 130, 130, 130, 130, 130, 130, 130 });
            values.Phase0EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 50, 50, 50, 50, 50, 50, 50, 50, 50 });
            values.Phase0EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 180, 180, 180, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase0EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 20, 20, 20, 50, 50, 50, 50, 50, 50, 50, 50, 50 });
            values.Phase1EyeState = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4 });
            values.Phase1BeamType = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1 });
            values.Phase1BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamCooldown = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 5, 5, 5, 5, 10, 10, 10, 10, 10 });
            values.Phase1EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 130, 130, 130, 30, 30, 30, 30, 130, 130, 130, 130, 130 });
            values.Phase1EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 30, 30, 30, 30, 45, 45, 45, 45, 45 });
            values.Phase1EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 220, 220, 220, 220, 10, 10, 10, 10, 10 });
            values.Phase1EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 20, 20, 20, 20, 45, 45, 45, 45, 45 });
            values.Phase2EyeState = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 4, 4, 4, 4, 0, 0, 0, 0, 0 });
            values.Phase2BeamType = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2 });
            values.Phase2BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamCooldown = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5 });
            values.Phase2EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 40, 40, 40, 40, 40, 40, 40, 30, 30, 30, 30, 30 });
            values.Phase2EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 30, 30, 30, 30, 30 });
            values.Phase2EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 260, 260, 260, 260, 260 });
            values.Phase2EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 20, 20, 20, 20, 20 });
            values.ItemChanceA = 0;
            values.ItemChanceB = 0;
            values.ItemChanceC = 0;
            values.ItemChanceD = 0;
            values.Padding27E = 0;
            values.CollisionRadius = 3276;
            values.ScanId = 0;
            values.CrystalScanId = 199;
            values.CrystalEffectiveness = 0xD5EA;
            values.EyeScanId = 0;
            values.EyeEffectiveness = 0xAAAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy19Values values{};
            values.CrystalHealth = 550;
            values.PhaseFlashTime = 60;
            values.Phase0CrystalHealth = 385;
            values.Phase1CrystalHealth = 220;
            values.Phase2CrystalHealth = 0;
            values.Phase0CrystalShotTime = 12;
            values.Phase1CrystalShotTime = 10;
            values.Phase2CrystalShotTime = 8;
            values.Phase0CrystalShotDelay = 10;
            values.Phase1CrystalShotDelay = 10;
            values.Phase2CrystalShotDelay = 10;
            values.Phase0CrystalUpTime = 150;
            values.Phase1CrystalUpTime = 120;
            values.Phase2CrystalUpTime = 90;
            values.CrystalBeamDamage = MakeManagedArray<std::uint16_t>({ 18, 18, 18 });
            values.EyeBeamDamage = MakeManagedArray<std::uint16_t>({ 4, 4, 4 });
            values.EyeSplashDamage = MakeManagedArray<std::uint16_t>({ 1, 1, 1 });
            values.EyeContactDamage = MakeManagedArray<std::uint16_t>({ 10, 10, 10 });
            values.Unused34 = 3547;
            values.Unused38 = 3547;
            values.Unused3C = 3547;
            values.Seg0AngleStep = 2457;
            values.Seg1AngleStep = 2048;
            values.Seg2AngleStep = 3686;
            values.Seg0BeamStartAngle = 163840;
            values.Seg1BeamStartAngle = 143360;
            values.Seg2BeamStartAngle = 73728;
            values.Seg0BeamAngleMin = 122880;
            values.Seg1BeamAngleMin = 81920;
            values.Seg2BeamAngleMin = -40960;
            values.Seg0BeamAngleMax = 327680;
            values.Seg1BeamAngleMax = 307200;
            values.Seg2BeamAngleMax = 225280;
            values.Seg0BeamAngleStep = 3481;
            values.Seg1BeamAngleStep = 3072;
            values.Seg2BeamAngleStep = 4096;
            values.EyeHealth = 4;
            values.ItemChanceHealth = 8;
            values.ItemChanceMissile = 5;
            values.ItemChanceUa = 5;
            values.ItemChanceNone = 82;
            values.Phase0EyeState = MakeManagedArray<std::uint8_t>({ 0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 4, 4 });
            values.Phase0BeamType = MakeManagedArray<std::uint8_t>({ 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
            values.Phase0BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase0BeamCooldown = MakeManagedArray<std::uint16_t>({ 5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase0EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 130, 130, 130, 130, 130, 130, 130, 130, 130 });
            values.Phase0EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 30, 30, 30, 50, 50, 50, 50, 50, 50, 50, 50, 50 });
            values.Phase0EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 180, 180, 180, 10, 10, 10, 10, 10, 10, 10, 10, 10 });
            values.Phase0EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 20, 20, 20, 50, 50, 50, 50, 50, 50, 50, 50, 50 });
            values.Phase1EyeState = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4 });
            values.Phase1BeamType = MakeManagedArray<std::uint8_t>({ 0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0 });
            values.Phase1BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase1BeamCooldown = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 5, 5, 5, 5, 10, 10, 10, 10, 10 });
            values.Phase1EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 130, 130, 130, 30, 30, 30, 30, 130, 130, 130, 130, 130 });
            values.Phase1EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 30, 30, 30, 30, 45, 45, 45, 45, 45 });
            values.Phase1EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 220, 220, 220, 220, 10, 10, 10, 10, 10 });
            values.Phase1EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 20, 20, 20, 20, 45, 45, 45, 45, 45 });
            values.Phase2EyeState = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 4, 4, 4, 4, 0, 0, 0, 0, 0 });
            values.Phase2BeamType = MakeManagedArray<std::uint8_t>({ 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2 });
            values.Phase2BeamSpawnMin = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamSpawnMax = MakeManagedArray<std::uint8_t>({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 });
            values.Phase2BeamCooldown = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5 });
            values.Phase2EyeStateTimer0 = MakeManagedArray<std::uint16_t>({ 130, 130, 130, 130, 130, 130, 130, 30, 30, 30, 30, 30 });
            values.Phase2EyeStateTimer1 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 30, 30, 30, 30, 30 });
            values.Phase2EyeStateTimer2 = MakeManagedArray<std::uint16_t>({ 10, 10, 10, 10, 10, 10, 10, 260, 260, 260, 260, 260 });
            values.Phase2EyeStateTimer3 = MakeManagedArray<std::uint16_t>({ 45, 45, 45, 45, 45, 45, 45, 20, 20, 20, 20, 20 });
            values.ItemChanceA = 0;
            values.ItemChanceB = 0;
            values.ItemChanceC = 0;
            values.ItemChanceD = 0;
            values.Padding27E = 0;
            values.CollisionRadius = 3276;
            values.ScanId = 0;
            values.CrystalScanId = 200;
            values.CrystalEffectiveness = 0xD5E6;
            values.EyeScanId = 0;
            values.EyeEffectiveness = 0xAAAA;
            return values;
        }()
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State0{
            {1, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior01)},
            {2, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior07)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State1{
            {0, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State2{
            {3, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior08)},
            {1, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior09)},
            {8, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State3{
            {5, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior11)},
            {1, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior09)},
            {8, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior10)},
            {8, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State4{
            {6, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State5{
            {4, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State6{
            {2, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State7{
            {1, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State8{
            {7, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State9{
            {10, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy23Entity>> Enemy23State10{
            {1, static_cast<bool(*)(Enemy23Entity*)>(&Enemy23Entity::Behavior02)}
        };
    }

    std::vector<EnemySubroutine<Enemy23Entity>> Enemy23Subroutines{
        EnemySubroutine<Enemy23Entity>(Enemy23State0),
        EnemySubroutine<Enemy23Entity>(Enemy23State1),
        EnemySubroutine<Enemy23Entity>(Enemy23State2),
        EnemySubroutine<Enemy23Entity>(Enemy23State3),
        EnemySubroutine<Enemy23Entity>(Enemy23State4),
        EnemySubroutine<Enemy23Entity>(Enemy23State5),
        EnemySubroutine<Enemy23Entity>(Enemy23State6),
        EnemySubroutine<Enemy23Entity>(Enemy23State7),
        EnemySubroutine<Enemy23Entity>(Enemy23State8),
        EnemySubroutine<Enemy23Entity>(Enemy23State9),
        EnemySubroutine<Enemy23Entity>(Enemy23State10)
    };

    std::vector<Entities::Enemies::Enemy23Values> Enemy23Values{
        []() {
            Entities::Enemies::Enemy23Values values{};
            values.HealthMax = 11;
            values.BeamDamage = 1;
            values.SplashDamage = 0;
            values.ContactDamage = 10;
            values.MinSpeedFactor1 = 409;
            values.MaxSpeedFactor1 = 918;
            values.MinSpeedFactor2 = 1638;
            values.MaxSpeedFactor2 = 2252;
            values.RangeMaxCosine = -4096;
            values.Unknown1C = 2457;
            values.Unused20 = 40960;
            values.DelayTime = 40;
            values.ShotTime = 15;
            values.Unused28 = 1638400;
            values.MinShots = 1;
            values.MaxShots = 2;
            values.DoubleSpeedSteps = 50;
            values.AimSteps = 4;
            values.SpeedSteps = 26;
            values.ScanId = 216;
            values.Effectiveness = 0xEAAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy23Values values{};
            values.HealthMax = 24;
            values.BeamDamage = 2;
            values.SplashDamage = 1;
            values.ContactDamage = 10;
            values.MinSpeedFactor1 = 614;
            values.MaxSpeedFactor1 = 1228;
            values.MinSpeedFactor2 = 2867;
            values.MaxSpeedFactor2 = 3686;
            values.RangeMaxCosine = -4096;
            values.Unknown1C = 3276;
            values.Unused20 = 61440;
            values.DelayTime = 25;
            values.ShotTime = 8;
            values.Unused28 = 1638400;
            values.MinShots = 1;
            values.MaxShots = 3;
            values.DoubleSpeedSteps = 60;
            values.AimSteps = 4;
            values.SpeedSteps = 30;
            values.ScanId = 216;
            values.Effectiveness = 0xEABA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy23Values values{};
            values.HealthMax = 120;
            values.BeamDamage = 10;
            values.SplashDamage = 2;
            values.ContactDamage = 10;
            values.MinSpeedFactor1 = 614;
            values.MaxSpeedFactor1 = 1228;
            values.MinSpeedFactor2 = 2457;
            values.MaxSpeedFactor2 = 3276;
            values.RangeMaxCosine = -4096;
            values.Unknown1C = 4096;
            values.Unused20 = 40960;
            values.DelayTime = 25;
            values.ShotTime = 40;
            values.Unused28 = 1638400;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.DoubleSpeedSteps = 60;
            values.AimSteps = 4;
            values.SpeedSteps = 30;
            values.ScanId = 208;
            values.Effectiveness = 0xEAF2;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy23Values values{};
            values.HealthMax = 120;
            values.BeamDamage = 8;
            values.SplashDamage = 2;
            values.ContactDamage = 10;
            values.MinSpeedFactor1 = 614;
            values.MaxSpeedFactor1 = 1228;
            values.MinSpeedFactor2 = 1638;
            values.MaxSpeedFactor2 = 2048;
            values.RangeMaxCosine = -4096;
            values.Unknown1C = 2048;
            values.Unused20 = 40960;
            values.DelayTime = 25;
            values.ShotTime = 30;
            values.Unused28 = 1638400;
            values.MinShots = 1;
            values.MaxShots = 2;
            values.DoubleSpeedSteps = 20;
            values.AimSteps = 30;
            values.SpeedSteps = 10;
            values.ScanId = 209;
            values.Effectiveness = 0xCEF9;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy23Values values{};
            values.HealthMax = 120;
            values.BeamDamage = 8;
            values.SplashDamage = 0;
            values.ContactDamage = 10;
            values.MinSpeedFactor1 = 614;
            values.MaxSpeedFactor1 = 1228;
            values.MinSpeedFactor2 = 1638;
            values.MaxSpeedFactor2 = 2048;
            values.RangeMaxCosine = -4096;
            values.Unknown1C = 4096;
            values.Unused20 = 61440;
            values.DelayTime = 25;
            values.ShotTime = 40;
            values.Unused28 = 1638400;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.DoubleSpeedSteps = 20;
            values.AimSteps = 30;
            values.SpeedSteps = 10;
            values.ScanId = 207;
            values.Effectiveness = 0xF2F9;
            return values;
        }()
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State0{
            {1, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State1{
            {3, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior11)},
            {2, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior18)},
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {8, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior09)},
            {12, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior10)},
            {4, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior19)},
            {10, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior20)},
            {7, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior21)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State2{
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {3, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior11)},
            {8, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior09)},
            {12, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior10)},
            {4, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior22)},
            {10, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior20)},
            {7, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior21)},
            {4, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior07)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State3{
            {14, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior07)},
            {14, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior08)},
            {8, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior09)},
            {12, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State4{
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {3, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior11)},
            {8, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior09)},
            {12, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior10)},
            {14, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State5{
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {3, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior13)},
            {8, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior14)},
            {12, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior10)},
            {1, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior15)},
            {14, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior16)},
            {11, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior17)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State6{
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {5, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State7{
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State8{
            {14, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State9{
            {13, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior06)},
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State10{
            {9, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State11{
            {14, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State12{
            {2, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State13{
            {0, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy24Entity>> Enemy24State14{
            {14, static_cast<bool(*)(Enemy24Entity*)>(&Enemy24Entity::Behavior02)}
        };
    }

    std::vector<EnemySubroutine<Enemy24Entity>> Enemy24Subroutines{
        EnemySubroutine<Enemy24Entity>(Enemy24State0),
        EnemySubroutine<Enemy24Entity>(Enemy24State1),
        EnemySubroutine<Enemy24Entity>(Enemy24State2),
        EnemySubroutine<Enemy24Entity>(Enemy24State3),
        EnemySubroutine<Enemy24Entity>(Enemy24State4),
        EnemySubroutine<Enemy24Entity>(Enemy24State5),
        EnemySubroutine<Enemy24Entity>(Enemy24State6),
        EnemySubroutine<Enemy24Entity>(Enemy24State7),
        EnemySubroutine<Enemy24Entity>(Enemy24State8),
        EnemySubroutine<Enemy24Entity>(Enemy24State9),
        EnemySubroutine<Enemy24Entity>(Enemy24State10),
        EnemySubroutine<Enemy24Entity>(Enemy24State11),
        EnemySubroutine<Enemy24Entity>(Enemy24State12),
        EnemySubroutine<Enemy24Entity>(Enemy24State13),
        EnemySubroutine<Enemy24Entity>(Enemy24State14)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State0{
            {1, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::BehaviorXX)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State1{
            {2, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State2{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {3, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior13)},
            {4, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior14)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State3{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {4, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior14)},
            {2, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior15)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State4{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {5, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State5{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {11, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior06)},
            {6, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior07)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State6{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {10, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior08)},
            {7, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior09)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State7{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {10, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior08)},
            {8, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State8{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {10, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior08)},
            {9, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior11)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State9{
            {13, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {10, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior08)},
            {11, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State10{
            {11, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State11{
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior02)},
            {12, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior03)},
            {2, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State12{
            {0, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy28Entity>> Enemy28State13{
            {2, static_cast<bool(*)(Enemy28Entity*)>(&Enemy28Entity::Behavior00)}
        };
    }

    std::vector<EnemySubroutine<Enemy28Entity>> Enemy28Subroutines{
        EnemySubroutine<Enemy28Entity>(Enemy28State0),
        EnemySubroutine<Enemy28Entity>(Enemy28State1),
        EnemySubroutine<Enemy28Entity>(Enemy28State2),
        EnemySubroutine<Enemy28Entity>(Enemy28State3),
        EnemySubroutine<Enemy28Entity>(Enemy28State4),
        EnemySubroutine<Enemy28Entity>(Enemy28State5),
        EnemySubroutine<Enemy28Entity>(Enemy28State6),
        EnemySubroutine<Enemy28Entity>(Enemy28State7),
        EnemySubroutine<Enemy28Entity>(Enemy28State8),
        EnemySubroutine<Enemy28Entity>(Enemy28State9),
        EnemySubroutine<Enemy28Entity>(Enemy28State10),
        EnemySubroutine<Enemy28Entity>(Enemy28State11),
        EnemySubroutine<Enemy28Entity>(Enemy28State12),
        EnemySubroutine<Enemy28Entity>(Enemy28State13)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State0{
            {1, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::BehaviorXX)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State1{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {2, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior12)},
            {9, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior13)},
            {6, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior14)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State2{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {3, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior11)},
            {9, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior13)},
            {6, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior14)},
            {7, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior17)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State3{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {9, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior13)},
            {6, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior14)},
            {4, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior07)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State4{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {9, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior13)},
            {7, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior09)},
            {5, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State5{
            {1, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State6{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {18, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::BehaviorXX)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State7{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {18, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State8{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {11, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior15)},
            {6, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior14)},
            {1, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior16)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State9{
            {8, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::BehaviorXX)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State10{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {11, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior11)},
            {8, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State11{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {12, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior07)},
            {8, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State12{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {14, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior04)},
            {15, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior05)},
            {16, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior06)},
            {7, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior09)},
            {13, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State13{
            {8, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State14{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {18, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::BehaviorXX)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State15{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior02)},
            {18, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State16{
            {18, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::BehaviorXX)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State17{
            {17, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy31Entity>> Enemy31State18{
            {18, static_cast<bool(*)(Enemy31Entity*)>(&Enemy31Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy31Entity>> Enemy31Subroutines{
        EnemySubroutine<Enemy31Entity>(Enemy31State0),
        EnemySubroutine<Enemy31Entity>(Enemy31State1),
        EnemySubroutine<Enemy31Entity>(Enemy31State2),
        EnemySubroutine<Enemy31Entity>(Enemy31State3),
        EnemySubroutine<Enemy31Entity>(Enemy31State4),
        EnemySubroutine<Enemy31Entity>(Enemy31State5),
        EnemySubroutine<Enemy31Entity>(Enemy31State6),
        EnemySubroutine<Enemy31Entity>(Enemy31State7),
        EnemySubroutine<Enemy31Entity>(Enemy31State8),
        EnemySubroutine<Enemy31Entity>(Enemy31State9),
        EnemySubroutine<Enemy31Entity>(Enemy31State10),
        EnemySubroutine<Enemy31Entity>(Enemy31State11),
        EnemySubroutine<Enemy31Entity>(Enemy31State12),
        EnemySubroutine<Enemy31Entity>(Enemy31State13),
        EnemySubroutine<Enemy31Entity>(Enemy31State14),
        EnemySubroutine<Enemy31Entity>(Enemy31State15),
        EnemySubroutine<Enemy31Entity>(Enemy31State16),
        EnemySubroutine<Enemy31Entity>(Enemy31State17),
        EnemySubroutine<Enemy31Entity>(Enemy31State18)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy33Entity>> Enemy33State0{
            {3, static_cast<bool(*)(Enemy33Entity*)>(&Enemy33Entity::Behavior02)},
            {2, static_cast<bool(*)(Enemy33Entity*)>(&Enemy33Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy33Entity>> Enemy33State1{
            {0, static_cast<bool(*)(Enemy33Entity*)>(&Enemy33Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy33Entity>> Enemy33State2{
            {3, static_cast<bool(*)(Enemy33Entity*)>(&Enemy33Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy33Entity>> Enemy33State3{
            {0, static_cast<bool(*)(Enemy33Entity*)>(&Enemy33Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy33Entity>> Enemy33Subroutines{
        EnemySubroutine<Enemy33Entity>(Enemy33State0),
        EnemySubroutine<Enemy33Entity>(Enemy33State1),
        EnemySubroutine<Enemy33Entity>(Enemy33State2),
        EnemySubroutine<Enemy33Entity>(Enemy33State3)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy35Entity>> Enemy35State0{
            {1, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior02)},
            {2, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy35Entity>> Enemy35State1{
            {0, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy35Entity>> Enemy35State2{
            {3, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy35Entity>> Enemy35State3{
            {4, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy35Entity>> Enemy35State4{
            {5, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy35Entity>> Enemy35State5{
            {6, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy35Entity>> Enemy35State6{
            {1, static_cast<bool(*)(Enemy35Entity*)>(&Enemy35Entity::Behavior02)}
        };
    }

    std::vector<EnemySubroutine<Enemy35Entity>> Enemy35Subroutines{
        EnemySubroutine<Enemy35Entity>(Enemy35State0),
        EnemySubroutine<Enemy35Entity>(Enemy35State1),
        EnemySubroutine<Enemy35Entity>(Enemy35State2),
        EnemySubroutine<Enemy35Entity>(Enemy35State3),
        EnemySubroutine<Enemy35Entity>(Enemy35State4),
        EnemySubroutine<Enemy35Entity>(Enemy35State5),
        EnemySubroutine<Enemy35Entity>(Enemy35State6)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy36Entity>> Enemy36State0{
            {1, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior03)},
            {2, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior05)},
            {1, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy36Entity>> Enemy36State1{
            {0, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy36Entity>> Enemy36State2{
            {3, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy36Entity>> Enemy36State3{
            {4, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy36Entity>> Enemy36State4{
            {5, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy36Entity>> Enemy36State5{
            {1, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior03)},
            {1, static_cast<bool(*)(Enemy36Entity*)>(&Enemy36Entity::Behavior04)}
        };
    }

    std::vector<EnemySubroutine<Enemy36Entity>> Enemy36Subroutines{
        EnemySubroutine<Enemy36Entity>(Enemy36State0),
        EnemySubroutine<Enemy36Entity>(Enemy36State1),
        EnemySubroutine<Enemy36Entity>(Enemy36State2),
        EnemySubroutine<Enemy36Entity>(Enemy36State3),
        EnemySubroutine<Enemy36Entity>(Enemy36State4),
        EnemySubroutine<Enemy36Entity>(Enemy36State5)
    };

    std::vector<Entities::Enemies::Enemy36Values> Enemy36Values{
        []() {
            Entities::Enemies::Enemy36Values values{};
            values.HealthMax = 55;
            values.BeamDamage = 2;
            values.SplashDamage = 0;
            values.ContactDamage = 7;
            values.MinSpeedFactor = 819;
            values.MaxSpeedFactor = 1433;
            values.DoubleSpeedSteps = 15;
            values.AimSteps = 10;
            values.DelayTime = 40;
            values.ShotTime = 15;
            values.MinShots = 1;
            values.MaxShots = 2;
            values.JumpSpeed = 819;
            values.RangeMaxCosine = -4096;
            values.SpeedSteps = 7;
            values.ScanId = 217;
            values.Effectiveness = 0xEABA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy36Values values{};
            values.HealthMax = 100;
            values.BeamDamage = 5;
            values.SplashDamage = 1;
            values.ContactDamage = 7;
            values.MinSpeedFactor = 819;
            values.MaxSpeedFactor = 1433;
            values.DoubleSpeedSteps = 15;
            values.AimSteps = 10;
            values.DelayTime = 25;
            values.ShotTime = 8;
            values.MinShots = 2;
            values.MaxShots = 3;
            values.JumpSpeed = 819;
            values.RangeMaxCosine = -4096;
            values.SpeedSteps = 7;
            values.ScanId = 217;
            values.Effectiveness = 0xEABA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy36Values values{};
            values.HealthMax = 150;
            values.BeamDamage = 10;
            values.SplashDamage = 2;
            values.ContactDamage = 7;
            values.MinSpeedFactor = 819;
            values.MaxSpeedFactor = 1433;
            values.DoubleSpeedSteps = 15;
            values.AimSteps = 10;
            values.DelayTime = 30;
            values.ShotTime = 25;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.JumpSpeed = 819;
            values.RangeMaxCosine = -4096;
            values.SpeedSteps = 7;
            values.ScanId = 193;
            values.Effectiveness = 0xEAB2;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy36Values values{};
            values.HealthMax = 150;
            values.BeamDamage = 8;
            values.SplashDamage = 2;
            values.ContactDamage = 7;
            values.MinSpeedFactor = 1228;
            values.MaxSpeedFactor = 2048;
            values.DoubleSpeedSteps = 15;
            values.AimSteps = 10;
            values.DelayTime = 25;
            values.ShotTime = 20;
            values.MinShots = 1;
            values.MaxShots = 2;
            values.JumpSpeed = 819;
            values.RangeMaxCosine = -4096;
            values.SpeedSteps = 7;
            values.ScanId = 194;
            values.Effectiveness = 0xCEBA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy36Values values{};
            values.HealthMax = 152;
            values.BeamDamage = 8;
            values.SplashDamage = 0;
            values.ContactDamage = 7;
            values.MinSpeedFactor = 409;
            values.MaxSpeedFactor = 1024;
            values.DoubleSpeedSteps = 15;
            values.AimSteps = 10;
            values.DelayTime = 25;
            values.ShotTime = 40;
            values.MinShots = 1;
            values.MaxShots = 2;
            values.JumpSpeed = 819;
            values.RangeMaxCosine = -4096;
            values.SpeedSteps = 7;
            values.ScanId = 195;
            values.Effectiveness = 0xF2BA;
            return values;
        }()
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State0{
            {1, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State1{
            {2, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State2{
            {4, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior13)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State3{
            {5, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior14)},
            {6, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior15)},
            {4, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior16)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State4{
            {3, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State5{
            {12, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior11)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State6{
            {7, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State7{
            {8, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior09)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State8{
            {9, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State9{
            {10, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State10{
            {11, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State11{
            {12, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State12{
            {15, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior17)},
            {13, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior16)},
            {14, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior18)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State13{
            {12, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State14{
            {3, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior07)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State15{
            {16, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy38Entity>> Enemy38State16{
            {0, static_cast<bool(*)(Enemy38Entity*)>(&Enemy38Entity::Behavior02)}
        };
    }

    std::vector<EnemySubroutine<Enemy38Entity>> Enemy38Subroutines{
        EnemySubroutine<Enemy38Entity>(Enemy38State0),
        EnemySubroutine<Enemy38Entity>(Enemy38State1),
        EnemySubroutine<Enemy38Entity>(Enemy38State2),
        EnemySubroutine<Enemy38Entity>(Enemy38State3),
        EnemySubroutine<Enemy38Entity>(Enemy38State4),
        EnemySubroutine<Enemy38Entity>(Enemy38State5),
        EnemySubroutine<Enemy38Entity>(Enemy38State6),
        EnemySubroutine<Enemy38Entity>(Enemy38State7),
        EnemySubroutine<Enemy38Entity>(Enemy38State8),
        EnemySubroutine<Enemy38Entity>(Enemy38State9),
        EnemySubroutine<Enemy38Entity>(Enemy38State10),
        EnemySubroutine<Enemy38Entity>(Enemy38State11),
        EnemySubroutine<Enemy38Entity>(Enemy38State12),
        EnemySubroutine<Enemy38Entity>(Enemy38State13),
        EnemySubroutine<Enemy38Entity>(Enemy38State14),
        EnemySubroutine<Enemy38Entity>(Enemy38State15),
        EnemySubroutine<Enemy38Entity>(Enemy38State16)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy39Entity>> Enemy39State0{
            {1, static_cast<bool(*)(Enemy39Entity*)>(&Enemy39Entity::Behavior0)}
        };
        const std::vector<EnemyBehavior<Enemy39Entity>> Enemy39State1{
            {2, static_cast<bool(*)(Enemy39Entity*)>(&Enemy39Entity::Behavior4)}
        };
        const std::vector<EnemyBehavior<Enemy39Entity>> Enemy39State2{
            {3, static_cast<bool(*)(Enemy39Entity*)>(&Enemy39Entity::Behavior3)}
        };
        const std::vector<EnemyBehavior<Enemy39Entity>> Enemy39State3{
            {4, static_cast<bool(*)(Enemy39Entity*)>(&Enemy39Entity::Behavior1)}
        };
        const std::vector<EnemyBehavior<Enemy39Entity>> Enemy39State4{
            {5, static_cast<bool(*)(Enemy39Entity*)>(&Enemy39Entity::Behavior5)},
            {5, static_cast<bool(*)(Enemy39Entity*)>(&Enemy39Entity::Behavior6)}
        };
        const std::vector<EnemyBehavior<Enemy39Entity>> Enemy39State5{
            {0, static_cast<bool(*)(Enemy39Entity*)>(&Enemy39Entity::Behavior2)}
        };
    }

    std::vector<EnemySubroutine<Enemy39Entity>> Enemy39Subroutines{
        EnemySubroutine<Enemy39Entity>(Enemy39State0),
        EnemySubroutine<Enemy39Entity>(Enemy39State1),
        EnemySubroutine<Enemy39Entity>(Enemy39State2),
        EnemySubroutine<Enemy39Entity>(Enemy39State3),
        EnemySubroutine<Enemy39Entity>(Enemy39State4),
        EnemySubroutine<Enemy39Entity>(Enemy39State5)
    };

    std::vector<Entities::Enemies::Enemy39Values> Enemy39Values{
        []() {
            Entities::Enemies::Enemy39Values values{};
            values.HealthMax = 600;
            values.BeamDamage = 30;
            values.SplashDamage = 15;
            values.ContactDamage = 12;
            values.Unused8 = 600;
            values.AttackDelay = 0;
            values.AttackCountMin = 3;
            values.AttackCountMax = 6;
            values.DiveTimerMin = 1;
            values.DiveTimerMax = 40;
            values.Unused14 = 0x100010;
            values.Unused18 = 50;
            values.ScanId = 222;
            values.Effectiveness = 0x8955;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy39Values values{};
            values.HealthMax = 600;
            values.BeamDamage = 30;
            values.SplashDamage = 0;
            values.ContactDamage = 12;
            values.Unused8 = 600;
            values.AttackDelay = 0;
            values.AttackCountMin = 2;
            values.AttackCountMax = 5;
            values.DiveTimerMin = 1;
            values.DiveTimerMax = 50;
            values.Unused14 = 0x100010;
            values.Unused18 = 50;
            values.ScanId = 240;
            values.Effectiveness = 0xB155;
            return values;
        }()
    };

    std::vector<Entities::Enemies::Enemy41Values> Enemy41Values{
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 227;
            values.ScanId2 = 201;
            values.AngleIncrement1 = 16384;
            values.Health = 200;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 30;
            values.StaticShotCount = 1;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 8192;
            values.RoamTime = 600;
            values.MoveIncrement3 = 410;
            values.RollTime = 81920;
            values.FloatingAngleInc = 10240;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 12288;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 0;
            values.SlamRange = 0;
            values.MoveIncrement4 = 0;
            values.MoveIncrement5 = 0;
            values.SlamDelay = 0;
            values.WobbleCycles = 2;
            values.WobbleRotInc = 64;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 16384;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 30;
            values.StaticShotCount = 2;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 450;
            values.MoveIncrement3 = 410;
            values.RollTime = 81920;
            values.FloatingAngleInc = 18432;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 15565;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 0;
            values.SlamRange = 0;
            values.MoveIncrement4 = 0;
            values.MoveIncrement5 = 0;
            values.SlamDelay = 0;
            values.WobbleCycles = 2;
            values.WobbleRotInc = 64;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 16384;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 20;
            values.StaticShotCount = 2;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 300;
            values.MoveIncrement3 = 410;
            values.RollTime = 81920;
            values.FloatingAngleInc = 26624;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 13517;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 0;
            values.SlamRange = 0;
            values.MoveIncrement4 = 0;
            values.MoveIncrement5 = 0;
            values.SlamDelay = 0;
            values.WobbleCycles = 2;
            values.WobbleRotInc = 64;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 202;
            values.ScanId2 = 203;
            values.AngleIncrement1 = 16384;
            values.Health = 1200;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 25;
            values.StaticShotCount = 1;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 600;
            values.MoveIncrement3 = 410;
            values.RollTime = 81920;
            values.FloatingAngleInc = 14336;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 14336;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 0;
            values.SlamRange = 0;
            values.MoveIncrement4 = 0;
            values.MoveIncrement5 = 0;
            values.SlamDelay = 0;
            values.WobbleCycles = 2;
            values.WobbleRotInc = 64;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 16384;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 25;
            values.StaticShotCount = 2;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 540;
            values.MoveIncrement3 = 819;
            values.RollTime = 81920;
            values.FloatingAngleInc = 18432;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 16384;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 0;
            values.SlamRange = 0;
            values.MoveIncrement4 = 0;
            values.MoveIncrement5 = 0;
            values.SlamDelay = 0;
            values.WobbleCycles = 2;
            values.WobbleRotInc = 64;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 16384;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 25;
            values.StaticShotCount = 3;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 450;
            values.MoveIncrement3 = 1229;
            values.RollTime = 81920;
            values.FloatingAngleInc = 24576;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 16384;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 0;
            values.SlamRange = 0;
            values.MoveIncrement4 = 0;
            values.MoveIncrement5 = 0;
            values.SlamDelay = 0;
            values.WobbleCycles = 2;
            values.WobbleRotInc = 64;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 204;
            values.ScanId2 = 205;
            values.AngleIncrement1 = 32768;
            values.Health = 900;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 25;
            values.StaticShotCount = 2;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 750;
            values.MoveIncrement3 = 819;
            values.RollTime = 81920;
            values.FloatingAngleInc = 14336;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 14336;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 16384;
            values.SlamRange = 40960;
            values.MoveIncrement4 = 4096;
            values.MoveIncrement5 = 3277;
            values.SlamDelay = 150;
            values.WobbleCycles = 10;
            values.WobbleRotInc = 32;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 32768;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 25;
            values.StaticShotCount = 3;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 660;
            values.MoveIncrement3 = 819;
            values.RollTime = 81920;
            values.FloatingAngleInc = 18432;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 16384;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 32768;
            values.SlamRange = 40960;
            values.MoveIncrement4 = 4915;
            values.MoveIncrement5 = 3277;
            values.SlamDelay = 120;
            values.WobbleCycles = 10;
            values.WobbleRotInc = 48;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 32768;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 20;
            values.StaticShotCount = 3;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 600;
            values.MoveIncrement3 = 819;
            values.RollTime = 81920;
            values.FloatingAngleInc = 24576;
            values.RollingAngleInc = 0;
            values.FloatingSpeed = 16384;
            values.RollingSpeed = 0;
            values.AngleIncrement5 = 32768;
            values.SlamRange = 40960;
            values.MoveIncrement4 = 6144;
            values.MoveIncrement5 = 3277;
            values.SlamDelay = 120;
            values.WobbleCycles = 12;
            values.WobbleRotInc = 55;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 206;
            values.ScanId2 = 223;
            values.AngleIncrement1 = 32768;
            values.Health = 800;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 20;
            values.StaticShotCount = 2;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 1500;
            values.MoveIncrement3 = 819;
            values.RollTime = 600;
            values.FloatingAngleInc = 14336;
            values.RollingAngleInc = 12288;
            values.FloatingSpeed = 14336;
            values.RollingSpeed = 22528;
            values.AngleIncrement5 = 32768;
            values.SlamRange = 40960;
            values.MoveIncrement4 = 4096;
            values.MoveIncrement5 = 3277;
            values.SlamDelay = 150;
            values.WobbleCycles = 10;
            values.WobbleRotInc = 50;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 32768;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 20;
            values.StaticShotCount = 3;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 1500;
            values.MoveIncrement3 = 600;
            values.RollTime = 819;
            values.FloatingAngleInc = 18432;
            values.RollingAngleInc = 14336;
            values.FloatingSpeed = 16384;
            values.RollingSpeed = 24576;
            values.AngleIncrement5 = 32768;
            values.SlamRange = 40960;
            values.MoveIncrement4 = 4915;
            values.MoveIncrement5 = 3277;
            values.SlamDelay = 120;
            values.WobbleCycles = 10;
            values.WobbleRotInc = 55;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy41Values values{};
            values.ScanId1 = 0;
            values.ScanId2 = 0;
            values.AngleIncrement1 = 32768;
            values.Health = 0;
            values.AngleIncrement2 = 4096;
            values.MinStaticShotTimer = 60;
            values.MaxStaticShotTimer = 120;
            values.StaticShotCooldown = 20;
            values.StaticShotCount = 4;
            values.Padding17 = 0;
            values.AngleIncrement3 = 4096;
            values.MoveIncrement1 = 1024;
            values.MoveIncrement2 = 614;
            values.AngleIncrement4 = 16384;
            values.RoamTime = 1800;
            values.MoveIncrement3 = 900;
            values.RollTime = 819;
            values.FloatingAngleInc = 26624;
            values.RollingAngleInc = 16384;
            values.FloatingSpeed = 16384;
            values.RollingSpeed = 24576;
            values.AngleIncrement5 = 32768;
            values.SlamRange = 49152;
            values.MoveIncrement4 = 6144;
            values.MoveIncrement5 = 3277;
            values.SlamDelay = 120;
            values.WobbleCycles = 12;
            values.WobbleRotInc = 65;
            values.MaxWobbleDist = 2048;
            values.Magic = 0xBEEF;
            values.Padding5E = 0;
            return values;
        }()
    };

    std::vector<Entities::Enemies::Enemy44Values> Enemy44Values{
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 36;
            values.HealTimer = 120;
            values.ReappearTimer = 360;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 36;
            values.HealTimer = 120;
            values.ReappearTimer = 300;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 36;
            values.HealTimer = 120;
            values.ReappearTimer = 240;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 72;
            values.HealTimer = 120;
            values.ReappearTimer = 600;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 72;
            values.HealTimer = 120;
            values.ReappearTimer = 480;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 96;
            values.HealTimer = 120;
            values.ReappearTimer = 360;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 48;
            values.HealTimer = 120;
            values.ReappearTimer = 420;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 48;
            values.HealTimer = 120;
            values.ReappearTimer = 360;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 48;
            values.HealTimer = 120;
            values.ReappearTimer = 300;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 90;
            values.HealTimer = 120;
            values.ReappearTimer = 600;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 90;
            values.HealTimer = 120;
            values.ReappearTimer = 540;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy44Values values{};
            values.ScanId = 0;
            values.Health = 90;
            values.HealTimer = 120;
            values.ReappearTimer = 480;
            values.ColRadius = 6144;
            values.Magic = 0xBEEF;
            values.PaddingE = 0;
            return values;
        }()
    };

    std::vector<Entities::Enemies::Enemy45Values> Enemy45Values{
        []() {
            Entities::Enemies::Enemy45Values values{};
            values.Health = 30;
            values.Damage = 2;
            values.Unused4 = 0;
            values.ContactDamage = 10;
            values.ShotCooldown = 20;
            values.SalvoCooldown = 90;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.ScanId = 190;
            values.Effectiveness = 0xAAAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy45Values values{};
            values.Health = 30;
            values.Damage = 2;
            values.Unused4 = 1;
            values.ContactDamage = 10;
            values.ShotCooldown = 20;
            values.SalvoCooldown = 90;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.ScanId = 190;
            values.Effectiveness = 0xAAAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy45Values values{};
            values.Health = 30;
            values.Damage = 2;
            values.Unused4 = 1;
            values.ContactDamage = 10;
            values.ShotCooldown = 20;
            values.SalvoCooldown = 250;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.ScanId = 190;
            values.Effectiveness = 0xAAAA;
            return values;
        }(),
        []() {
            Entities::Enemies::Enemy45Values values{};
            values.Health = 30;
            values.Damage = 2;
            values.Unused4 = 1;
            values.ContactDamage = 5;
            values.ShotCooldown = 20;
            values.SalvoCooldown = 250;
            values.MinShots = 1;
            values.MaxShots = 1;
            values.ScanId = 190;
            values.Effectiveness = 0xAAAA;
            return values;
        }()
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State0{
            {1, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State1{
            {0, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior14)},
            {2, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior15)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State2{
            {4, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State3{
            {19, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior16)},
            {8, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior18)},
            {5, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior24)},
            {12, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior25)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State4{
            {3, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State5{
            {19, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior16)},
            {6, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior23)},
            {8, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior18)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State6{
            {19, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior16)},
            {7, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior17)},
            {8, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior18)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State7{
            {9, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior05)},
            {8, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior11)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State8{
            {19, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior16)},
            {3, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior21)},
            {19, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior22)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State9{
            {10, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior09)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State10{
            {11, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State11{
            {3, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State12{
            {13, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State13{
            {14, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State14{
            {15, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior19)},
            {11, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior13)},
            {16, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior20)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State15{
            {18, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State16{
            {17, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State17{
            {15, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State18{
            {14, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior12)},
            {11, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior13)}
        };
        const std::vector<EnemyBehavior<Enemy46Entity>> Enemy46State19{
            {3, static_cast<bool(*)(Enemy46Entity*)>(&Enemy46Entity::Behavior07)}
        };
    }

    std::vector<EnemySubroutine<Enemy46Entity>> Enemy46Subroutines{
        EnemySubroutine<Enemy46Entity>(Enemy46State0),
        EnemySubroutine<Enemy46Entity>(Enemy46State1),
        EnemySubroutine<Enemy46Entity>(Enemy46State2),
        EnemySubroutine<Enemy46Entity>(Enemy46State3),
        EnemySubroutine<Enemy46Entity>(Enemy46State4),
        EnemySubroutine<Enemy46Entity>(Enemy46State5),
        EnemySubroutine<Enemy46Entity>(Enemy46State6),
        EnemySubroutine<Enemy46Entity>(Enemy46State7),
        EnemySubroutine<Enemy46Entity>(Enemy46State8),
        EnemySubroutine<Enemy46Entity>(Enemy46State9),
        EnemySubroutine<Enemy46Entity>(Enemy46State10),
        EnemySubroutine<Enemy46Entity>(Enemy46State11),
        EnemySubroutine<Enemy46Entity>(Enemy46State12),
        EnemySubroutine<Enemy46Entity>(Enemy46State13),
        EnemySubroutine<Enemy46Entity>(Enemy46State14),
        EnemySubroutine<Enemy46Entity>(Enemy46State15),
        EnemySubroutine<Enemy46Entity>(Enemy46State16),
        EnemySubroutine<Enemy46Entity>(Enemy46State17),
        EnemySubroutine<Enemy46Entity>(Enemy46State18),
        EnemySubroutine<Enemy46Entity>(Enemy46State19)
    };

    namespace
    {
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State0{
            {1, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior02)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State1{
            {0, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior14)},
            {2, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior15)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State2{
            {4, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State3{
            {19, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior16)},
            {8, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior18)},
            {5, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior24)},
            {12, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior25)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State4{
            {3, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior01)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State5{
            {19, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior16)},
            {6, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior23)},
            {8, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior18)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State6{
            {19, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior16)},
            {7, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior17)},
            {8, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior18)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State7{
            {9, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior05)},
            {8, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior11)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State8{
            {19, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior16)},
            {3, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior21)},
            {19, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior22)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State9{
            {10, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior09)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State10{
            {11, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State11{
            {3, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State12{
            {13, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State13{
            {14, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior04)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State14{
            {15, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior19)},
            {11, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior13)},
            {16, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior20)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State15{
            {18, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior10)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State16{
            {17, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State17{
            {15, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State18{
            {14, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior12)},
            {11, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior13)}
        };
        const std::vector<EnemyBehavior<Enemy47Entity>> Enemy47State19{
            {3, static_cast<bool(*)(Enemy47Entity*)>(&Enemy47Entity::Behavior07)}
        };
    }

    std::vector<EnemySubroutine<Enemy47Entity>> Enemy47Subroutines{
        EnemySubroutine<Enemy47Entity>(Enemy47State0),
        EnemySubroutine<Enemy47Entity>(Enemy47State1),
        EnemySubroutine<Enemy47Entity>(Enemy47State2),
        EnemySubroutine<Enemy47Entity>(Enemy47State3),
        EnemySubroutine<Enemy47Entity>(Enemy47State4),
        EnemySubroutine<Enemy47Entity>(Enemy47State5),
        EnemySubroutine<Enemy47Entity>(Enemy47State6),
        EnemySubroutine<Enemy47Entity>(Enemy47State7),
        EnemySubroutine<Enemy47Entity>(Enemy47State8),
        EnemySubroutine<Enemy47Entity>(Enemy47State9),
        EnemySubroutine<Enemy47Entity>(Enemy47State10),
        EnemySubroutine<Enemy47Entity>(Enemy47State11),
        EnemySubroutine<Enemy47Entity>(Enemy47State12),
        EnemySubroutine<Enemy47Entity>(Enemy47State13),
        EnemySubroutine<Enemy47Entity>(Enemy47State14),
        EnemySubroutine<Enemy47Entity>(Enemy47State15),
        EnemySubroutine<Enemy47Entity>(Enemy47State16),
        EnemySubroutine<Enemy47Entity>(Enemy47State17),
        EnemySubroutine<Enemy47Entity>(Enemy47State18),
        EnemySubroutine<Enemy47Entity>(Enemy47State19)
    };

    std::vector<std::vector<ColorRgb>> Enemy24Colors = {
        {
            ColorRgb(31, 31, 31),
            ColorRgb(0, 31, 0),
            ColorRgb(9, 7, 0),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(31, 24, 0),
            ColorRgb(0, 5, 0),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(31, 15, 0),
            ColorRgb(3, 2, 4),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(20, 10, 31),
            ColorRgb(9, 3, 0),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(31, 0, 0),
            ColorRgb(0, 2, 4),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(0, 13, 31),
            ColorRgb(9, 0, 0),
            ColorRgb(31, 31, 31)
        }
    };

    std::optional<std::string> GetEnemyModelName(EnemyType type)
    {
        const std::size_t index = static_cast<std::size_t>(type);
        const std::string& name = GetAt(EnemyModelNames, index);
        if (name.empty())
        {
            return std::nullopt;
        }
        return name;
    }

    const std::vector<std::string> EnemyModelNames = {
        "warwasp_lod0",
        "zoomer",
        "Temroid_lod0",
        "Chomtroid",
        "Chomtroid",
        "Chomtroid",
        "Chomtroid",
        "",
        "",
        "",
        "BarbedWarWasp",
        "shriekbat",
        "geemer",
        "",
        "",
        "",
        "blastcap",
        "",
        "Alimbic_Turret",
        "CylinderBoss",
        "CylinderBossEye",
        "",
        "",
        "PsychoBit",
        "Gorea1A_lod0",
        "",
        "",
        "",
        "Gorea1B_lod0",
        "",
        "PowerBomb",
        "Gorea2_lod0",
        "",
        "goreaMeteor",
        "PsychoBit",
        "GuardBot2_lod0",
        "GuardBot1",
        "DripStank_lod0",
        "AlimbicStatue_lod0",
        "LavaDemon",
        "",
        "BigEyeBall",
        "",
        "BigEyeNest",
        "",
        "BigEyeTurret",
        "SphinkTick_lod0",
        "SphinkTick_lod0",
        "",
        "",
        "",
        ""
    };

    std::int32_t GetEnemyDeathEffect(EnemyType type)
    {
        const std::size_t index = static_cast<std::size_t>(type);
        return GetAt(EnemyDeathEffects, index);
    }

    const std::vector<std::int32_t> EnemyDeathEffects = {
        193, 221, 219, 219, 219, 219, 219, 76, 76, 76,
        193, 108, 221, 76, 76, 76, 76, 6, 6, 76,
        77, 76, 76, 77, 76, 76, 76, 76, 76, 76,
        77, 76, 76, 76, 77, 77, 77, 220, 222, 0,
        6, 76, 76, 76, 76, 76, 223, 223, 76, 77,
        0, 220
    };

    const std::vector<std::int32_t> EnemyAudioRangeIndices = {
        8, 9, 10, 11, 11, 11, 11, 4, 4, 8,
        8, 12, 9, 4, 4, 4, 13, 18, 18, 15,
        15, 19, 15, 16, 22, 22, 22, 22, 22, 22,
        22, 22, 22, 34, 16, 20, 20, 9, 4, 4,
        24, 25, 25, 25, 25, 25, 30, 30, 4, 4,
        4, 4
    };

    const std::vector<std::int32_t> EnemyScanIds = {
        214, 210, 0, 224, 224, 224, 224, 0, 0, 0,
        215, 213, 211, 0, 0, 0, 212, 219, 219, 0,
        0, 226, 0, 216, 243, 0, 241, 0, 244, 242,
        467, 0, 466, 0, 0, 217, 217, 218, 221, 222,
        0, 227, 227, 227, 227, 227, 220, 245, 0, 0,
        0, 0
    };

    float GetDamageMultiplier(Entities::Effectiveness effectiveness)
    {
        const std::size_t index = static_cast<std::size_t>(effectiveness);
        return GetAt(DamageMultipliers, index);
    }

    const std::vector<float> DamageMultipliers = { 0.0F, 0.5F, 1.0F, 2.0F };

    void LoadEffectiveness(EnemyType type, std::span<Entities::Effectiveness> dest)
    {
        const std::size_t index = static_cast<std::size_t>(type);
        LoadEffectiveness(GetAt(EnemyEffectiveness, index), dest);
    }

    void LoadEffectiveness(std::int32_t value, std::span<Entities::Effectiveness> dest)
    {
        assert(value >= 0);
        LoadEffectiveness(static_cast<std::uint32_t>(value), dest);
    }

    void LoadEffectiveness(std::uint32_t value, std::span<Entities::Effectiveness> dest)
    {
        assert(dest.size() == 9);
        if (dest.size() < 9)
        {
            ThrowIndexOutOfRange();
        }
        dest[0] = static_cast<Entities::Effectiveness>(value & 3U);
        dest[1] = static_cast<Entities::Effectiveness>((value >> 2U) & 3U);
        dest[2] = static_cast<Entities::Effectiveness>((value >> 4U) & 3U);
        dest[3] = static_cast<Entities::Effectiveness>((value >> 6U) & 3U);
        dest[4] = static_cast<Entities::Effectiveness>((value >> 8U) & 3U);
        dest[5] = static_cast<Entities::Effectiveness>((value >> 10U) & 3U);
        dest[6] = static_cast<Entities::Effectiveness>((value >> 12U) & 3U);
        dest[7] = static_cast<Entities::Effectiveness>((value >> 14U) & 3U);
        dest[8] = static_cast<Entities::Effectiveness>((value >> 16U) & 3U);
    }

    const std::vector<std::int32_t> EnemyEffectiveness = {
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAA8,
        0x00000,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x20000,
        0x2AAAA,
        0x2AAAA,
        0x2AABA,
        0x2AABA,
        0x2AAAA,
        0x2EAFA,
        0x24D55,
        0x2AAAA,
        0x2AA99,
        0x2AA99,
        0x2AA99,
        0x2AA99,
        0x2AA99,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA
    };

    const std::vector<std::int32_t> SlenchEffectiveness = {
        0x2AAAA, 0x2AAAA, 0x155F5, 0x16566
    };

    const std::vector<std::int32_t> SlenchSynapseEffectiveness = {
        0x2AAAA, 0x800, 0x80, 0x2000
    };

    const std::vector<std::int32_t> GoreaEffectiveness = {
        5, 0x81, 0x401, 0x2001, 0x4001, 0x301
    };
}

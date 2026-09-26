#include "TapCheck.hpp"

#include "Tap.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace
    {
        struct Case
        {
            std::string Name;
            // The gesture, and whether the row should have acted.
            std::function<bool()> Run;
            bool Expected = false;
        };

        // A row on a settings page: the width of the panel, 34 tall.
        constexpr GuiSize RowSize{420, 34};

        [[nodiscard]] std::string Bool(bool value)
        {
            return value ? "True" : "False";
        }
    }

    std::int32_t TapCheck::Run()
    {
        const int fingerIdentity = 0;
        const int otherIdentity = 0;
        const void* finger = &fingerIdentity;
        const void* other = &otherIdentity;
        const std::vector<Case> cases{
            {"a tap", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                return tap.Release(finger, {120, 17}, RowSize);
            }, true},
            // The bug, in one line: a finger that went down on a row and then
            // dragged the page.
            {"a scroll that starts on the row", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                (void)tap.Moved(finger, {121, 30});
                (void)tap.Moved(finger, {121, 90});
                return tap.Release(finger, {121, 140}, RowSize);
            }, false},
            // Once given up, a gesture does not come back.
            {"a scroll that ends where it started", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                (void)tap.Moved(finger, {120, 70});
                (void)tap.Moved(finger, {120, 17});
                return tap.Release(finger, {120, 17}, RowSize);
            }, false},
            // A few points of travel is a tap and has to stay one.
            {"a finger that wobbles under the slop", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                (void)tap.Moved(finger, {124, 21});
                (void)tap.Moved(finger, {126, 14});
                return tap.Release(finger, {126, 14}, RowSize);
            }, true},
            // The scroll gesture takes the pointer: OnPointerCaptureLost.
            {"the scroller takes the pointer", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                tap.Cancel();
                return tap.Release(finger, {120, 20}, RowSize);
            }, false},
            {"a release outside the row", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                return tap.Release(finger, {120, 96}, RowSize);
            }, false},
            // A second finger letting go somewhere else is not this row's.
            {"another finger's release", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                return tap.Release(other, {120, 17}, RowSize);
            }, false},
            {"the first finger, after the second let go", [=]
            {
                Tap tap;
                tap.Press(finger, {120, 17}, true);
                (void)tap.Release(other, {120, 17}, RowSize);
                return tap.Release(finger, {120, 17}, RowSize);
            }, true},
            // The desktop keeps what it had.
            {"a mouse dragged across the row", [=]
            {
                Tap tap;
                tap.Press(other, {40, 17}, false);
                (void)tap.Moved(other, {300, 20});
                return tap.Release(other, {300, 20}, RowSize);
            }, true},
            {"a mouse released off the row", [=]
            {
                Tap tap;
                tap.Press(other, {40, 17}, false);
                return tap.Release(other, {40, 200}, RowSize);
            }, false},
            // SliderRow's own question: whose gesture is this drag?
            {"a slider dragged along its track", [] { return Tap::Sideways({40, 6}); }, true},
            {"a page scrolled past a slider", [] { return Tap::Sideways({6, 40}); }, false},
            {"a diagonal that is mostly sideways", [] { return Tap::Sideways({-30, 25}); }, true},
            {"a slider nudged under the slop", [] { return Tap::Sideways({5, 0}); }, false}};

        std::vector<std::string> failed;
        for (const Case& test : cases)
        {
            const bool acted = test.Run();
            const bool ok = acted == test.Expected;
            if (!ok)
            {
                failed.push_back(test.Name);
            }
            std::cout << "TAP " << (ok ? "ok  " : "FAIL") << " " << test.Name << " | acted " << Bool(acted)
                << ", wanted " << Bool(test.Expected) << '\n';
        }
        if (failed.empty())
        {
            std::cout << "TAP all " << cases.size() << " cases pass (slop "
                << ::MphRead::NativeRuntime::ToString(Tap::Slop, "") << " points)\n";
        }
        else
        {
            std::string names;
            for (std::size_t i = 0; i < failed.size(); i++)
            {
                names += (i == 0 ? "" : ", ") + failed[i];
            }
            std::cout << "TAP " << failed.size() << " case(s) FAILED: " << names << '\n';
        }
        return static_cast<std::int32_t>(failed.size());
    }
}

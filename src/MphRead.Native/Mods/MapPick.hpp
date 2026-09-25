#pragma once

#include "EndScreen.hpp"

#include "../NativeRuntime/System/Globalization.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace MphRead::Mods::Network
{
    struct MapChoicesPacket;
}

namespace MphRead::Mods
{
    // The vote for the next map on the results screen.
    class MapPick final
    {
    public:
        MapPick() = delete;

        [[nodiscard]] static bool Open() noexcept { return _open; }
        [[nodiscard]] static bool Available();
        [[nodiscard]] static const std::vector<std::string>& Order() noexcept { return _order; }
        [[nodiscard]] static std::int32_t VotesFor(const std::string& roomKey);
        [[nodiscard]] static std::int32_t Eligible() noexcept { return _eligible; }
        [[nodiscard]] static std::string Leader();
        [[nodiscard]] static const std::string& Picked() noexcept { return _picked; }
        [[nodiscard]] static std::int32_t Cursor() noexcept { return _cursor; }
        [[nodiscard]] static std::int32_t Scroll() noexcept { return _scroll; }
        [[nodiscard]] static std::int32_t Window() noexcept { return _window; }
        [[nodiscard]] static std::string NameOf(const std::string& roomKey);

        static void Begin(const std::string& currentRoom, bool open);
        static void Apply(const Network::MapChoicesPacket& packet);
        [[nodiscard]] static std::int32_t IndexOf(const std::string& roomKey);
        [[nodiscard]] static std::int32_t PickedIndex();
        static void Reset();
        [[nodiscard]] static std::string Chosen();
        static void Choose(std::int32_t index);
        static void ChooseCursor();
        static void Step(std::int32_t by);
        static void Wheel(std::int32_t by);
        static void Resend();
        static void NoteLayout(const std::vector<EndScreen::Hit>& rows, std::int32_t window);
        [[nodiscard]] static std::int32_t Hovered();
        static bool HandleClick();

    private:
        using IgnoreCaseMap = std::map<std::string, std::int32_t, NativeRuntime::OrdinalIgnoreCaseLess>;

        [[nodiscard]] static std::string PlainName(const std::string& roomKey);
        static void BuildLabels();
        static void Reorder();
        static void ClampScroll();
        static void ScrollToCursor();
        [[nodiscard]] static bool Contains(const std::vector<std::string>& list, const std::string& value);

        static std::vector<std::string> _order;
        static IgnoreCaseMap _tally;
        static std::vector<std::string> _voted;
        static std::vector<std::string> _rooms;
        static bool _open;
        static std::int32_t _eligible;
        static std::string _picked;
        static std::int32_t _cursor;
        static std::int32_t _scroll;
        static std::int32_t _window;
        static std::map<std::string, std::string, NativeRuntime::OrdinalIgnoreCaseLess> _labels;
        static std::vector<EndScreen::Hit> _hits;
    };
}

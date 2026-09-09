#include "Mods/Chat/ChatBox.hpp"
#include "Mods/Chat/chat_font.hpp"
#include "Mods/Chat/chat_hud.hpp"
#include "Mods/InputSettings.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool single_player = true;
bool net_active = false;
std::string player_name = "Player";
std::vector<std::string> sent_text;
std::int64_t now_milliseconds = 0;

bool test_single_player() noexcept { return single_player; }
bool test_net_active() noexcept { return net_active; }
std::string test_player_name() { return player_name; }
void test_send_chat(std::string_view text) { sent_text.emplace_back(text); }
std::int64_t test_tick_count64() noexcept { return now_milliseconds; }

} // namespace

int main() {
    using fruityprime::chat::font::Cell;
    using fruityprime::chat::font::Count;
    using fruityprime::chat::font::First;
    using fruityprime::chat::font::index;
    using fruityprime::chat::font::measure;
    using fruityprime::chat::font::pixels;
    using fruityprime::chat::font::widths;
    assert(Count == 95);
    assert(Cell == 8);
    assert(index(First) == 0);
    assert(index('~') == 94);
    assert(index('\n') == -1);
    assert(widths()[static_cast<std::size_t>(index(' '))] == 3);
    assert(widths()[static_cast<std::size_t>(index('i'))] == 2);
    assert(widths()[static_cast<std::size_t>(index('A'))] == 6);
    assert(measure("Aim") == 14);
    const auto& glyphs = pixels();
    const std::size_t space = static_cast<std::size_t>(index(' ')) *
                              static_cast<std::size_t>(Cell * Cell);
    for (int pixel = 0; pixel < Cell * Cell; ++pixel) {
        assert(glyphs[space + static_cast<std::size_t>(pixel)] == 0);
    }
    const std::size_t a = static_cast<std::size_t>(index('A')) *
                          static_cast<std::size_t>(Cell * Cell);
    assert(glyphs[a + 1] == 1); // row 0, column 1 of the authored A.
    assert(glyphs[a + 0] == 0);

    assert(std::fabs(fruityprime::chat::hud::aspect_fix(1920, 1080)
                     - 0.75F) < 0.0001F);
    assert(fruityprime::chat::hud::margin(false) == 3.0F);
    assert(fruityprime::chat::hud::margin(true) == 30.0F);
    assert(fruityprime::chat::hud::clearance(false, 12.0F) == 12.0F);
    assert(fruityprime::chat::hud::clearance(true, 12.0F)
           == fruityprime::chat::hud::Bottom);
    assert(std::fabs(fruityprime::chat::hud::LineHeight - 4.35F)
           < 0.0001F);
    const std::string long_line(120, 'x');
    const std::string fitted = fruityprime::chat::hud::fit(long_line, 1.0F,
                                                            0.0F);
    const std::string tailed = fruityprime::chat::hud::tail(long_line, 1.0F,
                                                             0.0F);
    assert(fruityprime::chat::hud::width(fitted, 1.0F) <= 250.0F);
    assert(fruityprime::chat::hud::width(tailed, 1.0F) <= 250.0F);
    assert(fitted.size() < long_line.size());
    assert(tailed.size() < long_line.size());

    using fruityprime::chat::ChatBox;
    using fruityprime::chat::VisibleChatLine;
    fruityprime::chat::detail::BindRuntime({
        &test_single_player,
        &test_net_active,
        &test_player_name,
        &test_send_chat,
        &test_tick_count64
    });
    ChatBox::Clear();
    assert(!ChatBox::Available());
    ChatBox::Open();
    assert(!ChatBox::Composing());

    single_player = false;
    assert(ChatBox::Available());
    fruityprime::mods::InputSettings::ChatKey(0);
    assert(!ChatBox::HandleKeyDown(0, false, false, true));
    fruityprime::mods::InputSettings::ChatKey('T');
    assert(!ChatBox::HandleKeyDown('T', true, false, true));
    assert(!ChatBox::HandleKeyDown('T', false, true, true));
    assert(!ChatBox::HandleKeyDown('T', false, false, false));
    assert(ChatBox::HandleKeyDown('T', false, false, true));
    assert(ChatBox::Composing() && ChatBox::Visible());

    ChatBox::HandleText('t'); // opening key's character is swallowed
    ChatBox::HandleText('h');
    ChatBox::HandleText('i');
    ChatBox::HandleText(31);
    ChatBox::HandleText(127);
    assert(ChatBox::ComposeText() == "hi");
    std::string compose_copy = ChatBox::ComposeText();
    compose_copy.clear();
    assert(ChatBox::ComposeText() == "hi");
    assert(ChatBox::HandleKeyDown(259, false, false, true));
    assert(ChatBox::ComposeText() == "h");
    for (char value : std::string("ello   world")) {
        ChatBox::HandleText(value);
    }
    assert(ChatBox::HandleKeyDown(259, true, false, true));
    assert(ChatBox::ComposeText() == "hello   ");
    assert(ChatBox::HandleKeyDown(259, true, false, true));
    assert(ChatBox::ComposeText().empty());
    for (char value : std::string("  padded  ")) {
        ChatBox::HandleText(value);
    }
    assert(ChatBox::HandleKeyDown('V', true, false, true));
    assert(ChatBox::ComposeText() == "  padded  ");

    now_milliseconds = 100;
    assert(ChatBox::HandleKeyDown(257, false, false, true));
    assert(!ChatBox::Composing());
    assert(ChatBox::Sent() == 0);
    assert(ChatBox::ConsumeJustClosed());
    assert(!ChatBox::ConsumeJustClosed());
    std::vector<VisibleChatLine> visible;
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 1);
    assert(visible[0].Line.Name == "You");
    assert(visible[0].Line.Text == "padded");

    now_milliseconds = 9'100;
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 1 && visible[0].Alpha == 1.0F);
    now_milliseconds = 9'101;
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 1
           && std::fabs(visible[0].Alpha - 0.999F) < 0.0001F);
    now_milliseconds = 10'099;
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 1
           && std::fabs(visible[0].Alpha - 0.001F) < 0.0001F);
    now_milliseconds = 10'100;
    ChatBox::CollectVisible(visible);
    assert(visible.empty());

    ChatBox::Clear();
    now_milliseconds = 100;
    ChatBox::Add("Pilot", "future", fruityprime::net::ChatPacket::KindSay);
    now_milliseconds = 50;
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 1 && visible[0].Alpha == 1.0F);

    ChatBox::Clear();
    for (int index = 1; index <= 4; ++index) {
        now_milliseconds = index;
        ChatBox::Add("P" + std::to_string(index), std::to_string(index),
                     fruityprime::net::ChatPacket::KindSay);
    }
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 3);
    assert(visible[0].Line.Text == "2");
    assert(visible[1].Line.Text == "3");
    assert(visible[2].Line.Text == "4");

    ChatBox::Clear();
    ChatBox::Add("", "\xC2\xA0", fruityprime::net::ChatPacket::KindSay);
    ChatBox::Add("", "\xE3\x80\x80", fruityprime::net::ChatPacket::KindSay);
    ChatBox::CollectVisible(visible);
    assert(visible.empty());

    ChatBox::Open(false);
    ChatBox::Cancel();
    ChatBox::Open(false);
    assert(ChatBox::Composing());
    assert(ChatBox::ConsumeJustClosed()); // Open does not clear the flag.
    ChatBox::Cancel();
    ChatBox::Clear();
    assert(ChatBox::ConsumeJustClosed()); // Clear does not clear it either.
    assert(!ChatBox::Composing());

    const int received_before = ChatBox::Received();
    fruityprime::net::ChatPacket packet;
    packet.slot = 2;
    packet.name.clear();
    packet.text = "   ";
    ChatBox::Receive(packet);
    assert(ChatBox::Received() == received_before + 1);
    ChatBox::CollectVisible(visible);
    assert(visible.empty());

    packet.text = "incoming";
    ChatBox::Receive(packet);
    assert(ChatBox::Received() == received_before + 2);
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 1 && visible.back().Line.Name == "Player2");
    packet.kind = fruityprime::net::ChatPacket::KindSystem;
    packet.name = "ignored";
    packet.text = "notice";
    ChatBox::Receive(packet);
    ChatBox::CollectVisible(visible);
    assert(visible.back().Line.Name.empty());
    assert(visible.back().Line.Kind
           == fruityprime::net::ChatPacket::KindSystem);

    ChatBox::Clear();
    net_active = true;
    player_name = "Sylux";
    const int sent_before = ChatBox::Sent();
    ChatBox::Send("   ");
    assert(ChatBox::Sent() == sent_before + 1);
    assert(sent_text.empty());
    ChatBox::Send("hello");
    assert(ChatBox::Sent() == sent_before + 2);
    assert(sent_text.size() == 1 && sent_text.back() == "hello");
    ChatBox::CollectVisible(visible);
    assert(visible.size() == 1 && visible.back().Line.Name == "Sylux");

    ChatBox::Open(false);
    for (char value : std::string(" x ")) {
        ChatBox::HandleText(value);
    }
    assert(ChatBox::HandleKeyDown(335, false, false, true));
    assert(sent_text.size() == 2 && sent_text.back() == "x");
    assert(ChatBox::Sent() == sent_before + 3);
    const int received_after = ChatBox::Received();
    const int sent_after = ChatBox::Sent();
    ChatBox::Clear();
    assert(ChatBox::Received() == received_after);
    assert(ChatBox::Sent() == sent_after);

    ChatBox::Open(true);
    ChatBox::HandleText(31); // even an invalid first character is swallowed
    ChatBox::HandleText('a');
    assert(ChatBox::ComposeText() == "a");
    assert(ChatBox::HandleKeyDown('Q', false, false, true));
    for (int index = 0; index < 100; ++index) {
        ChatBox::HandleText('x');
    }
    assert(ChatBox::ComposeText().size()
           == static_cast<std::size_t>(ChatBox::MaxLength));
    assert(ChatBox::HandleKeyDown(256, false, false, true));
    assert(!ChatBox::Composing());

    single_player = true;
    ChatBox::Add("Pilot", "hidden", fruityprime::net::ChatPacket::KindSay);
    assert(!ChatBox::Visible());
    std::cout << "native chat tests passed\n";
    return 0;
}

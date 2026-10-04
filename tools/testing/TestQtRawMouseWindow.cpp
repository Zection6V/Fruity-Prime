// Interactive Windows integration witness. Run directly on a desktop; not a
// hardware polling-rate benchmark and deliberately not an unattended CI test.
#include "../../src/MphRead.Native/Renderer.hpp"
#include "../../src/MphRead.Native.Qt/Platform/QtApp.hpp"
#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QCoreApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QWindow>
#include <windows.h>
#undef CreateWindow
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <string_view>
#include <vector>

namespace RP = MphRead::RendererPlatform;
namespace
{
    void Expect(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
    struct Monitor final : QAbstractNativeEventFilter
    {
        HWND target = nullptr;
        std::int64_t x = 0, y = 0;
        unsigned packets = 0;
        bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override
        {
            const auto* msg = static_cast<MSG*>(message);
            if (msg->hwnd == target && msg->message == WM_INPUT)
            {
                RAWINPUT raw{};
                UINT size = sizeof(raw);
                const auto read = GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
                if (read != UINT(-1) && raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
                {
                    x += raw.data.mouse.lLastX;
                    y += raw.data.mouse.lLastY;
                    ++packets;
                }
            }
            return false;
        }
    };
    struct Probe final : RP::WindowEvents
    {
        RP::Window& window;
        Monitor monitor;
        unsigned frame = 0, delivered = 0, callbacks = 0, totalPackets = 0;
        unsigned buttons = 0, wheels = 0, text = 0;
        unsigned focusStage = 0;
        unsigned burstSize = 8, wantedBursts = 3, sentPackets = 0;
        unsigned currentBurstPackets = 0;
        std::vector<INPUT> moves;
        std::vector<double> inputTimes;
        std::chrono::steady_clock::time_point inputStart{};
        HWND other = nullptr;
        bool injected = false, captured = false;
        float beforeX = 0, beforeY = 0;
        RP::MouseState beforeMotion{};
        RP::MouseState focusBaseline{};
        explicit Probe(RP::Window& value, bool stress) : window(value)
        {
            burstSize = stress ? 134 : 8;
            wantedBursts = stress ? 600 : 3;
            moves.resize(burstSize);
            inputTimes.reserve(wantedBursts);
            for (auto& move : moves)
            {
                move.type = INPUT_MOUSE;
                move.mi.dx = 3;
                move.mi.dy = -2;
                move.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
            }
            monitor.target = reinterpret_cast<HWND>(MphRead::Qt::GameWindow()->winId());
            QCoreApplication::instance()->installNativeEventFilter(&monitor);
        }
        ~Probe()
        {
            QCoreApplication::instance()->removeNativeEventFilter(&monitor);
            if (other) DestroyWindow(other);
        }
        void OnLoad() override
        {
            window.Focus();
            SetForegroundWindow(monitor.target);
        }
        void OnInputSample() override
        {
            inputStart = std::chrono::steady_clock::now();
            ++frame;
            monitor.x = monitor.y = 0;
            monitor.packets = callbacks = 0;
            if (focusStage == 1 || focusStage == 2) return;
            if (!window.IsFocused()) return;
            if (!captured)
            {
                window.Cursor(RP::CursorState::Grabbed);
                captured = true;
                if (buttons == 0)
                {
                    QMouseEvent down(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(5, 5),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent up(QEvent::MouseButtonRelease, QPointF(5, 5), QPointF(5, 5),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(MphRead::Qt::GameWindow(), &down);
                    QCoreApplication::sendEvent(MphRead::Qt::GameWindow(), &up);
                    QWheelEvent wheel(QPointF(5, 5), QPointF(5, 5), {}, QPoint(0, 120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                    QCoreApplication::sendEvent(MphRead::Qt::GameWindow(), &wheel);
                    QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
                    QCoreApplication::sendEvent(MphRead::Qt::GameWindow(), &key);
                    Expect(buttons == 2 && wheels == 1 && text == 1, "Raw capture broke Qt buttons/wheel/text.");
                }
            }
            beforeX = window.Mouse().X;
            beforeY = window.Mouse().Y;
            beforeMotion = window.Mouse().GetSnapshot();
            // Legacy Qt motion must not move the raw virtual cursor or raise aim.
            QMouseEvent legacy(QEvent::MouseMove, QPointF(5, 5), QPointF(5, 5),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(MphRead::Qt::GameWindow(), &legacy);
            Expect(callbacks == 0 && window.Mouse().X == beforeX && window.Mouse().Y == beforeY, "Qt motion doubled raw aim.");
            if (!injected)
            {
                Expect(SendInput(burstSize, moves.data(), sizeof(INPUT)) == burstSize, "SendInput failed.");
                sentPackets += burstSize;
                currentBurstPackets = 0;
                injected = true;
            }
        }
        void OnMouseMove(const RP::MouseMoveEventArgs& args) override
        {
            if (!captured) return;
            ++callbacks;
            Expect(args.DeltaX == static_cast<float>(monitor.x) && args.DeltaY == static_cast<float>(monitor.y), "Raw packet sums differ from frame event.");
            Expect(window.Mouse().X == args.X && window.Mouse().Y == args.Y, "MouseState was not latched before event callback.");
        }
        void OnInputEventsProcessed() override
        {
            captured = window.IsFocused();
            window.Cursor(captured ? RP::CursorState::Grabbed : RP::CursorState::Normal);
        }
        void OnMouseDown(const RP::MouseButtonEventArgs&) override { ++buttons; }
        void OnMouseUp(const RP::MouseButtonEventArgs&) override { ++buttons; }
        void OnMouseWheel(const RP::MouseWheelEventArgs& args) override
        {
            Expect(args.OffsetY == 1, "Qt wheel units changed.");
            ++wheels;
        }
        void OnTextInput(const RP::TextInputEventArgs& args) override
        {
            Expect(args.Unicode == 'a', "Qt text changed.");
            ++text;
        }
        void OnRenderFrame(const RP::FrameEventArgs&) override
        {
            if (focusStage == 1)
            {
                Expect(!window.IsFocused() && !captured && callbacks == 0, "Background raw motion reached game capture.");
                Expect(!window.Mouse().MotionValid && window.Mouse().DeltaFrom(focusBaseline) == std::pair<float, float>{},
                    "Virtual/absolute rebase or focus loss produced player aim delta.");
                INPUT background{};
                background.type = INPUT_MOUSE;
                background.mi.dx = 500;
                background.mi.dy = 500;
                background.mi.dwFlags = MOUSEEVENTF_MOVE;
                Expect(SendInput(1, &background, sizeof(INPUT)) == 1, "Background injection failed.");
                focusStage = 2;
                return;
            }
            if (focusStage == 2)
            {
                // Foreground registration belongs to the application: another
                // HWND in this process can still route packets to the target.
                // Capture/foreground checks must discard their aim contribution.
                Expect(!captured && callbacks == 0, "Other-HWND raw motion contributed to game aim.");
                Expect(window.Mouse().DeltaFrom(focusBaseline) == std::pair<float, float>{},
                    "Background absolute pointer produced player aim delta.");
                window.Focus();
                SetForegroundWindow(monitor.target);
                focusStage = 3;
                return;
            }
            if (captured && monitor.packets)
            {
                Expect(window.Mouse().X == beforeX + static_cast<float>(monitor.x)
                    && window.Mouse().Y == beforeY + static_cast<float>(monitor.y), "Current-pump motion missed this simulation frame or was DPI-scaled.");
                Expect(callbacks == ((monitor.x || monitor.y) ? 1u : 0u), "Raw motion was not aggregated once per frame.");
                Expect(window.Mouse().DeltaFrom(beforeMotion) == std::pair<float, float>{static_cast<float>(monitor.x), static_cast<float>(monitor.y)},
                    "MouseState player delta differs from the current pump.");
                totalPackets += monitor.packets;
                currentBurstPackets += monitor.packets;
                Expect(currentBurstPackets <= burstSize, "Raw burst was duplicated or physical mouse interfered with test.");
                inputTimes.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - inputStart).count());
                if (currentBurstPackets < burstSize) return;
                ++delivered;
                injected = false;
                if (delivered == 1)
                {
                    focusBaseline = window.Mouse().GetSnapshot();
                    other = CreateWindowExW(0, L"STATIC", L"Raw mouse focus witness", WS_POPUP | WS_VISIBLE,
                        0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
                    Expect(other && SetForegroundWindow(other), "Could not transfer real window focus.");
                    focusStage = 1;
                }
                if (delivered == wantedBursts)
                {
                    const auto rawPosition = window.Mouse().GetSnapshot();
                    window.Cursor(RP::CursorState::Normal);
                    Expect(window.Mouse().DeltaFrom(rawPosition) == std::pair<float, float>{},
                        "Raw release rebased the player's coordinate snapshots into aim motion.");
                    window.Close();
                }
            }
            if (frame > 10000) throw std::runtime_error("No live foreground raw motion arrived.");
        }
    };
}
int main(int argc, char** argv)
{
    try
    {
        const bool stress = argc == 2 && std::string_view(argv[1]) == "--stress";
        Expect(argc == 1 || stress, "Usage: fruity_qt_raw_mouse_window_tests [--stress]");
        RP::MouseState previous{};
        previous.X = 10000;
        previous.Y = -10000;
        auto current = previous;
        current.X += 7;
        current.Y -= 3;
        Expect(current.DeltaFrom(previous) == std::pair<float, float>{7, -3}, "Normal player motion changed.");
        ++current.PositionEpoch;
        Expect(current.DeltaFrom(previous) == std::pair<float, float>{}, "Window/capture epoch allowed a camera jump.");
        current = previous;
        current.MotionValid = false;
        Expect(current.DeltaFrom(previous) == std::pair<float, float>{}, "Unfocused current snapshot moved aim.");
        Expect(previous.DeltaFrom(current) == std::pair<float, float>{}, "First frame after focus regain moved aim.");
        std::optional<RP::MouseState> previousWindow;
        for (auto mode : {RP::GraphicsWindowMode::OpenGL, RP::GraphicsWindowMode::NoApi})
        {
            RP::WindowSettings settings;
            settings.GraphicsMode = mode;
            settings.ClientSize = {641, 481};
            settings.Title = "Raw mouse integration witness";
            settings.StartVisible = true;
            auto window = RP::CreateWindow(settings);
            if (mode == RP::GraphicsWindowMode::NoApi)
            {
                window->WindowBorder(static_cast<std::int32_t>(RP::WindowBorderValue::Hidden));
                MphRead::Qt::GameWindow()->showFullScreen();
            }
            Probe probe(*window, stress);
            if (previousWindow)
                Expect(window->Mouse().PositionEpoch != previousWindow->PositionEpoch
                    && window->Mouse().DeltaFrom(*previousWindow) == std::pair<float, float>{},
                    "Replacement window retained a comparable old mouse coordinate domain.");
            window->Run(probe);
            previousWindow = window->Mouse().GetSnapshot();
            Expect(probe.delivered == probe.wantedBursts && probe.totalPackets == probe.sentPackets,
                "Interactive test ended with missing/duplicate raw samples.");
            std::sort(probe.inputTimes.begin(), probe.inputTimes.end());
            const auto percentile = [&](double fraction) {
                return probe.inputTimes[static_cast<std::size_t>((probe.inputTimes.size() - 1) * fraction)];
            };
            std::cout << "PASS Qt raw window mode=" << (mode == RP::GraphicsWindowMode::OpenGL ? "OpenGL" : "NoApi/Vulkan")
                << " same-frame/double-count/DPI/aggregation/buttons/wheel/text/focus packets=" << probe.totalPackets
                << " sent_packets=" << probe.sentPackets << " burst_size=" << probe.burstSize
                << " input_test_us_p50=" << percentile(0.5) << " input_test_us_p99=" << percentile(0.99)
                << " input_test_us_max=" << probe.inputTimes.back() << '\n';
        }
        MphRead::Qt::ShutdownApplication();
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

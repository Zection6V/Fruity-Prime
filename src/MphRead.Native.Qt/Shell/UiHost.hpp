#pragma once

#include <QtCore/QObject>
#include <QtCore/QSize>

#include <memory>

class QEvent;
class QQmlEngine;
class QQuickItem;
class QQuickRenderControl;
class QQuickWindow;
class QWindow;

namespace MphRead::Qt
{
    class ShellBridge;

    // The menus: one Qt Quick scene rendered offscreen by QQuickRenderControl
    // into a texture on the game's own graphics device, which UiOverlay
    // composites over the frame. Nothing is rendered unless the scene changed,
    // and nothing is composited while no page is showing, so a match with the
    // menus closed pays nothing for them.
    class UiHost final : public QObject
    {
    public:
        UiHost(QWindow& gameWindow, ShellBridge& bridge);
        ~UiHost() override;

        // Once per frame, with the game's context current. Renders if dirty
        // and hands the result to UiOverlay.
        void Tick(int framebufferWidth, int framebufferHeight);

        // An input event from the game window, in its coordinates.
        void Deliver(QEvent& event);

        void MarkDirty() noexcept { _dirty = true; }

    private:
        bool Initialise();
        void EnsureTarget(QSize pixels);
        void ReleaseTarget();
        void DumpOnce();

        QWindow& _gameWindow;
        ShellBridge& _bridge;
        std::unique_ptr<QQuickRenderControl> _control;
        std::unique_ptr<QQuickWindow> _window;
        std::unique_ptr<QQmlEngine> _engine;
        QQuickItem* _root = nullptr;
        bool _initialised = false;
        bool _failed = false;
        bool _dirty = true;
        unsigned _texture = 0;
        QSize _targetSize{};
    };
}

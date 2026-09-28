#include "QtApp.hpp"

#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>

#include <memory>

namespace MphRead::Qt
{
    namespace
    {
        std::unique_ptr<QGuiApplication>& Application()
        {
            static std::unique_ptr<QGuiApplication> app;
            return app;
        }
    }

    void ShutdownApplication()
    {
        // Before static destruction: Qt's own thread storage is gone by then.
        Application().reset();
    }

    void EnsureApplication()
    {
        if (QCoreApplication::instance() != nullptr)
        {
            return;
        }
        // Qt keeps the argc reference for the application's lifetime.
        static int argc = 1;
        static char name[] = "FruityPrime";
        static char* argv[] = {name, nullptr};
        QCoreApplication::setApplicationName(QStringLiteral("Fruity Prime"));
        QCoreApplication::setOrganizationName(QStringLiteral("FruityPrime"));
        // The menus render through QRhi into a texture the game composites, on
        // the same API as the game: OpenGL until the RHI's Vulkan backend.
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        // Wayland's client-side decorations move an OpenGL window's default
        // framebuffer off 0 into Qt's own FBO; the renderer draws to 0.
        if (!qEnvironmentVariableIsSet("QT_WAYLAND_DISABLE_WINDOWDECORATION"))
        {
            qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
        }
        Application() = std::make_unique<QGuiApplication>(argc, argv);
    }
}

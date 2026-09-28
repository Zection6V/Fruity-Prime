#include "QtApp.hpp"

#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>

#include <memory>

namespace MphRead::Qt
{
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
        static std::unique_ptr<QGuiApplication> app = std::make_unique<QGuiApplication>(argc, argv);
    }
}

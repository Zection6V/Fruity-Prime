// FP_QT_UISHOT=DIR: the Qt menus photographed without a window, at the size
// and under the names the Avalonia launcher's -uishot uses, so the two sets
// can be laid side by side. It proves the layout, not the window manager.

#include "UiCapture.hpp"

#include "ShellBridge.hpp"
#include "../Platform/QtApp.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickGraphicsDevice>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickRenderTarget>
#include <QtQuick/QQuickWindow>

#include <iostream>
#include <memory>
#include <vector>

namespace MphRead::Qt
{
    namespace
    {
        struct Shot
        {
            const char* Name;
            const char* Page;
            const char* Screen;
        };
    }

    int UiCapture::Run(const QString& directory, QVariantList rooms)
    {
        EnsureApplication();
        QDir().mkpath(directory);
        const QSize size(940, 528);

        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        auto context = std::make_unique<QOpenGLContext>();
        context->setFormat(format);
        auto surface = std::make_unique<QOffscreenSurface>();
        surface->setFormat(format);
        surface->create();
        if (!context->create() || !context->makeCurrent(surface.get()))
        {
            std::cout << "[qtuishot] no OpenGL context\n";
            return 1;
        }

        QOpenGLFunctions* const gl = context->functions();
        GLuint texture = 0;
        gl->glGenTextures(1, &texture);
        gl->glBindTexture(GL_TEXTURE_2D, texture);
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width(), size.height(), 0, GL_RGBA,
            GL_UNSIGNED_BYTE, nullptr);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glBindTexture(GL_TEXTURE_2D, 0);

        ShellBridge bridge(ShellBridge::Actions{});
        bridge.SetRooms(std::move(rooms), true);
        auto control = std::make_unique<QQuickRenderControl>();
        auto window = std::make_unique<QQuickWindow>(control.get());
        window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(context.get()));
        window->setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(texture, size));
        window->resize(size);
        // -uishot photographs over black.
        window->setColor(::Qt::black);
        if (!control->initialize())
        {
            std::cout << "[qtuishot] Qt Quick could not start\n";
            return 1;
        }
        auto engine = std::make_unique<QQmlEngine>();
        engine->rootContext()->setContextProperty(QStringLiteral("shell"), &bridge);
        QQmlComponent component(engine.get(), QUrl(QStringLiteral("qrc:/qt/qml/FruityPrime/Ui/Main.qml")));
        std::unique_ptr<QObject> object(component.create());
        auto* const root = qobject_cast<QQuickItem*>(object.get());
        if (root == nullptr)
        {
            std::cout << "[qtuishot] " << component.errorString().toStdString() << '\n';
            return 1;
        }
        root->setParentItem(window->contentItem());
        root->setSize(size);
        window->contentItem()->setSize(size);

        const std::vector<Shot> shots = {
            {"start", "front", "start"},
            {"pausemenu", "pause", "start"},
            {"play-offline", "front", "play"},
        };
        int written = 0;
        for (const Shot& shot : shots)
        {
            root->setProperty("screen", QString::fromUtf8(shot.Screen));
            bridge.SetPage(QString::fromUtf8(shot.Page));
            // A few passes so images, fonts and bindings settle.
            for (int i = 0; i < 4; ++i)
            {
                QCoreApplication::processEvents();
                control->polishItems();
                control->beginFrame();
                control->sync();
                control->render();
                control->endFrame();
            }
            context->makeCurrent(surface.get());
            GLuint fbo = 0;
            gl->glGenFramebuffers(1, &fbo);
            gl->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            QImage image(size, QImage::Format_RGBA8888_Premultiplied);
            gl->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
            gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
            gl->glDeleteFramebuffers(1, &fbo);
            image = image.flipped(::Qt::Vertical).convertToFormat(QImage::Format_RGB32);
            const QString path = QDir(directory).filePath(QString::fromUtf8(shot.Name) + QStringLiteral(".png"));
            if (image.save(path))
            {
                ++written;
                std::cout << "[qtuishot] " << path.toStdString() << '\n';
            }
        }
        // Qt Quick first, on its context, then the context itself.
        object.reset();
        engine.reset();
        window.reset();
        control.reset();
        context->makeCurrent(surface.get());
        gl->glDeleteTextures(1, &texture);
        context->doneCurrent();
        std::cout << "[qtuishot] " << written << " screen(s) written to " << directory.toStdString() << '\n';
        return written == static_cast<int>(shots.size()) ? 0 : 1;
    }
}

#include "UiHost.hpp"

#include "ShellBridge.hpp"

#include "../../MphRead.Native/Mods/Render/UiOverlay.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QWheelEvent>
#include <QtGui/QWindow>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickGraphicsDevice>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickOpenGLUtils>
#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickRenderTarget>
#include <QtQuick/QQuickWindow>

#include <iostream>

namespace MphRead::Qt
{
    namespace
    {
        // The offscreen scene borrows the game window for its screen, device
        // pixel ratio and input method (the Android/iOS keyboard for chat).
        class RenderControl final : public QQuickRenderControl
        {
        public:
            explicit RenderControl(QWindow& window) : _window(window) {}

            QWindow* renderWindow(QPoint* offset) override
            {
                if (offset != nullptr)
                {
                    *offset = QPoint(0, 0);
                }
                return &_window;
            }

        private:
            QWindow& _window;
        };
    }

    UiHost::UiHost(QWindow& gameWindow, ShellBridge& bridge)
        : _gameWindow(gameWindow), _bridge(bridge)
    {
    }

    UiHost::~UiHost()
    {
        _root = nullptr;
        _engine.reset();
        ReleaseTarget();
        _window.reset();
        _control.reset();
    }

    bool UiHost::Initialise()
    {
        if (_initialised || _failed)
        {
            return _initialised;
        }
        QOpenGLContext* const context = QOpenGLContext::currentContext();
        if (context == nullptr)
        {
            _failed = true;
            std::cout << "[ui] no current OpenGL context for the menus\n";
            return false;
        }
        _control = std::make_unique<RenderControl>(_gameWindow);
        _window = std::make_unique<QQuickWindow>(_control.get());
        _window->setColor(::Qt::transparent);
        _window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(context));
        if (!_control->initialize())
        {
            _failed = true;
            std::cout << "[ui] Qt Quick could not start on the game's context\n";
            return false;
        }
        QObject::connect(_control.get(), &QQuickRenderControl::renderRequested, this,
            [this]() { _dirty = true; });
        QObject::connect(_control.get(), &QQuickRenderControl::sceneChanged, this,
            [this]() { _dirty = true; });

        _engine = std::make_unique<QQmlEngine>();
        _engine->rootContext()->setContextProperty(QStringLiteral("shell"), &_bridge);
        QQmlComponent component(_engine.get(),
            QUrl(QStringLiteral("qrc:/qt/qml/FruityPrime/Ui/Main.qml")));
        QObject* const object = component.create();
        _root = qobject_cast<QQuickItem*>(object);
        if (_root == nullptr)
        {
            _failed = true;
            std::cout << "[ui] the menus could not be loaded: "
                      << component.errorString().toStdString() << '\n';
            delete object;
            return false;
        }
        _root->setParentItem(_window->contentItem());
        _initialised = true;
        return true;
    }

    void UiHost::EnsureTarget(QSize pixels)
    {
        if (_texture != 0 && pixels == _targetSize)
        {
            return;
        }
        ReleaseTarget();
        QOpenGLFunctions* const gl = QOpenGLContext::currentContext()->functions();
        GLuint texture = 0;
        gl->glGenTextures(1, &texture);
        gl->glBindTexture(GL_TEXTURE_2D, texture);
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixels.width(), pixels.height(), 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->glBindTexture(GL_TEXTURE_2D, 0);
        _texture = texture;
        _targetSize = pixels;
        _window->setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(texture, pixels));
        const qreal ratio = _gameWindow.devicePixelRatio();
        const QSize logical(qRound(pixels.width() / ratio), qRound(pixels.height() / ratio));
        _window->resize(logical);
        _window->contentItem()->setSize(logical);
        _root->setSize(logical);
        _dirty = true;
    }

    void UiHost::ReleaseTarget()
    {
        if (_texture == 0)
        {
            return;
        }
        if (QOpenGLContext* const context = QOpenGLContext::currentContext())
        {
            GLuint texture = _texture;
            context->functions()->glDeleteTextures(1, &texture);
        }
        _texture = 0;
        _targetSize = QSize();
        ::MphRead::Mods::Render::UiOverlay::Release();
    }

    void UiHost::Tick(int framebufferWidth, int framebufferHeight)
    {
        using ::MphRead::Mods::Render::UiOverlay;
        if (!_bridge.Showing() || framebufferWidth <= 0 || framebufferHeight <= 0)
        {
            UiOverlay::Visible(false);
            return;
        }
        if (!Initialise())
        {
            UiOverlay::Visible(false);
            return;
        }
        EnsureTarget(QSize(framebufferWidth, framebufferHeight));
        if (_dirty)
        {
            _dirty = false;
            QOpenGLContext* const context = QOpenGLContext::currentContext();
            QSurface* const surface = context->surface();
            _control->polishItems();
            _control->beginFrame();
            _control->sync();
            _control->render();
            _control->endFrame();
            // An offscreen frame leaves the context current on Qt's own
            // fallback surface; the game draws to its window.
            context->makeCurrent(surface);
            // Qt Quick leaves its own GL state bound; the game expects the
            // defaults it starts every pass from.
            QQuickOpenGLUtils::resetOpenGLState();
            DumpOnce();
            UiOverlay::UseTexture(static_cast<std::int32_t>(_texture),
                _targetSize.width(), _targetSize.height());
        }
        UiOverlay::Visible(true);
    }

    void UiHost::DumpOnce()
    {
        // FP_QT_UI_DUMP=path: the menus' texture as rendered, once, to tell a
        // scene problem from a compositing one.
        static const QString path = qEnvironmentVariable("FP_QT_UI_DUMP");
        static bool done = false;
        if (path.isEmpty() || done)
        {
            return;
        }
        done = true;
        QOpenGLFunctions* const gl = QOpenGLContext::currentContext()->functions();
        GLuint fbo = 0;
        gl->glGenFramebuffers(1, &fbo);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);
        QImage image(_targetSize, QImage::Format_RGBA8888);
        gl->glReadPixels(0, 0, _targetSize.width(), _targetSize.height(), GL_RGBA,
            GL_UNSIGNED_BYTE, image.bits());
        gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        gl->glDeleteFramebuffers(1, &fbo);
        image.save(path);
        std::cout << "[ui] dumped " << path.toStdString() << " status "
                  << gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) << '\n';
    }

    void UiHost::Deliver(QEvent& event)
    {
        if (!_initialised)
        {
            return;
        }
        // The offscreen window's coordinates are the game window's own. The
        // event is the game window's, so the scene gets its own copy.
        const std::unique_ptr<QEvent> copy(event.clone());
        QCoreApplication::sendEvent(_window.get(), copy.get());
    }
}

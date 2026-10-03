#include "QtApp.hpp"
#include "../../MphRead.Native/Mods/Branding.hpp"

#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>

#include <memory>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace MphRead::Qt
{
    namespace
    {
        // Still alive at static destruction means the process is leaving through
        // exit() -- every diagnostic command does -- without ShutdownApplication.
        // Deleting it then crashes: Qt's thread storage (the font cache) has
        // already been torn down, and ~QGuiApplication reads it (SIGSEGV in
        // QThreadStorage<QFontCache*>::deleteData on macOS). The process is
        // ending; leave the object to the OS rather than run that destructor.
        struct ApplicationHolder final
        {
            std::unique_ptr<QGuiApplication> app;
            ~ApplicationHolder() { (void)app.release(); }
        };

        std::unique_ptr<QGuiApplication>& Application()
        {
            static ApplicationHolder holder;
            return holder.app;
        }
    }

#if defined(__APPLE__)
    namespace
    {
        // A game has no windows for macOS to bring back: its window's size
        // and place are WindowGeometry's. Left on, a launch after a crash
        // opens AppKit's modal "reopen windows?" alert from inside Qt's first
        // processEvents, and the process waits on it -- invisible to a
        // scripted check, a hang to anyone else. Registered, not written: the
        // registration domain lasts this process and touches no preference.
        void OptOutOfWindowRestoration()
        {
            Class const userDefaults = objc_getClass("NSUserDefaults");
            if (userDefaults == nil) return;
            const auto send = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
            const id defaults = send(reinterpret_cast<id>(userDefaults), sel_registerName("standardUserDefaults"));
            const void* keys[]{CFSTR("ApplePersistenceIgnoreState")};
            const void* values[]{kCFBooleanTrue};
            CFDictionaryRef registered = CFDictionaryCreate(nullptr, keys, values, 1,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(defaults,
                sel_registerName("registerDefaults:"), reinterpret_cast<id>(const_cast<__CFDictionary*>(registered)));
            CFRelease(registered);
        }
    }
#endif

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
        QCoreApplication::setApplicationName(QString::fromUtf8(Mods::Branding::Name.data(), static_cast<int>(Mods::Branding::Name.size())));
        QCoreApplication::setOrganizationName(QStringLiteral("FruityPrime"));
        // The menus render through QRhi into a texture the game composites, on
        // the same API as the game: OpenGL until the RHI's Vulkan backend.
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        // Glyphs rasterised by the font engine at their size, as Skia does
        // for Avalonia: the distance-field default softens a pixel face at
        // the launcher's small sizes until it reads as blurred.
        const QByteArray text = qgetenv("FP_QT_TEXT");
        QQuickWindow::setTextRenderType(text == "qt" ? QQuickWindow::QtTextRendering
                                        : text == "curve" ? QQuickWindow::CurveTextRendering
                                                          : QQuickWindow::NativeTextRendering);
        // Wayland's client-side decorations move an OpenGL window's default
        // framebuffer off 0 into Qt's own FBO; the renderer draws to 0.
        if (!qEnvironmentVariableIsSet("QT_WAYLAND_DISABLE_WINDOWDECORATION"))
        {
            qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
        }
#if defined(__APPLE__)
        OptOutOfWindowRestoration();
#endif
        Application() = std::make_unique<QGuiApplication>(argc, argv);
        // Grey antialiasing with hinting: aliased glyphs read as pixelated
        // (FP_QT_TEXT_AA=0 brings them back). Never subpixel colour, which
        // would sit on the wrong pixels of a texture blended over the game.
        QFont font = QGuiApplication::font();
        font.setStyleStrategy(qgetenv("FP_QT_TEXT_AA") == "0"
            ? QFont::StyleStrategy(QFont::NoAntialias | QFont::NoSubpixelAntialias)
            : QFont::StyleStrategy(QFont::PreferAntialias | QFont::NoSubpixelAntialias));
        font.setHintingPreference(QFont::PreferVerticalHinting);
        QGuiApplication::setFont(font);
    }
}

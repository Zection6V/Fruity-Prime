#include "UiChecks.hpp"
#include "SettingsModel.hpp"
#include "UiCapture.hpp"
#include "QmlTypes.hpp"
#include "ShellBridge.hpp"
#include "../Platform/QtApp.hpp"
#include "../../MphRead.Native/Mods/Input/GamepadChecks.hpp"
#include "../../MphRead.Native/Mods/Input/GamepadUiRouter.hpp"
#include "../../MphRead.Native/Mods/Input/KeyCapture.hpp"
#include "../../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtGui/QImage>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtGui/qtestsupport_gui.h>
#include <QtTest/qtesttouch.h>

#include <algorithm>
#include <iostream>

namespace MphRead::Qt
{
    int RunTapChecks()
    {
        EnsureApplication();
        RegisterQmlTypes();
        ShellBridge bridge(ShellBridge::Actions{});
        QQuickView view;
        QQmlComponent component(view.engine());
        component.setData(R"qml(
import QtQuick
import FruityPrime.Ui
Flickable {
    width: 300; height: 200; contentHeight: 600
    ToggleRow { objectName: "toggle"; x: 0; y: 80; width: 300; label: "Gesture check" }
}
)qml", QUrl());
        view.setContent(QUrl(), &component, component.create());
        view.resize(300, 200);
        view.show();
        if (!QTest::qWaitForWindowExposed(&view)) return 1;
        auto* row = view.rootObject() ? view.rootObject()->findChild<QQuickItem*>(QStringLiteral("toggle")) : nullptr;
        if (!row) return 1;
        std::unique_ptr<QPointingDevice> device(QTest::createTouchDevice());
        auto touch = QTest::touchEvent(&view, device.get());
        touch.press(0, QPoint(150, 97), &view).commit();
        QTest::qWait(20);
        touch.release(0, QPoint(150, 97), &view).commit();
        QTest::qWait(20);
        if (!row->property("on").toBool()) return 1;
        row->setProperty("on", false);
        touch.press(0, QPoint(150, 97), &view).commit();
        QTest::qWait(20);
        touch.move(0, QPoint(150, 50), &view).commit();
        QTest::qWait(20);
        touch.move(0, QPoint(150, 10), &view).commit();
        QTest::qWait(20);
        touch.release(0, QPoint(150, 10), &view).commit();
        QTest::qWait(20);
        if (row->property("on").toBool()) return 1;
        std::cout << "[tapcheck] PASS: Qt touch tap acts; scrolling a settings row does not toggle it\n";
        return 0;
    }

    void RunUiChecks(const std::optional<std::string>& shots)
    {
        namespace Input = ::MphRead::Mods::Input;
        using Prefs = ::MphRead::Mods::Launcher::LauncherPrefs;
        const auto check = Input::GamepadChecks::Check;
        EnsureApplication();
        const auto originalLatency = Prefs::LowLatency();
        {
            SettingsModel model;
            auto& keyboard = *static_cast<RowModel*>(model.Keyboard());
            const auto key = std::find_if(keyboard.Rows().begin(), keyboard.Rows().end(),
                [](const Row& row) { return row.Type == QStringLiteral("key") && row.Binding >= 0; });
            check(key != keyboard.Rows().end(), "Qt settings expose keyboard bindings");
            const int row = static_cast<int>(key - keyboard.Rows().begin());
            model.listenKey(row);
            check(model.Listening() && Input::KeyCapture::AnyListening(), "Qt binding captures game input");
            check(model.pressKey(::Qt::Key_Escape, 0, 0, 0, QString()), "Escape belongs to Qt binding capture");
            check(!model.Listening() && !Input::KeyCapture::AnyListening(), "Escape releases binding capture");
            model.listenKey(-1);
            check(!model.Listening(), "invalid Qt binding row does not capture");
            auto& gamepad = *static_cast<RowModel*>(model.Gamepad());
            check(std::any_of(gamepad.Rows().begin(), gamepad.Rows().end(),
                [](const Row& value) { return value.Type == QStringLiteral("pad"); }),
                "Qt settings expose controller bindings");
            auto& display = *static_cast<RowModel*>(model.Display());
            Row* latency = display.Find(QStringLiteral("lowLatency"));
            check(latency != nullptr, "Qt settings expose low latency");
            const int latencyRow = static_cast<int>(latency - display.Rows().data());
            display.setIndex(latencyRow, (static_cast<int>(originalLatency) + 1) % 3);
            check(Prefs::LowLatency() != originalLatency, "Qt low latency preview is live");
            model.cancel();
            check(Prefs::LowLatency() == originalLatency, "Qt Cancel restores low latency");
            model.listenKey(row);
        }
        check(!Input::KeyCapture::AnyListening(), "destroying Qt settings releases capture");
        check(!Input::GamepadContexts::Capturing(), "Qt settings release controller capture");

        QTemporaryDir temporary;
        const std::string directory = shots.value_or(temporary.path().toStdString());
        check(UiCapture::Run(directory) == 0, "Qt Quick screen creation and rendering");
        for (const char* screen : {"settings-gamepad", "settings-controls", "pausemenu", "start-phone-portrait"})
        {
            QImage image(QDir(QString::fromStdString(directory)).filePath(QString::fromLatin1(screen) + ".png"));
            check(!image.isNull(), std::string("Qt rendered ") + screen);
            bool varied = false;
            for (int y = 0; y < image.height() && !varied; y += 7)
                for (int x = 0; x < image.width(); x += 7)
                    if (image.pixel(x, y) != image.pixel(0, 0)) { varied = true; break; }
            check(varied, std::string("Qt pixels contain screen content: ") + screen);
        }
    }
}

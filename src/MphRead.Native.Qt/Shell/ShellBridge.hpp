#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>

#include <functional>

namespace MphRead::Qt
{
    // What the QML pages see of the shell (the context property "shell").
    // The pages only present and ask; every decision stays in Shell.cpp and
    // the portable launcher code it calls.
    class ShellBridge final : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QString page READ Page NOTIFY pageChanged)
        Q_PROPERTY(QVariantList rooms READ Rooms NOTIFY roomsChanged)
        Q_PROPERTY(QVariantList modes READ Modes CONSTANT)
        Q_PROPERTY(QVariantList hunters READ Hunters CONSTANT)
        Q_PROPERTY(bool gameFilesReady READ GameFilesReady NOTIFY roomsChanged)
        Q_PROPERTY(QString playerName READ PlayerName NOTIFY profileChanged)
        Q_PROPERTY(QString version READ Version CONSTANT)
        Q_PROPERTY(QString windowLabel READ WindowLabel NOTIFY windowChanged)

    public:
        struct Actions
        {
            std::function<void(QString room, int mode, int hunter, int bots, int botLevel)> Play;
            std::function<void()> Quit;
            std::function<void()> Resume;
            std::function<void()> LeaveMatch;
            std::function<void()> ToggleFullscreen;
        };

        explicit ShellBridge(Actions actions);

        [[nodiscard]] QString Page() const { return _page; }
        [[nodiscard]] bool Showing() const noexcept { return !_page.isEmpty(); }
        void SetPage(QString page);
        [[nodiscard]] QVariantList Rooms() const { return _rooms; }
        void SetRooms(QVariantList rooms, bool gameFilesReady);
        [[nodiscard]] QVariantList Modes() const;
        [[nodiscard]] QVariantList Hunters() const;
        [[nodiscard]] bool GameFilesReady() const noexcept { return _gameFilesReady; }
        [[nodiscard]] QString PlayerName() const;
        [[nodiscard]] QString Version() const;
        [[nodiscard]] QString WindowLabel() const;

        Q_INVOKABLE void play(const QString& room, int mode, int hunter, int bots, int botLevel);
        Q_INVOKABLE void quit();
        Q_INVOKABLE void resume();
        Q_INVOKABLE void leaveMatch();
        Q_INVOKABLE void openSupport();
        Q_INVOKABLE void toggleFullscreen();
        Q_INVOKABLE void openSettings();

    signals:
        void pageChanged();
        void roomsChanged();
        void profileChanged();
        void windowChanged();

    private:
        Actions _actions;
        QString _page;
        QVariantList _rooms;
        bool _gameFilesReady = false;
    };
}

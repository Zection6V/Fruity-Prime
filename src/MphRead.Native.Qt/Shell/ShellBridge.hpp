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

    public:
        struct Actions
        {
            std::function<void(QString room, int mode, int hunter, int bots, int botLevel)> Play;
            std::function<void()> Quit;
            std::function<void()> Resume;
            std::function<void()> LeaveMatch;
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

        Q_INVOKABLE void play(const QString& room, int mode, int hunter, int bots, int botLevel);
        Q_INVOKABLE void quit();
        Q_INVOKABLE void resume();
        Q_INVOKABLE void leaveMatch();

    signals:
        void pageChanged();
        void roomsChanged();

    private:
        Actions _actions;
        QString _page;
        QVariantList _rooms;
        bool _gameFilesReady = false;
    };
}

#pragma once

#if defined(__ANDROID__)
#include <QtQuick/QQuickPaintedItem>
#include <QtGui/QImage>
#include <future>
#include <optional>
#include <vector>
#else
#include <QtQuick/QQuickItem>
#endif

namespace MphRead::Qt
{
    // HunterStand: the launcher's turntable. The engine draws the real model
    // (LauncherHunter) over the menus, in the rectangle this item holds; the
    // item itself paints nothing.
    class HunterStandItem : public
#if defined(__ANDROID__)
        QQuickPaintedItem
#else
        QQuickItem
#endif
    {
        Q_OBJECT
        // A PlayModel hunter index (0-6, 7 Random) and a suit, 0-3.
        Q_PROPERTY(int hunter READ Hunter WRITE SetHunter NOTIFY changed)
        Q_PROPERTY(int suit READ Suit WRITE SetSuit NOTIFY changed)

    public:
        explicit HunterStandItem(QQuickItem* parent = nullptr);
        ~HunterStandItem() override;

        [[nodiscard]] int Hunter() const noexcept { return _hunter; }
        void SetHunter(int value);
        [[nodiscard]] int Suit() const noexcept { return _suit; }
        void SetSuit(int value);

        // Once a frame, after the menus render: hand the first stand on
        // screen to the engine, or tell it there is none.
        static void Publish(double windowWidth, double windowHeight);
#if defined(__ANDROID__)
        void paint(QPainter* painter) override;
#endif

    signals:
        void changed();

    private:
        int _hunter = 0;
        int _suit = 0;
#if defined(__ANDROID__)
        void PollPicture();
        QImage _picture;
        std::shared_future<std::optional<std::vector<std::uint8_t>>> _pending;
        int _pendingHunter = -1;
        int _pendingSuit = -1;
#endif
    };
}

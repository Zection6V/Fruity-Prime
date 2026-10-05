#include "HunterStandItem.hpp"

#include "../../MphRead.Native/Mods/Launcher/Portable/LaunchPlan.hpp"
#include "../../MphRead.Native/Mods/Render/LauncherHunter.hpp"
#if defined(__ANDROID__)
#include "../../MphRead.Native/Mods/Render/HunterShot.hpp"
#include <QtCore/QTimer>
#include <QtGui/QPainter>
#endif

#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <vector>

namespace MphRead::Qt
{
    namespace
    {
        std::vector<HunterStandItem*>& Live()
        {
            static std::vector<HunterStandItem*> items;
            return items;
        }

        [[nodiscard]] bool Showing(const QQuickItem* item)
        {
            for (const QQuickItem* at = item; at != nullptr; at = at->parentItem())
            {
                if (!at->isVisible() || at->opacity() <= 0.0)
                {
                    return false;
                }
            }
            return item->window() != nullptr;
        }
    }

    HunterStandItem::HunterStandItem(QQuickItem* parent) :
#if defined(__ANDROID__)
        QQuickPaintedItem(parent)
#else
        QQuickItem(parent)
#endif
    {
        Live().push_back(this);
#if defined(__ANDROID__)
        auto* timer = new QTimer(this);
        timer->setInterval(200);
        connect(timer, &QTimer::timeout, this, &HunterStandItem::PollPicture);
        timer->start();
#endif
    }

#if defined(__ANDROID__)
    void HunterStandItem::PollPicture()
    {
        using Mods::Render::HunterShot;
        constexpr int size = 256;
        if (_pending.valid())
        {
            if (_pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
            try
            {
                const auto& pixels = _pending.get();
                if (pixels && pixels->size() == size * size * 4 &&
                    _pendingHunter == _hunter && _pendingSuit == _suit)
                {
                    // The Android renderer returns top-down BGRA. Detach the
                    // image before the worker future releases those bytes.
                    _picture = QImage(pixels->data(), size, size, QImage::Format_ARGB32).copy();
                    update();
                }
            }
            catch (...) { _picture = {}; update(); }
            _pending = {};
        }
        if (!Showing(this) || !HunterShot::Current) return;
        const auto hunter = _hunter >= 7 ? Mods::Launcher::Hunters::Resolve(::MphRead::Hunter::Random)
                                         : static_cast<::MphRead::Hunter>(_hunter);
        _pendingHunter = _hunter;
        _pendingSuit = _suit;
        _pending = HunterShot::Current->RenderAsync(hunter, _suit, size, size);
    }

    void HunterStandItem::paint(QPainter* painter)
    {
        if (!_picture.isNull()) painter->drawImage(boundingRect(), _picture);
    }
#endif

    HunterStandItem::~HunterStandItem()
    {
        auto& live = Live();
        live.erase(std::remove(live.begin(), live.end(), this), live.end());
        if (live.empty())
        {
            ::MphRead::Mods::Render::LauncherHunter::Wanted(false);
        }
    }

    void HunterStandItem::SetHunter(int value)
    {
        if (value != _hunter)
        {
            _hunter = value;
            emit changed();
        }
    }

    void HunterStandItem::SetSuit(int value)
    {
        if (value != _suit)
        {
            _suit = value;
            emit changed();
        }
    }

    void HunterStandItem::Publish(double windowWidth, double windowHeight)
    {
        using ::MphRead::Mods::Render::LauncherHunter;
        for (HunterStandItem* const item : Live())
        {
            if (!Showing(item) || item->width() <= 1 || item->height() <= 1
                || windowWidth <= 0 || windowHeight <= 0)
            {
                continue;
            }
            const QRectF box = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
            // Random shows one of the seven, as the launcher rerolls it.
            const ::MphRead::Hunter hunter = item->_hunter >= 7
                ? ::MphRead::Mods::Launcher::Hunters::Resolve(::MphRead::Hunter::Random)
                : static_cast<::MphRead::Hunter>(item->_hunter);
            LauncherHunter::Wanted(true);
            LauncherHunter::Hunter(hunter);
            LauncherHunter::Suit(std::clamp(item->_suit, 0, 3));
            LauncherHunter::Left(static_cast<float>(box.left() / windowWidth));
            LauncherHunter::Top(static_cast<float>(box.top() / windowHeight));
            LauncherHunter::Right(static_cast<float>(box.right() / windowWidth));
            LauncherHunter::Bottom(static_cast<float>(box.bottom() / windowHeight));
            return;
        }
        LauncherHunter::Wanted(false);
    }
}

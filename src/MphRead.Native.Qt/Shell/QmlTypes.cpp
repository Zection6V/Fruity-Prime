#include "QmlTypes.hpp"

#include "HunterStandItem.hpp"
#include "PlayModel.hpp"
#include "ServerBadgeItem.hpp"

#include <QtQml/qqml.h>

namespace MphRead::Qt
{
    void RegisterQmlTypes()
    {
        static bool done = false;
        if (done)
        {
            return;
        }
        done = true;
        const char* const uri = "FruityPrime.Launcher";
        qmlRegisterType<PlayModel>(uri, 1, 0, "PlayModel");
        qmlRegisterType<HunterStandItem>(uri, 1, 0, "HunterStand");
        qmlRegisterType<ServerBadgeItem>(uri, 1, 0, "ServerBadge");
    }
}

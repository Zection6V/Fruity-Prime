#pragma once

#include <cstdint>
#include <jni.h>

namespace MphRead::Droid
{
    class GamepadBridge final
    {
    public:
        GamepadBridge() = delete;
        GamepadBridge(const GamepadBridge&) = delete;
        GamepadBridge& operator=(const GamepadBridge&) = delete;
        GamepadBridge(GamepadBridge&&) = delete;
        GamepadBridge& operator=(GamepadBridge&&) = delete;
        ~GamepadBridge() = delete;

        [[nodiscard]] static bool HandleKey(
            std::int32_t keyCode,
            jobject event,
            bool down,
            JNIEnv* env
        );

        [[nodiscard]] static bool HandleMotion(
            jobject event,
            JNIEnv* env
        );
    };
}

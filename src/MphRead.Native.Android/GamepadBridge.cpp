#include "GamepadBridge.hpp"

#if !defined(__ANDROID__)
#error "GamepadBridge is only valid for the Android native target."
#endif

#include "../MphRead.Native/Mods/Input/GamepadInput.hpp"

#include <android/input.h>

#include <cstdint>

namespace
{
    using MphRead::Mods::Input::GamepadButtons;
    using MphRead::Mods::Input::GamepadInput;
    using MphRead::Mods::Input::GamepadState;

    constexpr float TriggerPress = 0.65F;
    constexpr float HatPress = 0.5F;

    std::int32_t Bits(GamepadButtons value) noexcept
    {
        return static_cast<std::int32_t>(value);
    }

    bool IsGamepad(std::int32_t source) noexcept
    {
        return (source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD
            || (source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK
            || (source & AINPUT_SOURCE_DPAD) == AINPUT_SOURCE_DPAD;
    }

    jmethodID GetMethod(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature
    )
    {
        jclass type = env->GetObjectClass(object);
        jmethodID method = env->GetMethodID(type, name, signature);
        env->DeleteLocalRef(type);
        return method;
    }

    std::int32_t GetSource(JNIEnv* env, jobject event)
    {
        const jmethodID method = GetMethod(env, event, "getSource", "()I");
        return static_cast<std::int32_t>(env->CallIntMethod(event, method));
    }

    std::int32_t GetAction(JNIEnv* env, jobject event)
    {
        const jmethodID method = GetMethod(env, event, "getAction", "()I");
        return static_cast<std::int32_t>(env->CallIntMethod(event, method));
    }

    std::int32_t GetRepeatCount(JNIEnv* env, jobject event)
    {
        const jmethodID method = GetMethod(env, event, "getRepeatCount", "()I");
        return static_cast<std::int32_t>(env->CallIntMethod(event, method));
    }

    float GetAxisValue(
        JNIEnv* env,
        jobject event,
        jmethodID method,
        std::int32_t axis
    )
    {
        return static_cast<float>(
            env->CallFloatMethod(event, method, static_cast<jint>(axis))
        );
    }

    float Pick(
        JNIEnv* env,
        jobject event,
        jmethodID getAxisValue,
        std::int32_t first,
        std::int32_t second
    )
    {
        const float value = GetAxisValue(env, event, getAxisValue, first);
        return value != 0.0F
            ? value
            : GetAxisValue(env, event, getAxisValue, second);
    }

    GamepadButtons Map(std::int32_t code) noexcept
    {
        switch (code)
        {
        case AKEYCODE_BUTTON_A:
            return GamepadButtons::A;
        case AKEYCODE_BUTTON_B:
            return GamepadButtons::B;
        case AKEYCODE_BUTTON_X:
            return GamepadButtons::X;
        case AKEYCODE_BUTTON_Y:
            return GamepadButtons::Y;
        case AKEYCODE_BUTTON_L1:
            return GamepadButtons::LeftBumper;
        case AKEYCODE_BUTTON_R1:
            return GamepadButtons::RightBumper;
        case AKEYCODE_BUTTON_L2:
            return GamepadButtons::LeftTrigger;
        case AKEYCODE_BUTTON_R2:
            return GamepadButtons::RightTrigger;
        case AKEYCODE_BUTTON_SELECT:
            return GamepadButtons::Back;
        case AKEYCODE_BUTTON_START:
            return GamepadButtons::Start;
        case AKEYCODE_BUTTON_THUMBL:
            return GamepadButtons::LeftThumb;
        case AKEYCODE_BUTTON_THUMBR:
            return GamepadButtons::RightThumb;
        case AKEYCODE_DPAD_UP:
            return GamepadButtons::DpadUp;
        case AKEYCODE_DPAD_DOWN:
            return GamepadButtons::DpadDown;
        case AKEYCODE_DPAD_LEFT:
            return GamepadButtons::DpadLeft;
        case AKEYCODE_DPAD_RIGHT:
            return GamepadButtons::DpadRight;
        default:
            return GamepadButtons::None;
        }
    }
}

namespace MphRead::Droid
{
    bool GamepadBridge::HandleKey(
        std::int32_t keyCode,
        jobject event,
        bool down,
        JNIEnv* env
    )
    {
        if (event == nullptr || !IsGamepad(GetSource(env, event)))
        {
            return false;
        }

        const GamepadButtons button = Map(keyCode);
        if (button == GamepadButtons::None)
        {
            return false;
        }

        if (down && GetRepeatCount(env, event) > 0)
        {
            return true;
        }

        GamepadState state = GamepadInput::State;
        state.Connected = true;
        if (!state.Name.has_value())
        {
            state.Name = "gamepad";
        }

        if (down)
        {
            state.Buttons = static_cast<GamepadButtons>(
                Bits(state.Buttons) | Bits(button)
            );
        }
        else
        {
            state.Buttons = static_cast<GamepadButtons>(
                Bits(state.Buttons) & ~Bits(button)
            );
        }

        GamepadInput::State = state;
        return true;
    }

    bool GamepadBridge::HandleMotion(
        jobject event,
        JNIEnv* env
    )
    {
        if (event == nullptr
            || !IsGamepad(GetSource(env, event))
            || GetAction(env, event) != AMOTION_EVENT_ACTION_MOVE)
        {
            return false;
        }

        const jmethodID getAxisValue = GetMethod(
            env, event, "getAxisValue", "(I)F"
        );

        GamepadState state = GamepadInput::State;
        state.Connected = true;
        if (!state.Name.has_value())
        {
            state.Name = "gamepad";
        }

        state.LeftX = GetAxisValue(
            env, event, getAxisValue, AMOTION_EVENT_AXIS_X
        );
        state.LeftY = -GetAxisValue(
            env, event, getAxisValue, AMOTION_EVENT_AXIS_Y
        );
        state.RightX = Pick(
            env,
            event,
            getAxisValue,
            AMOTION_EVENT_AXIS_Z,
            AMOTION_EVENT_AXIS_RX
        );
        state.RightY = -Pick(
            env,
            event,
            getAxisValue,
            AMOTION_EVENT_AXIS_RZ,
            AMOTION_EVENT_AXIS_RY
        );
        state.LeftTrigger = Pick(
            env,
            event,
            getAxisValue,
            AMOTION_EVENT_AXIS_LTRIGGER,
            AMOTION_EVENT_AXIS_BRAKE
        );
        state.RightTrigger = Pick(
            env,
            event,
            getAxisValue,
            AMOTION_EVENT_AXIS_RTRIGGER,
            AMOTION_EVENT_AXIS_GAS
        );

        GamepadButtons buttons = static_cast<GamepadButtons>(
            Bits(state.Buttons)
            & ~(Bits(GamepadButtons::LeftTrigger)
                | Bits(GamepadButtons::RightTrigger)
                | Bits(GamepadButtons::DpadUp)
                | Bits(GamepadButtons::DpadDown)
                | Bits(GamepadButtons::DpadLeft)
                | Bits(GamepadButtons::DpadRight))
        );

        if (state.LeftTrigger > TriggerPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::LeftTrigger)
            );
        }
        if (state.RightTrigger > TriggerPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::RightTrigger)
            );
        }

        const float hatX = GetAxisValue(
            env, event, getAxisValue, AMOTION_EVENT_AXIS_HAT_X
        );
        const float hatY = GetAxisValue(
            env, event, getAxisValue, AMOTION_EVENT_AXIS_HAT_Y
        );

        if (hatX < -HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadLeft)
            );
        }
        else if (hatX > HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadRight)
            );
        }

        if (hatY < -HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadUp)
            );
        }
        else if (hatY > HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadDown)
            );
        }

        state.Buttons = buttons;
        GamepadInput::State = state;
        return true;
    }
}

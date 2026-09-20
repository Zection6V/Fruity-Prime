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

    bool TryGetMethod(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        jmethodID& method
    )
    {
        jclass type = env->GetObjectClass(object);
        if (type == nullptr)
        {
            return false;
        }

        if (env->ExceptionCheck())
        {
            env->DeleteLocalRef(type);
            return false;
        }

        method = env->GetMethodID(type, name, signature);
        env->DeleteLocalRef(type);
        return method != nullptr && !env->ExceptionCheck();
    }

    bool TryGetInt(
        JNIEnv* env,
        jobject event,
        const char* name,
        std::int32_t& value
    )
    {
        jmethodID method = nullptr;
        if (!TryGetMethod(env, event, name, "()I", method))
        {
            return false;
        }

        const jint result = env->CallIntMethod(event, method);
        if (env->ExceptionCheck())
        {
            return false;
        }

        value = static_cast<std::int32_t>(result);
        return true;
    }

    bool TryGetAxisValue(
        JNIEnv* env,
        jobject event,
        jmethodID method,
        std::int32_t axis,
        float& value
    )
    {
        const jfloat result = env->CallFloatMethod(
            event, method, static_cast<jint>(axis)
        );
        if (env->ExceptionCheck())
        {
            return false;
        }

        value = static_cast<float>(result);
        return true;
    }

    bool Pick(
        JNIEnv* env,
        jobject event,
        jmethodID getAxisValue,
        std::int32_t first,
        std::int32_t second,
        float& result
    )
    {
        float value = 0.0F;
        if (!TryGetAxisValue(env, event, getAxisValue, first, value))
        {
            return false;
        }

        if (value != 0.0F)
        {
            result = value;
            return true;
        }

        return TryGetAxisValue(
            env, event, getAxisValue, second, result
        );
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
        if (event == nullptr)
        {
            return false;
        }

        std::int32_t source = 0;
        if (!TryGetInt(env, event, "getSource", source)
            || !IsGamepad(source))
        {
            return false;
        }

        const GamepadButtons button = Map(keyCode);
        if (button == GamepadButtons::None)
        {
            return false;
        }

        if (down)
        {
            std::int32_t repeatCount = 0;
            if (!TryGetInt(env, event, "getRepeatCount", repeatCount))
            {
                return false;
            }
            if (repeatCount > 0)
            {
                return true;
            }
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
        if (event == nullptr)
        {
            return false;
        }

        std::int32_t source = 0;
        if (!TryGetInt(env, event, "getSource", source)
            || !IsGamepad(source))
        {
            return false;
        }

        std::int32_t action = 0;
        if (!TryGetInt(env, event, "getAction", action)
            || action != AMOTION_EVENT_ACTION_MOVE)
        {
            return false;
        }

        jmethodID getAxisValue = nullptr;
        if (!TryGetMethod(
                env, event, "getAxisValue", "(I)F", getAxisValue
            ))
        {
            return false;
        }

        GamepadState state = GamepadInput::State;
        state.Connected = true;
        if (!state.Name.has_value())
        {
            state.Name = "gamepad";
        }

        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_X, state.LeftX
            ))
        {
            return false;
        }

        float value = 0.0F;
        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_Y, value
            ))
        {
            return false;
        }
        state.LeftY = -value;

        if (!Pick(
                env,
                event,
                getAxisValue,
                AMOTION_EVENT_AXIS_Z,
                AMOTION_EVENT_AXIS_RX,
                state.RightX
            ))
        {
            return false;
        }

        if (!Pick(
                env,
                event,
                getAxisValue,
                AMOTION_EVENT_AXIS_RZ,
                AMOTION_EVENT_AXIS_RY,
                value
            ))
        {
            return false;
        }
        state.RightY = -value;

        if (!Pick(
                env,
                event,
                getAxisValue,
                AMOTION_EVENT_AXIS_LTRIGGER,
                AMOTION_EVENT_AXIS_BRAKE,
                state.LeftTrigger
            ))
        {
            return false;
        }

        if (!Pick(
                env,
                event,
                getAxisValue,
                AMOTION_EVENT_AXIS_RTRIGGER,
                AMOTION_EVENT_AXIS_GAS,
                state.RightTrigger
            ))
        {
            return false;
        }

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

        float hatX = 0.0F;
        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_HAT_X, hatX
            ))
        {
            return false;
        }

        float hatY = 0.0F;
        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_HAT_Y, hatY
            ))
        {
            return false;
        }

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

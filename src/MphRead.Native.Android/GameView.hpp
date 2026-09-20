#pragma once

#if !defined(__ANDROID__)
#error "GameView is only valid for the Android native target."
#endif

#include "AndroidInput.hpp"

#include <cstdint>
#include <functional>
#include <jni.h>
#include <memory>
#include <string>

namespace OpenTK::Mathematics
{
    struct Vector2i;
}

namespace MphRead
{
    class Scene;
}

namespace MphRead::Droid
{
    class TouchControls;

    // Native policy peer for the Android SurfaceView subclass represented by
    // GameView.cs. The real Android host must create an actual SurfaceView
    // object which also implements SurfaceHolder.Callback and forwards the
    // framework callbacks below to this peer. No Activity, looper, Java view
    // hierarchy, or lifecycle is fabricated here.
    class GameView final
    {
    public:
        using Build = std::function<std::unique_ptr<MphRead::Scene>(
            AndroidInput&,
            OpenTK::Mathematics::Vector2i
        )>;
        using Action = std::function<void()>;
        using ErrorAction = std::function<void(std::string)>;
        using BoolAction = std::function<void(bool)>;

        GameView(
            JNIEnv* env,
            jobject view,
            TouchControls& controls,
            std::shared_ptr<AndroidInput> input,
            Build build,
            Action onEnd,
            Action onLoaded,
            ErrorAction onError,
            Action onPauseMenu,
            BoolAction onSoftKeyboard
        );
        ~GameView();

        GameView(const GameView&) = delete;
        GameView& operator=(const GameView&) = delete;
        GameView(GameView&&) = delete;
        GameView& operator=(GameView&&) = delete;

        [[nodiscard]] bool OnCheckIsTextEditor() const;
        [[nodiscard]] jobject OnCreateInputConnection(
            JNIEnv* env,
            jobject outAttrs
        );

        [[nodiscard]] bool OnKeyDown(
            JNIEnv* env,
            std::int32_t keyCode,
            jobject event
        );
        [[nodiscard]] bool OnKeyUp(
            JNIEnv* env,
            std::int32_t keyCode,
            jobject event
        );
        [[nodiscard]] bool OnGenericMotionEvent(
            JNIEnv* env,
            jobject event
        );

        [[nodiscard]] MphRead::Scene* Scene() const noexcept;

        void Stop();
        void OnPause();
        void OnResume();

        void SurfaceCreated(JNIEnv* env, jobject holder);
        void SurfaceChanged(
            JNIEnv* env,
            jobject holder,
            std::int32_t format,
            std::int32_t width,
            std::int32_t height
        );
        void SurfaceDestroyed(JNIEnv* env, jobject holder);

    private:
        class ViewTarget;
        class RenderLoop;

        [[nodiscard]] bool HandleKey(
            JNIEnv* env,
            std::int32_t keyCode,
            jobject event
        );
        [[nodiscard]] static AndroidInput::Keys Map(
            std::int32_t keyCode
        ) noexcept;

        std::shared_ptr<ViewTarget> _view;
        std::shared_ptr<RenderLoop> _loop;
        std::int32_t _keyTaken = 0;
    };
}

#pragma once

#if !defined(__ANDROID__)
#error "TouchOverlayView is only valid for the Android native target."
#endif

#include "TouchControls.hpp"

#include <cstdint>
#include <jni.h>
#include <memory>

namespace MphRead::Droid
{
    class TouchOverlayView final
    {
    public:
        // Native peer for the Java Android.View subclass. The Java side is a
        // mechanical callback surface only; all TouchOverlayView policy stays
        // here exactly as in TouchOverlayView.cs.
        TouchOverlayView(JNIEnv* env, jobject view, TouchControls* controls);
        TouchOverlayView(const TouchOverlayView&) = delete;
        TouchOverlayView& operator=(const TouchOverlayView&) = delete;
        TouchOverlayView(TouchOverlayView&&) = delete;
        TouchOverlayView& operator=(TouchOverlayView&&) = delete;
        ~TouchOverlayView();

        void Refresh();

        void OnSizeChanged(
            JNIEnv* env,
            std::int32_t w,
            std::int32_t h,
            std::int32_t oldw,
            std::int32_t oldh
        );

        void OnDraw(JNIEnv* env, jobject canvas);

        [[nodiscard]] bool OnTouchEvent(JNIEnv* env, jobject event);

    private:
        class ViewTarget;

        [[nodiscard]] jobject View() const noexcept;

        std::shared_ptr<ViewTarget> _view;
        TouchControls* _controls = nullptr;

        jobject _fill = nullptr;
        jobject _stroke = nullptr;
        jobject _text = nullptr;
    };
}

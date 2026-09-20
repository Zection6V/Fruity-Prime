#include "TouchOverlayView.hpp"

#include "../MphRead.Native/Formats/Types.hpp"

#include <android/input.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    template <typename T>
    class LocalRef final
    {
    public:
        LocalRef() = default;

        LocalRef(JNIEnv* env, T value) noexcept
            : _env(env), _value(value)
        {
        }

        ~LocalRef()
        {
            Reset();
        }

        LocalRef(const LocalRef&) = delete;
        LocalRef& operator=(const LocalRef&) = delete;

        LocalRef(LocalRef&& other) noexcept
            : _env(other._env), _value(other._value)
        {
            other._env = nullptr;
            other._value = nullptr;
        }

        LocalRef& operator=(LocalRef&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                _env = other._env;
                _value = other._value;
                other._env = nullptr;
                other._value = nullptr;
            }
            return *this;
        }

        [[nodiscard]] T Get() const noexcept
        {
            return _value;
        }

        explicit operator bool() const noexcept
        {
            return _value != nullptr;
        }

        void Reset() noexcept
        {
            if (_env != nullptr && _value != nullptr)
            {
                _env->DeleteLocalRef(_value);
            }
            _env = nullptr;
            _value = nullptr;
        }

    private:
        JNIEnv* _env = nullptr;
        T _value = nullptr;
    };

    class ScopedJniEnv final
    {
    public:
        explicit ScopedJniEnv(JavaVM* vm)
            : _vm(vm)
        {
            if (_vm == nullptr)
            {
                throw std::runtime_error("Android Java VM is not available");
            }

            const jint result = _vm->GetEnv(
                reinterpret_cast<void**>(&_env),
                JNI_VERSION_1_6
            );
            if (result == JNI_EDETACHED)
            {
                if (_vm->AttachCurrentThread(&_env, nullptr) != JNI_OK)
                {
                    throw std::runtime_error(
                        "could not attach the current thread to the Android Java VM"
                    );
                }
                _attached = true;
            }
            else if (result != JNI_OK || _env == nullptr)
            {
                throw std::runtime_error(
                    "could not obtain the Android JNI environment"
                );
            }
        }

        ~ScopedJniEnv()
        {
            if (_attached)
            {
                _vm->DetachCurrentThread();
            }
        }

        ScopedJniEnv(const ScopedJniEnv&) = delete;
        ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

        [[nodiscard]] JNIEnv* Get() const noexcept
        {
            return _env;
        }

    private:
        JavaVM* _vm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    std::string JavaStringToUtf8(JNIEnv* env, jstring value)
    {
        if (value == nullptr)
        {
            return {};
        }

        const jsize length = env->GetStringLength(value);
        if (env->ExceptionCheck())
        {
            return {};
        }

        const jchar* chars = env->GetStringChars(value, nullptr);
        if (chars == nullptr)
        {
            return {};
        }

        std::string result;
        result.reserve(static_cast<std::size_t>(length));
        for (jsize i = 0; i < length; ++i)
        {
            std::uint32_t codePoint = chars[i];
            if (codePoint >= 0xD800u && codePoint <= 0xDBFFu
                && i + 1 < length)
            {
                const std::uint32_t low = chars[i + 1];
                if (low >= 0xDC00u && low <= 0xDFFFu)
                {
                    codePoint = 0x10000u
                        + ((codePoint - 0xD800u) << 10)
                        + (low - 0xDC00u);
                    ++i;
                }
            }

            if (codePoint <= 0x7Fu)
            {
                result.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FFu)
            {
                result.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
                result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
            else if (codePoint <= 0xFFFFu)
            {
                result.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
                result.push_back(
                    static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu))
                );
                result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
                result.push_back(
                    static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu))
                );
                result.push_back(
                    static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu))
                );
                result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
        }

        env->ReleaseStringChars(value, chars);
        return result;
    }

    [[noreturn]] void ThrowPendingJavaException(JNIEnv* env)
    {
        LocalRef<jthrowable> throwable(
            env,
            static_cast<jthrowable>(env->ExceptionOccurred())
        );
        env->ExceptionClear();

        std::string message = "Android Java exception";
        if (throwable)
        {
            LocalRef<jclass> throwableClass(
                env,
                env->GetObjectClass(throwable.Get())
            );
            if (throwableClass)
            {
                const jmethodID toString = env->GetMethodID(
                    throwableClass.Get(),
                    "toString",
                    "()Ljava/lang/String;"
                );
                if (toString != nullptr && !env->ExceptionCheck())
                {
                    LocalRef<jstring> text(
                        env,
                        static_cast<jstring>(
                            env->CallObjectMethod(throwable.Get(), toString)
                        )
                    );
                    if (!env->ExceptionCheck() && text)
                    {
                        message = JavaStringToUtf8(env, text.Get());
                    }
                }
            }
        }

        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
        }
        throw std::runtime_error(message);
    }

    void CheckJavaException(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            ThrowPendingJavaException(env);
        }
    }

    LocalRef<jclass> FindClass(JNIEnv* env, const char* name)
    {
        LocalRef<jclass> result(env, env->FindClass(name));
        CheckJavaException(env);
        if (!result)
        {
            throw std::runtime_error(std::string("Android class not found: ") + name);
        }
        return result;
    }

    LocalRef<jclass> ObjectClass(JNIEnv* env, jobject object)
    {
        LocalRef<jclass> result(env, env->GetObjectClass(object));
        CheckJavaException(env);
        if (!result)
        {
            throw std::runtime_error("Android object class is not available");
        }
        return result;
    }

    jmethodID GetMethodId(
        JNIEnv* env,
        jclass type,
        const char* name,
        const char* signature
    )
    {
        const jmethodID result = env->GetMethodID(type, name, signature);
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::runtime_error(std::string("Android method not found: ") + name);
        }
        return result;
    }

    jfieldID GetFieldId(
        JNIEnv* env,
        jclass type,
        const char* name,
        const char* signature
    )
    {
        const jfieldID result = env->GetFieldID(type, name, signature);
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::runtime_error(std::string("Android field not found: ") + name);
        }
        return result;
    }

    jfieldID GetStaticFieldId(
        JNIEnv* env,
        jclass type,
        const char* name,
        const char* signature
    )
    {
        const jfieldID result = env->GetStaticFieldID(type, name, signature);
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::runtime_error(
                std::string("Android static field not found: ") + name
            );
        }
        return result;
    }

    jobject NewGlobalRefChecked(JNIEnv* env, jobject value)
    {
        jobject result = env->NewGlobalRef(value);
        CheckJavaException(env);
        if (value != nullptr && result == nullptr)
        {
            throw std::bad_alloc();
        }
        return result;
    }

    void DeleteGlobalRefNoThrow(JavaVM* vm, jobject value) noexcept
    {
        if (vm == nullptr || value == nullptr)
        {
            return;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        const jint result = vm->GetEnv(
            reinterpret_cast<void**>(&env),
            JNI_VERSION_1_6
        );
        if (result == JNI_EDETACHED)
        {
            if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            {
                return;
            }
            attached = true;
        }
        else if (result != JNI_OK || env == nullptr)
        {
            return;
        }

        env->DeleteGlobalRef(value);
        if (attached)
        {
            vm->DetachCurrentThread();
        }
    }

    std::u16string Utf8ToUtf16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());

        std::size_t i = 0;
        while (i < value.size())
        {
            const std::uint8_t first = static_cast<std::uint8_t>(value[i]);
            std::uint32_t codePoint;
            std::size_t length;
            std::uint32_t minimum;

            if (first <= 0x7Fu)
            {
                codePoint = first;
                length = 1;
                minimum = 0;
            }
            else if ((first & 0xE0u) == 0xC0u)
            {
                codePoint = first & 0x1Fu;
                length = 2;
                minimum = 0x80u;
            }
            else if ((first & 0xF0u) == 0xE0u)
            {
                codePoint = first & 0x0Fu;
                length = 3;
                minimum = 0x800u;
            }
            else if ((first & 0xF8u) == 0xF0u)
            {
                codePoint = first & 0x07u;
                length = 4;
                minimum = 0x10000u;
            }
            else
            {
                codePoint = 0xFFFDu;
                length = 1;
                minimum = 0;
            }

            bool valid = i + length <= value.size();
            if (valid && length > 1)
            {
                for (std::size_t j = 1; j < length; ++j)
                {
                    const std::uint8_t next =
                        static_cast<std::uint8_t>(value[i + j]);
                    if ((next & 0xC0u) != 0x80u)
                    {
                        valid = false;
                        break;
                    }
                    codePoint = (codePoint << 6) | (next & 0x3Fu);
                }
                if (codePoint < minimum
                    || codePoint > 0x10FFFFu
                    || (codePoint >= 0xD800u && codePoint <= 0xDFFFu))
                {
                    valid = false;
                }
            }

            if (!valid)
            {
                codePoint = 0xFFFDu;
                length = 1;
            }

            if (codePoint <= 0xFFFFu)
            {
                result.push_back(static_cast<char16_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000u;
                result.push_back(
                    static_cast<char16_t>(0xD800u + (codePoint >> 10))
                );
                result.push_back(
                    static_cast<char16_t>(0xDC00u + (codePoint & 0x3FFu))
                );
            }
            i += length;
        }

        return result;
    }

    LocalRef<jstring> NewJavaString(JNIEnv* env, std::string_view value)
    {
        const std::u16string utf16 = Utf8ToUtf16(value);
        if (utf16.size()
            > static_cast<std::size_t>(std::numeric_limits<jsize>::max()))
        {
            throw std::length_error("string is too long for Android JNI");
        }

        static const jchar empty = 0;
        const jchar* chars = utf16.empty()
            ? &empty
            : reinterpret_cast<const jchar*>(utf16.data());

        LocalRef<jstring> result(
            env,
            env->NewString(chars, static_cast<jsize>(utf16.size()))
        );
        CheckJavaException(env);
        if (!result)
        {
            throw std::bad_alloc();
        }
        return result;
    }

    constexpr std::uint32_t ArgbBits(
        std::uint8_t a,
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b
    ) noexcept
    {
        return (static_cast<std::uint32_t>(a) << 24)
            | (static_cast<std::uint32_t>(r) << 16)
            | (static_cast<std::uint32_t>(g) << 8)
            | static_cast<std::uint32_t>(b);
    }

    jint AsJint(std::uint32_t value) noexcept
    {
        static_assert(sizeof(jint) == sizeof(std::uint32_t));
        return std::bit_cast<jint>(value);
    }

    constexpr std::uint32_t Edge = ArgbBits(150, 38, 46, 60);
    constexpr std::uint32_t Panel = ArgbBits(70, 26, 31, 41);
    constexpr std::uint32_t Accent = ArgbBits(210, 41, 197, 255);
    constexpr std::uint32_t AccentFill = ArgbBits(90, 41, 197, 255);
    constexpr std::uint32_t Label = ArgbBits(190, 138, 147, 166);

    void CallViewVoid(JNIEnv* env, jobject view, const char* name)
    {
        LocalRef<jclass> type = ObjectClass(env, view);
        const jmethodID method = GetMethodId(env, type.Get(), name, "()V");
        env->CallVoidMethod(view, method);
        CheckJavaException(env);
    }

    jint GetViewInt(JNIEnv* env, jobject view, const char* name)
    {
        LocalRef<jclass> type = ObjectClass(env, view);
        const jmethodID method = GetMethodId(env, type.Get(), name, "()I");
        const jint value = env->CallIntMethod(view, method);
        CheckJavaException(env);
        return value;
    }

    float GetDensity(JNIEnv* env, jobject view)
    {
        LocalRef<jclass> viewClass = ObjectClass(env, view);
        const jmethodID getResources = GetMethodId(
            env,
            viewClass.Get(),
            "getResources",
            "()Landroid/content/res/Resources;"
        );
        LocalRef<jobject> resources(
            env,
            env->CallObjectMethod(view, getResources)
        );
        CheckJavaException(env);
        if (!resources)
        {
            return 1.0F;
        }

        LocalRef<jclass> resourcesClass = ObjectClass(env, resources.Get());
        const jmethodID getDisplayMetrics = GetMethodId(
            env,
            resourcesClass.Get(),
            "getDisplayMetrics",
            "()Landroid/util/DisplayMetrics;"
        );
        LocalRef<jobject> metrics(
            env,
            env->CallObjectMethod(resources.Get(), getDisplayMetrics)
        );
        CheckJavaException(env);
        if (!metrics)
        {
            return 1.0F;
        }

        LocalRef<jclass> metricsClass = ObjectClass(env, metrics.Get());
        const jfieldID densityField = GetFieldId(
            env,
            metricsClass.Get(),
            "density",
            "F"
        );
        const jfloat density = env->GetFloatField(metrics.Get(), densityField);
        CheckJavaException(env);
        return density;
    }

    jobject CreatePaint(JNIEnv* env)
    {
        LocalRef<jclass> paintClass = FindClass(env, "android/graphics/Paint");
        const jmethodID ctor = GetMethodId(env, paintClass.Get(), "<init>", "(I)V");
        LocalRef<jobject> local(
            env,
            env->NewObject(paintClass.Get(), ctor, static_cast<jint>(1))
        );
        CheckJavaException(env);
        if (!local)
        {
            throw std::bad_alloc();
        }
        return NewGlobalRefChecked(env, local.Get());
    }

    void SetPaintStyle(
        JNIEnv* env,
        jobject paint,
        const char* fieldName
    )
    {
        LocalRef<jclass> paintClass = FindClass(env, "android/graphics/Paint");
        LocalRef<jclass> styleClass = FindClass(
            env,
            "android/graphics/Paint$Style"
        );
        const jfieldID field = GetStaticFieldId(
            env,
            styleClass.Get(),
            fieldName,
            "Landroid/graphics/Paint$Style;"
        );
        LocalRef<jobject> style(
            env,
            env->GetStaticObjectField(styleClass.Get(), field)
        );
        CheckJavaException(env);

        const jmethodID method = GetMethodId(
            env,
            paintClass.Get(),
            "setStyle",
            "(Landroid/graphics/Paint$Style;)V"
        );
        env->CallVoidMethod(paint, method, style.Get());
        CheckJavaException(env);
    }

    void SetTextAlignCenter(JNIEnv* env, jobject paint)
    {
        LocalRef<jclass> paintClass = FindClass(env, "android/graphics/Paint");
        LocalRef<jclass> alignClass = FindClass(
            env,
            "android/graphics/Paint$Align"
        );
        const jfieldID field = GetStaticFieldId(
            env,
            alignClass.Get(),
            "CENTER",
            "Landroid/graphics/Paint$Align;"
        );
        LocalRef<jobject> align(
            env,
            env->GetStaticObjectField(alignClass.Get(), field)
        );
        CheckJavaException(env);

        const jmethodID method = GetMethodId(
            env,
            paintClass.Get(),
            "setTextAlign",
            "(Landroid/graphics/Paint$Align;)V"
        );
        env->CallVoidMethod(paint, method, align.Get());
        CheckJavaException(env);
    }

    void SetPaintFloat(
        JNIEnv* env,
        jobject paint,
        const char* name,
        jfloat value
    )
    {
        LocalRef<jclass> paintClass = ObjectClass(env, paint);
        const jmethodID method = GetMethodId(
            env,
            paintClass.Get(),
            name,
            "(F)V"
        );
        env->CallVoidMethod(paint, method, value);
        CheckJavaException(env);
    }

    void SetPaintColor(JNIEnv* env, jobject paint, std::uint32_t color)
    {
        LocalRef<jclass> paintClass = ObjectClass(env, paint);
        const jmethodID method = GetMethodId(
            env,
            paintClass.Get(),
            "setColor",
            "(I)V"
        );
        env->CallVoidMethod(paint, method, AsJint(color));
        CheckJavaException(env);
    }

    jfloat GetPaintTextSize(JNIEnv* env, jobject paint)
    {
        LocalRef<jclass> paintClass = ObjectClass(env, paint);
        const jmethodID method = GetMethodId(
            env,
            paintClass.Get(),
            "getTextSize",
            "()F"
        );
        const jfloat result = env->CallFloatMethod(paint, method);
        CheckJavaException(env);
        return result;
    }

    void DrawCircle(
        JNIEnv* env,
        jobject canvas,
        float x,
        float y,
        float radius,
        jobject paint
    )
    {
        LocalRef<jclass> canvasClass = ObjectClass(env, canvas);
        const jmethodID method = GetMethodId(
            env,
            canvasClass.Get(),
            "drawCircle",
            "(FFFLandroid/graphics/Paint;)V"
        );
        env->CallVoidMethod(canvas, method, x, y, radius, paint);
        CheckJavaException(env);
    }

    void DrawText(
        JNIEnv* env,
        jobject canvas,
        std::string_view text,
        float x,
        float y,
        jobject paint
    )
    {
        LocalRef<jclass> canvasClass = ObjectClass(env, canvas);
        const jmethodID method = GetMethodId(
            env,
            canvasClass.Get(),
            "drawText",
            "(Ljava/lang/String;FFLandroid/graphics/Paint;)V"
        );
        LocalRef<jstring> javaText = NewJavaString(env, text);
        env->CallVoidMethod(
            canvas,
            method,
            javaText.Get(),
            x,
            y,
            paint
        );
        CheckJavaException(env);
    }

    jint MotionInt(JNIEnv* env, jobject event, const char* name)
    {
        LocalRef<jclass> type = ObjectClass(env, event);
        const jmethodID method = GetMethodId(env, type.Get(), name, "()I");
        const jint result = env->CallIntMethod(event, method);
        CheckJavaException(env);
        return result;
    }

    jint MotionIntAt(
        JNIEnv* env,
        jobject event,
        const char* name,
        jint index
    )
    {
        LocalRef<jclass> type = ObjectClass(env, event);
        const jmethodID method = GetMethodId(env, type.Get(), name, "(I)I");
        const jint result = env->CallIntMethod(event, method, index);
        CheckJavaException(env);
        return result;
    }

    jfloat MotionFloatAt(
        JNIEnv* env,
        jobject event,
        const char* name,
        jint index
    )
    {
        LocalRef<jclass> type = ObjectClass(env, event);
        const jmethodID method = GetMethodId(env, type.Get(), name, "(I)F");
        const jfloat result = env->CallFloatMethod(event, method, index);
        CheckJavaException(env);
        return result;
    }
}

namespace MphRead::Droid
{
    class TouchOverlayView::ViewTarget final
    {
    public:
        ViewTarget(JNIEnv* env, jobject view)
        {
            if (env == nullptr)
            {
                throw std::invalid_argument(
                    "Android JNI environment must not be null"
                );
            }
            if (view == nullptr)
            {
                throw std::invalid_argument(
                    "Android TouchOverlayView peer must not be null"
                );
            }
            if (env->GetJavaVM(&_vm) != JNI_OK || _vm == nullptr)
            {
                throw std::runtime_error("Android Java VM is not available");
            }
            _view = NewGlobalRefChecked(env, view);
        }

        ~ViewTarget()
        {
            DeleteGlobalRefNoThrow(_vm, _view);
        }

        ViewTarget(const ViewTarget&) = delete;
        ViewTarget& operator=(const ViewTarget&) = delete;

        [[nodiscard]] JavaVM* Vm() const noexcept
        {
            return _vm;
        }

        [[nodiscard]] jobject Object() const noexcept
        {
            return _view;
        }

        void PostInvalidate() const
        {
            ScopedJniEnv scoped(_vm);
            JNIEnv* const env = scoped.Get();
            CallViewVoid(env, _view, "postInvalidate");
        }

    private:
        JavaVM* _vm = nullptr;
        jobject _view = nullptr;
    };

    TouchOverlayView::TouchOverlayView(
        JNIEnv* env,
        jobject view,
        TouchControls* controls
    )
        : _view(std::make_shared<ViewTarget>(env, view)),
          _controls(controls)
    {
        jobject fill = nullptr;
        jobject stroke = nullptr;
        jobject text = nullptr;

        try
        {
            fill = CreatePaint(env);
            stroke = CreatePaint(env);
            text = CreatePaint(env);

            _fill = fill;
            _stroke = stroke;
            _text = text;

            if (_controls == nullptr)
            {
                throw System::NullReferenceException();
            }

            const std::shared_ptr<ViewTarget> invalidationTarget = _view;
            _controls->Invalidated(
                [invalidationTarget]()
                {
                    invalidationTarget->PostInvalidate();
                }
            );

            LocalRef<jclass> viewClass = ObjectClass(env, View());
            const jmethodID setWillNotDraw = GetMethodId(
                env,
                viewClass.Get(),
                "setWillNotDraw",
                "(Z)V"
            );
            env->CallVoidMethod(View(), setWillNotDraw, JNI_FALSE);
            CheckJavaException(env);

            SetPaintStyle(env, _stroke, "STROKE");
            SetPaintStyle(env, _fill, "FILL");
            SetPaintStyle(env, _text, "FILL");
            SetTextAlignCenter(env, _text);
        }
        catch (...)
        {
            if (text != nullptr)
            {
                env->DeleteGlobalRef(text);
            }
            if (stroke != nullptr)
            {
                env->DeleteGlobalRef(stroke);
            }
            if (fill != nullptr)
            {
                env->DeleteGlobalRef(fill);
            }
            _fill = nullptr;
            _stroke = nullptr;
            _text = nullptr;
            throw;
        }
    }

    TouchOverlayView::~TouchOverlayView()
    {
        JavaVM* const vm = _view == nullptr ? nullptr : _view->Vm();
        DeleteGlobalRefNoThrow(vm, _text);
        DeleteGlobalRefNoThrow(vm, _stroke);
        DeleteGlobalRefNoThrow(vm, _fill);
        _text = nullptr;
        _stroke = nullptr;
        _fill = nullptr;
    }

    jobject TouchOverlayView::View() const noexcept
    {
        return _view->Object();
    }

    void TouchOverlayView::Refresh()
    {
        ScopedJniEnv scoped(_view->Vm());
        JNIEnv* const env = scoped.Get();

        if (GetViewInt(env, View(), "getWidth") > 0
            && GetViewInt(env, View(), "getHeight") > 0)
        {
            const float width = static_cast<float>(
                GetViewInt(env, View(), "getWidth")
            );
            const float height = static_cast<float>(
                GetViewInt(env, View(), "getHeight")
            );
            const float density = GetDensity(env, View());
            _controls->Layout(width, height, density);
        }

        CallViewVoid(env, View(), "requestLayout");
        CallViewVoid(env, View(), "invalidate");
    }

    void TouchOverlayView::OnSizeChanged(
        JNIEnv* env,
        std::int32_t w,
        std::int32_t h,
        std::int32_t oldw,
        std::int32_t oldh
    )
    {
        LocalRef<jclass> viewClass = FindClass(env, "android/view/View");
        const jmethodID baseOnSizeChanged = GetMethodId(
            env,
            viewClass.Get(),
            "onSizeChanged",
            "(IIII)V"
        );
        env->CallNonvirtualVoidMethod(
            View(),
            viewClass.Get(),
            baseOnSizeChanged,
            static_cast<jint>(w),
            static_cast<jint>(h),
            static_cast<jint>(oldw),
            static_cast<jint>(oldh)
        );
        CheckJavaException(env);

        _controls->Layout(
            static_cast<float>(w),
            static_cast<float>(h),
            GetDensity(env, View())
        );
        CallViewVoid(env, View(), "invalidate");
    }

    void TouchOverlayView::OnDraw(JNIEnv* env, jobject canvas)
    {
        LocalRef<jclass> viewClass = FindClass(env, "android/view/View");
        const jmethodID baseOnDraw = GetMethodId(
            env,
            viewClass.Get(),
            "onDraw",
            "(Landroid/graphics/Canvas;)V"
        );
        env->CallNonvirtualVoidMethod(
            View(),
            viewClass.Get(),
            baseOnDraw,
            canvas
        );
        CheckJavaException(env);

        if (_controls->PadDriving())
        {
            return;
        }

        const float unit = std::max(
            1.0F,
            static_cast<float>(GetViewInt(env, View(), "getHeight")) / 100.0F
        );
        SetPaintFloat(
            env,
            _stroke,
            "setStrokeWidth",
            std::max(2.0F, unit * 0.22F)
        );

        for (const std::shared_ptr<TouchButton>& button : _controls->Buttons())
        {
            if (!button->Visible())
            {
                continue;
            }

            const bool held = _controls->IsHeld(button->Action());
            SetPaintColor(env, _fill, held ? AccentFill : Panel);
            SetPaintColor(env, _stroke, held ? Accent : Edge);

            const float fillX = button->CentreX();
            const float fillY = button->CentreY();
            const float fillRadius = button->Radius();
            DrawCircle(env, canvas, fillX, fillY, fillRadius, _fill);

            const float strokeX = button->CentreX();
            const float strokeY = button->CentreY();
            const float strokeRadius = button->Radius();
            DrawCircle(env, canvas, strokeX, strokeY, strokeRadius, _stroke);

            SetPaintColor(env, _text, held ? Accent : Label);
            const float textRadius = button->Radius();
            SetPaintFloat(
                env,
                _text,
                "setTextSize",
                textRadius * 0.42F
            );

            const std::string label = button->Label();
            const float textX = button->CentreX();
            const float textBaseY = button->CentreY();
            const float textSize = GetPaintTextSize(env, _text);
            DrawText(
                env,
                canvas,
                label,
                textX,
                textBaseY + textSize * 0.35F,
                _text
            );
        }

        if (_controls->StickActive())
        {
            SetPaintColor(env, _stroke, Edge);
            SetPaintColor(env, _fill, Panel);
            const float stickFillX = _controls->StickX();
            const float stickFillY = _controls->StickY();
            const float stickFillRadius = _controls->StickRadius();
            DrawCircle(
                env,
                canvas,
                stickFillX,
                stickFillY,
                stickFillRadius,
                _fill
            );

            const float stickStrokeX = _controls->StickX();
            const float stickStrokeY = _controls->StickY();
            const float stickStrokeRadius = _controls->StickRadius();
            DrawCircle(
                env,
                canvas,
                stickStrokeX,
                stickStrokeY,
                stickStrokeRadius,
                _stroke
            );

            SetPaintColor(env, _fill, AccentFill);
            SetPaintColor(env, _stroke, Accent);

            const float knobFillX = _controls->StickKnobX();
            const float knobFillY = _controls->StickKnobY();
            const float knobFillRadius = _controls->StickKnobRadius();
            DrawCircle(
                env,
                canvas,
                knobFillX,
                knobFillY,
                knobFillRadius,
                _fill
            );

            const float knobStrokeX = _controls->StickKnobX();
            const float knobStrokeY = _controls->StickKnobY();
            const float knobStrokeRadius = _controls->StickKnobRadius();
            DrawCircle(
                env,
                canvas,
                knobStrokeX,
                knobStrokeY,
                knobStrokeRadius,
                _stroke
            );
        }
    }

    bool TouchOverlayView::OnTouchEvent(JNIEnv* env, jobject event)
    {
        if (event == nullptr)
        {
            return false;
        }

        const jint actionMasked = MotionInt(env, event, "getActionMasked");
        switch (actionMasked)
        {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            {
                const jint index = MotionInt(env, event, "getActionIndex");
                const jint pointerId = MotionIntAt(
                    env,
                    event,
                    "getPointerId",
                    index
                );
                const jfloat x = MotionFloatAt(env, event, "getX", index);
                const jfloat y = MotionFloatAt(env, event, "getY", index);
                _controls->PointerDown(pointerId, x, y);
            }
            break;

        case AMOTION_EVENT_ACTION_MOVE:
            for (
                jint i = 0;
                i < MotionInt(env, event, "getPointerCount");
                ++i
            )
            {
                const jint pointerId = MotionIntAt(
                    env,
                    event,
                    "getPointerId",
                    i
                );
                const jfloat x = MotionFloatAt(env, event, "getX", i);
                const jfloat y = MotionFloatAt(env, event, "getY", i);
                _controls->PointerMove(pointerId, x, y);
            }
            break;

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            {
                const jint index = MotionInt(env, event, "getActionIndex");
                const jint pointerId = MotionIntAt(
                    env,
                    event,
                    "getPointerId",
                    index
                );
                _controls->PointerUp(pointerId);
            }
            break;

        case AMOTION_EVENT_ACTION_CANCEL:
            _controls->ReleaseEverything();
            break;

        default:
            return false;
        }

        CallViewVoid(env, View(), "invalidate");
        return true;
    }
}

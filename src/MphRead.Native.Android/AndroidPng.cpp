#include "AndroidPng.hpp"

#if !defined(__ANDROID__)
#error "AndroidPng is only valid for the Android native target."
#endif

#include "../MphRead.Native/Formats/Types.hpp"

#include <jni.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    JavaVM* g_javaVm = nullptr;

    class ScopedJniEnv final
    {
    public:
        ScopedJniEnv()
        {
            if (g_javaVm == nullptr)
            {
                throw std::runtime_error("Android Java VM is not available");
            }

            const jint result = g_javaVm->GetEnv(
                reinterpret_cast<void**>(&_env), JNI_VERSION_1_6
            );
            if (result == JNI_EDETACHED)
            {
                if (g_javaVm->AttachCurrentThread(
                        reinterpret_cast<void**>(&_env), nullptr
                    ) != JNI_OK)
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
                g_javaVm->DetachCurrentThread();
            }
        }

        ScopedJniEnv(const ScopedJniEnv&) = delete;
        ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

        JNIEnv* Get() const noexcept
        {
            return _env;
        }

    private:
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

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

        T Get() const noexcept
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

    std::int32_t WrapAdd(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right)
        );
    }

    std::int32_t WrapSub(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right)
        );
    }

    std::int32_t WrapMul(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right)
        );
    }

    std::uint8_t ArrayAt(
        const std::vector<std::uint8_t>& array,
        std::int32_t index
    )
    {
        if (index < 0 || static_cast<std::size_t>(index) >= array.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return array[static_cast<std::size_t>(index)];
    }

    void ArraySet(
        std::vector<jint>& array,
        std::int32_t index,
        jint value
    )
    {
        if (index < 0 || static_cast<std::size_t>(index) >= array.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        array[static_cast<std::size_t>(index)] = value;
    }

    void AppendUtf8(std::string& output, std::uint32_t value)
    {
        if (value <= 0x7FU)
        {
            output.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (value >> 6)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else if (value <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (value >> 18)));
            output.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    std::string Utf16ToUtf8(std::u16string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            const std::uint32_t first =
                static_cast<std::uint16_t>(value[index]);
            if (first >= 0xD800U && first <= 0xDBFFU)
            {
                if (index + 1 < value.size())
                {
                    const std::uint32_t second =
                        static_cast<std::uint16_t>(value[index + 1]);
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        const std::uint32_t codePoint = 0x10000U
                            + ((first - 0xD800U) << 10)
                            + (second - 0xDC00U);
                        AppendUtf8(result, codePoint);
                        ++index;
                        continue;
                    }
                }
                AppendUtf8(result, 0xFFFDU);
                continue;
            }
            if (first >= 0xDC00U && first <= 0xDFFFU)
            {
                AppendUtf8(result, 0xFFFDU);
                continue;
            }
            AppendUtf8(result, first);
        }
        return result;
    }

    std::u16string Utf8ToUtf16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());

        std::size_t index = 0;
        while (index < value.size())
        {
            const std::uint8_t first =
                static_cast<std::uint8_t>(value[index]);
            if (first <= 0x7FU)
            {
                result.push_back(static_cast<char16_t>(first));
                ++index;
                continue;
            }

            std::uint32_t codePoint = 0;
            std::size_t count = 0;
            if (first >= 0xC2U && first <= 0xDFU)
            {
                codePoint = first & 0x1FU;
                count = 2;
            }
            else if (first >= 0xE0U && first <= 0xEFU)
            {
                codePoint = first & 0x0FU;
                count = 3;
            }
            else if (first >= 0xF0U && first <= 0xF4U)
            {
                codePoint = first & 0x07U;
                count = 4;
            }
            else
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            if (index + count > value.size())
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            bool valid = true;
            for (std::size_t offset = 1; offset < count; ++offset)
            {
                const std::uint8_t next =
                    static_cast<std::uint8_t>(value[index + offset]);
                if ((next & 0xC0U) != 0x80U)
                {
                    valid = false;
                    break;
                }
                codePoint = (codePoint << 6) | (next & 0x3FU);
            }

            if (!valid
                || (count == 3 && codePoint < 0x800U)
                || (count == 4 && codePoint < 0x10000U)
                || codePoint > 0x10FFFFU
                || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000U;
                result.push_back(
                    static_cast<char16_t>(0xD800U + (codePoint >> 10))
                );
                result.push_back(
                    static_cast<char16_t>(0xDC00U + (codePoint & 0x3FFU))
                );
            }
            index += count;
        }

        return result;
    }

    std::string JavaStringToUtf8(JNIEnv* env, jstring value)
    {
        if (value == nullptr)
        {
            return {};
        }

        const jsize length = env->GetStringLength(value);
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return {};
        }

        const jchar* chars = env->GetStringChars(value, nullptr);
        if (env->ExceptionCheck() || chars == nullptr)
        {
            env->ExceptionClear();
            return {};
        }

        std::string result;
        try
        {
            result = Utf16ToUtf8(std::u16string_view(
                reinterpret_cast<const char16_t*>(chars),
                static_cast<std::size_t>(length)
            ));
        }
        catch (...)
        {
            env->ReleaseStringChars(value, chars);
            throw;
        }
        env->ReleaseStringChars(value, chars);
        return result;
    }

    std::string TryThrowableText(
        JNIEnv* env,
        jthrowable throwable,
        const char* methodName
    )
    {
        if (throwable == nullptr)
        {
            return {};
        }

        LocalRef<jclass> throwableClass(env, env->GetObjectClass(throwable));
        if (env->ExceptionCheck() || !throwableClass)
        {
            env->ExceptionClear();
            return {};
        }

        const jmethodID method = env->GetMethodID(
            throwableClass.Get(), methodName, "()Ljava/lang/String;"
        );
        if (env->ExceptionCheck() || method == nullptr)
        {
            env->ExceptionClear();
            return {};
        }

        LocalRef<jstring> text(
            env,
            static_cast<jstring>(env->CallObjectMethod(throwable, method))
        );
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return {};
        }
        return JavaStringToUtf8(env, text.Get());
    }

    [[noreturn]] void ThrowPendingJavaException(JNIEnv* env)
    {
        LocalRef<jthrowable> throwable(env, env->ExceptionOccurred());
        env->ExceptionClear();

        std::string message = TryThrowableText(
            env, throwable.Get(), "getMessage"
        );
        if (message.empty())
        {
            message = TryThrowableText(env, throwable.Get(), "toString");
        }
        if (message.empty())
        {
            message = "Android Java exception";
        }
        throw std::runtime_error(std::move(message));
    }

    void CheckJavaException(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            ThrowPendingJavaException(env);
        }
    }

    LocalRef<jstring> NewJavaString(JNIEnv* env, std::u16string_view value)
    {
        static_assert(sizeof(char16_t) == sizeof(jchar));
        if (value.size()
            > static_cast<std::size_t>(std::numeric_limits<jsize>::max()))
        {
            throw std::length_error("string is too long for Android JNI");
        }

        static const jchar empty = 0;
        const jchar* const chars = value.empty()
            ? &empty
            : reinterpret_cast<const jchar*>(value.data());
        LocalRef<jstring> result(
            env,
            env->NewString(chars, static_cast<jsize>(value.size()))
        );
        CheckJavaException(env);
        if (!result)
        {
            throw std::bad_alloc();
        }
        return result;
    }

    LocalRef<jclass> FindClass(JNIEnv* env, const char* name)
    {
        LocalRef<jclass> result(env, env->FindClass(name));
        CheckJavaException(env);
        if (!result)
        {
            throw std::runtime_error(
                std::string("Android class not found: ") + name
            );
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
        const jfieldID result =
            env->GetStaticFieldID(type, name, signature);
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::runtime_error(
                std::string("Android static field not found: ") + name
            );
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
            throw std::runtime_error(
                std::string("Android method not found: ") + name
            );
        }
        return result;
    }

    jmethodID GetStaticMethodId(
        JNIEnv* env,
        jclass type,
        const char* name,
        const char* signature
    )
    {
        const jmethodID result =
            env->GetStaticMethodID(type, name, signature);
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::runtime_error(
                std::string("Android static method not found: ") + name
            );
        }
        return result;
    }
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*)
{
    g_javaVm = vm;
    return JNI_VERSION_1_6;
}

namespace MphRead::Droid
{
    void AndroidPng::Write(
        std::vector<std::uint8_t>& rgb,
        std::int32_t width,
        std::int32_t height,
        const std::string& path
    )
    {
        static_assert(sizeof(jint) == sizeof(std::uint32_t));

        const std::int32_t pixelCount = WrapMul(width, height);
        if (pixelCount < 0)
        {
            throw System::OverflowException();
        }

        std::vector<jint> pixels(static_cast<std::size_t>(pixelCount));
        for (std::int32_t y = 0; y < height; ++y)
        {
            const std::int32_t source = WrapMul(
                WrapMul(WrapSub(WrapSub(height, 1), y), width),
                3
            );
            const std::int32_t target = WrapMul(y, width);

            for (std::int32_t x = 0; x < width; ++x)
            {
                const std::int32_t i =
                    WrapAdd(source, WrapMul(x, 3));
                const std::uint32_t red = ArrayAt(rgb, i);
                const std::uint32_t green =
                    ArrayAt(rgb, WrapAdd(i, 1));
                const std::uint32_t blue =
                    ArrayAt(rgb, WrapAdd(i, 2));
                const std::uint32_t argb = 0xFF000000U
                    | (red << 16) | (green << 8) | blue;

                ArraySet(
                    pixels,
                    WrapAdd(target, x),
                    std::bit_cast<jint>(argb)
                );
            }
        }

        ScopedJniEnv scopedEnv;
        JNIEnv* const env = scopedEnv.Get();

        LocalRef<jintArray> javaPixels(
            env,
            env->NewIntArray(static_cast<jsize>(pixelCount))
        );
        CheckJavaException(env);
        if (!javaPixels)
        {
            throw std::bad_alloc();
        }
        if (pixelCount != 0)
        {
            env->SetIntArrayRegion(
                javaPixels.Get(),
                0,
                static_cast<jsize>(pixelCount),
                pixels.data()
            );
            CheckJavaException(env);
        }

        LocalRef<jclass> configClass =
            FindClass(env, "android/graphics/Bitmap$Config");
        const jfieldID argb8888Field = GetStaticFieldId(
            env,
            configClass.Get(),
            "ARGB_8888",
            "Landroid/graphics/Bitmap$Config;"
        );
        LocalRef<jobject> argb8888(
            env,
            env->GetStaticObjectField(
                configClass.Get(), argb8888Field
            )
        );
        CheckJavaException(env);

        LocalRef<jclass> bitmapClass =
            FindClass(env, "android/graphics/Bitmap");
        const jmethodID createBitmap = GetStaticMethodId(
            env,
            bitmapClass.Get(),
            "createBitmap",
            "([IIILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"
        );
        LocalRef<jobject> bitmap(
            env,
            env->CallStaticObjectMethod(
                bitmapClass.Get(),
                createBitmap,
                javaPixels.Get(),
                static_cast<jint>(width),
                static_cast<jint>(height),
                argb8888.Get()
            )
        );
        CheckJavaException(env);

        if (!bitmap)
        {
            throw std::runtime_error(
                "the bitmap could not be allocated"
            );
        }

        LocalRef<jclass> fileOutputStreamClass =
            FindClass(env, "java/io/FileOutputStream");
        const jmethodID fileOutputStreamConstructor = GetMethodId(
            env,
            fileOutputStreamClass.Get(),
            "<init>",
            "(Ljava/lang/String;)V"
        );
        const std::u16string pathUtf16 = Utf8ToUtf16(path);
        LocalRef<jstring> javaPath =
            NewJavaString(env, pathUtf16);
        LocalRef<jobject> stream(
            env,
            env->NewObject(
                fileOutputStreamClass.Get(),
                fileOutputStreamConstructor,
                javaPath.Get()
            )
        );
        CheckJavaException(env);
        if (!stream)
        {
            throw std::bad_alloc();
        }

        std::exception_ptr bodyException;
        try
        {
            LocalRef<jclass> compressFormatClass = FindClass(
                env, "android/graphics/Bitmap$CompressFormat"
            );
            const jfieldID pngField = GetStaticFieldId(
                env,
                compressFormatClass.Get(),
                "PNG",
                "Landroid/graphics/Bitmap$CompressFormat;"
            );
            LocalRef<jobject> png(
                env,
                env->GetStaticObjectField(
                    compressFormatClass.Get(), pngField
                )
            );
            CheckJavaException(env);

            const jmethodID compress = GetMethodId(
                env,
                bitmapClass.Get(),
                "compress",
                "(Landroid/graphics/Bitmap$CompressFormat;ILjava/io/OutputStream;)Z"
            );
            (void)env->CallBooleanMethod(
                bitmap.Get(),
                compress,
                png.Get(),
                static_cast<jint>(100),
                stream.Get()
            );
            CheckJavaException(env);
        }
        catch (...)
        {
            bodyException = std::current_exception();
        }

        LocalRef<jclass> outputStreamClass =
            FindClass(env, "java/io/OutputStream");
        const jmethodID close = GetMethodId(
            env, outputStreamClass.Get(), "close", "()V"
        );
        env->CallVoidMethod(stream.Get(), close);
        CheckJavaException(env);

        if (bodyException)
        {
            std::rethrow_exception(bodyException);
        }
    }
}

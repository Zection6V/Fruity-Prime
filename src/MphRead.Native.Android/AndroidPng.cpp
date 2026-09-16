#include "AndroidPng.hpp"

#if !defined(__ANDROID__)
#error "AndroidPng is only valid for the Android native target."
#endif

#include <jni.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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

            const jint result = g_javaVm->GetEnv(reinterpret_cast<void**>(&_env), JNI_VERSION_1_6);
            if (result == JNI_EDETACHED)
            {
                if (g_javaVm->AttachCurrentThread(reinterpret_cast<void**>(&_env), nullptr) != JNI_OK)
                {
                    throw std::runtime_error("could not attach the current thread to the Android Java VM");
                }
                _attached = true;
            }
            else if (result != JNI_OK || _env == nullptr)
            {
                throw std::runtime_error("could not obtain the Android JNI environment");
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

    std::string JavaThrowableText(JNIEnv* env, jthrowable throwable)
    {
        if (throwable == nullptr)
        {
            return "Android Java exception";
        }

        LocalRef<jclass> throwableClass(env, env->GetObjectClass(throwable));
        if (env->ExceptionCheck() || !throwableClass)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }

        const jmethodID toString = env->GetMethodID(
            throwableClass.Get(), "toString", "()Ljava/lang/String;"
        );
        if (env->ExceptionCheck() || toString == nullptr)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }

        LocalRef<jstring> text(
            env,
            static_cast<jstring>(env->CallObjectMethod(throwable, toString))
        );
        if (env->ExceptionCheck() || !text)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }

        const char* chars = env->GetStringUTFChars(text.Get(), nullptr);
        if (env->ExceptionCheck() || chars == nullptr)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }
        std::string result(chars);
        env->ReleaseStringUTFChars(text.Get(), chars);
        return result;
    }

    [[noreturn]] void ThrowPendingJavaException(JNIEnv* env)
    {
        LocalRef<jthrowable> throwable(env, env->ExceptionOccurred());
        env->ExceptionClear();
        throw std::runtime_error(JavaThrowableText(env, throwable.Get()));
    }

    void CheckJavaException(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            ThrowPendingJavaException(env);
        }
    }

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

    std::uint8_t ArrayAt(std::span<const std::uint8_t> array, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= array.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return array[static_cast<std::size_t>(index)];
    }

    LocalRef<jstring> NewJavaString(JNIEnv* env, std::u16string_view value)
    {
        static_assert(sizeof(char16_t) == sizeof(jchar));
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
            throw std::runtime_error(std::string("Android class not found: ") + name);
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
            throw std::runtime_error(std::string("Android static field not found: ") + name);
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

    jmethodID GetStaticMethodId(
        JNIEnv* env,
        jclass type,
        const char* name,
        const char* signature
    )
    {
        const jmethodID result = env->GetStaticMethodID(type, name, signature);
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::runtime_error(std::string("Android static method not found: ") + name);
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
        std::span<const std::uint8_t> rgb,
        std::int32_t width,
        std::int32_t height,
        std::u16string_view path
    )
    {
        static_assert(sizeof(jint) == sizeof(std::uint32_t));

        ScopedJniEnv scopedEnv;
        JNIEnv* const env = scopedEnv.Get();

        const std::int32_t pixelCount = WrapMul(width, height);
        if (pixelCount < 0)
        {
            throw std::overflow_error("Array dimensions exceeded supported range.");
        }

        LocalRef<jobject> bitmap;
        {
            LocalRef<jintArray> pixels(env, env->NewIntArray(static_cast<jsize>(pixelCount)));
            CheckJavaException(env);
            if (!pixels)
            {
                throw std::bad_alloc();
            }

            jint* pixelData = env->GetIntArrayElements(pixels.Get(), nullptr);
            CheckJavaException(env);
            if (pixelData == nullptr)
            {
                throw std::bad_alloc();
            }

            try
            {
                for (std::int32_t y = 0; y < height; ++y)
                {
                    const std::int32_t source = WrapMul(
                        WrapMul(WrapSub(WrapSub(height, 1), y), width),
                        3
                    );
                    const std::int32_t target = WrapMul(y, width);
                    for (std::int32_t x = 0; x < width; ++x)
                    {
                        const std::int32_t i = WrapAdd(source, WrapMul(x, 3));
                        const std::int32_t targetIndex = WrapAdd(target, x);

                        const std::uint32_t red = ArrayAt(rgb, i);
                        const std::uint32_t green = ArrayAt(rgb, WrapAdd(i, 1));
                        const std::uint32_t blue = ArrayAt(rgb, WrapAdd(i, 2));
                        const std::uint32_t argb = 0xFF000000u
                            | (red << 16) | (green << 8) | blue;

                        if (targetIndex < 0 || targetIndex >= pixelCount)
                        {
                            throw std::out_of_range("Index was outside the bounds of the array.");
                        }
                        pixelData[targetIndex] = std::bit_cast<jint>(argb);
                    }
                }
            }
            catch (...)
            {
                env->ReleaseIntArrayElements(pixels.Get(), pixelData, 0);
                throw;
            }
            env->ReleaseIntArrayElements(pixels.Get(), pixelData, 0);

            LocalRef<jclass> configClass = FindClass(env, "android/graphics/Bitmap$Config");
            const jfieldID argb8888Field = GetStaticFieldId(
                env,
                configClass.Get(),
                "ARGB_8888",
                "Landroid/graphics/Bitmap$Config;"
            );
            LocalRef<jobject> argb8888(
                env,
                env->GetStaticObjectField(configClass.Get(), argb8888Field)
            );
            CheckJavaException(env);

            LocalRef<jclass> bitmapClass = FindClass(env, "android/graphics/Bitmap");
            const jmethodID createBitmap = GetStaticMethodId(
                env,
                bitmapClass.Get(),
                "createBitmap",
                "([IIILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"
            );
            bitmap = LocalRef<jobject>(
                env,
                env->CallStaticObjectMethod(
                    bitmapClass.Get(),
                    createBitmap,
                    pixels.Get(),
                    static_cast<jint>(width),
                    static_cast<jint>(height),
                    argb8888.Get()
                )
            );
            CheckJavaException(env);
        }

        if (!bitmap)
        {
            throw std::runtime_error("the bitmap could not be allocated");
        }

        LocalRef<jclass> fileOutputStreamClass = FindClass(env, "java/io/FileOutputStream");
        const jmethodID fileOutputStreamConstructor = GetMethodId(
            env,
            fileOutputStreamClass.Get(),
            "<init>",
            "(Ljava/lang/String;)V"
        );
        LocalRef<jstring> javaPath = NewJavaString(env, path);
        LocalRef<jobject> stream(
            env,
            env->NewObject(fileOutputStreamClass.Get(), fileOutputStreamConstructor, javaPath.Get())
        );
        CheckJavaException(env);
        if (!stream)
        {
            throw std::runtime_error("the output stream could not be allocated");
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
                env->GetStaticObjectField(compressFormatClass.Get(), pngField)
            );
            CheckJavaException(env);

            LocalRef<jclass> bitmapClass(env, env->GetObjectClass(bitmap.Get()));
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

        LocalRef<jclass> outputStreamClass = FindClass(env, "java/io/OutputStream");
        const jmethodID close = GetMethodId(env, outputStreamClass.Get(), "close", "()V");
        env->CallVoidMethod(stream.Get(), close);
        CheckJavaException(env);

        if (bodyException)
        {
            std::rethrow_exception(bodyException);
        }
    }
}

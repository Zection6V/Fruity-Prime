#include "AndroidThumbnails.hpp"

#include "AndroidMaps.hpp"
#include "PreviewService.hpp"

#include "../MphRead.Native/Mods/ThumbnailGenerator.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

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

    private:
        void Reset() noexcept
        {
            if (_env != nullptr && _value != nullptr)
            {
                _env->DeleteLocalRef(_value);
            }
            _env = nullptr;
            _value = nullptr;
        }

        JNIEnv* _env = nullptr;
        T _value = nullptr;
    };

    std::string Utf16ToUtf8(const jchar* chars, jsize length)
    {
        std::string result;
        result.reserve(static_cast<std::size_t>(length));

        for (jsize index = 0; index < length; ++index)
        {
            std::uint32_t codePoint = chars[index];
            if (codePoint >= 0xD800u && codePoint <= 0xDBFFu)
            {
                if (index + 1 < length)
                {
                    const std::uint32_t low = chars[index + 1];
                    if (low >= 0xDC00u && low <= 0xDFFFu)
                    {
                        codePoint = 0x10000u
                            + ((codePoint - 0xD800u) << 10)
                            + (low - 0xDC00u);
                        ++index;
                    }
                    else
                    {
                        codePoint = 0xFFFDu;
                    }
                }
                else
                {
                    codePoint = 0xFFFDu;
                }
            }
            else if (codePoint >= 0xDC00u && codePoint <= 0xDFFFu)
            {
                codePoint = 0xFFFDu;
            }

            if (codePoint <= 0x7Fu)
            {
                result.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FFu)
            {
                result.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
                result.push_back(static_cast<char>(
                    0x80u | (codePoint & 0x3Fu)
                ));
            }
            else if (codePoint <= 0xFFFFu)
            {
                result.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
                result.push_back(static_cast<char>(
                    0x80u | ((codePoint >> 6) & 0x3Fu)
                ));
                result.push_back(static_cast<char>(
                    0x80u | (codePoint & 0x3Fu)
                ));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
                result.push_back(static_cast<char>(
                    0x80u | ((codePoint >> 12) & 0x3Fu)
                ));
                result.push_back(static_cast<char>(
                    0x80u | ((codePoint >> 6) & 0x3Fu)
                ));
                result.push_back(static_cast<char>(
                    0x80u | (codePoint & 0x3Fu)
                ));
            }
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
        if (chars == nullptr)
        {
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
            }
            return {};
        }

        std::string result;
        try
        {
            result = Utf16ToUtf8(chars, length);
        }
        catch (...)
        {
            env->ReleaseStringChars(value, chars);
            throw;
        }
        env->ReleaseStringChars(value, chars);
        return result;
    }

    std::string TakePendingJavaExceptionMessage(JNIEnv* env)
    {
        LocalRef<jthrowable> throwable(
            env,
            static_cast<jthrowable>(env->ExceptionOccurred())
        );
        env->ExceptionClear();

        if (!throwable)
        {
            return "Android Java exception";
        }

        LocalRef<jclass> throwableClass(
            env,
            env->GetObjectClass(throwable.Get())
        );
        if (env->ExceptionCheck() || !throwableClass)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }

        const jmethodID getMessage = env->GetMethodID(
            throwableClass.Get(),
            "getMessage",
            "()Ljava/lang/String;"
        );
        if (env->ExceptionCheck() || getMessage == nullptr)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }

        LocalRef<jstring> message(
            env,
            static_cast<jstring>(
                env->CallObjectMethod(throwable.Get(), getMessage)
            )
        );
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return "Android Java exception";
        }
        if (!message)
        {
            return {};
        }
        return JavaStringToUtf8(env, message.Get());
    }

    void CheckJavaException(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            throw std::runtime_error(
                TakePendingJavaExceptionMessage(env)
            );
        }
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

    LocalRef<jclass> ObjectClass(JNIEnv* env, jobject object)
    {
        if (object == nullptr)
        {
            throw std::runtime_error(
                "Object reference not set to an instance of an object."
            );
        }

        LocalRef<jclass> result(
            env,
            env->GetObjectClass(object)
        );
        CheckJavaException(env);
        if (!result)
        {
            throw std::runtime_error(
                "Android object class is not available"
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
        const jmethodID result = env->GetMethodID(
            type,
            name,
            signature
        );
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
        const jmethodID result = env->GetStaticMethodID(
            type,
            name,
            signature
        );
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::runtime_error(
                std::string("Android static method not found: ") + name
            );
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
            const auto first = static_cast<unsigned char>(value[index]);
            std::uint32_t codePoint = 0xFFFDu;
            std::size_t count = 1;

            if (first <= 0x7Fu)
            {
                codePoint = first;
            }
            else if ((first & 0xE0u) == 0xC0u
                && index + 1 < value.size())
            {
                const auto second =
                    static_cast<unsigned char>(value[index + 1]);
                if ((second & 0xC0u) == 0x80u)
                {
                    const std::uint32_t candidate =
                        (static_cast<std::uint32_t>(first & 0x1Fu) << 6)
                        | static_cast<std::uint32_t>(second & 0x3Fu);
                    if (candidate >= 0x80u)
                    {
                        codePoint = candidate;
                        count = 2;
                    }
                }
            }
            else if ((first & 0xF0u) == 0xE0u
                && index + 2 < value.size())
            {
                const auto second =
                    static_cast<unsigned char>(value[index + 1]);
                const auto third =
                    static_cast<unsigned char>(value[index + 2]);
                if ((second & 0xC0u) == 0x80u
                    && (third & 0xC0u) == 0x80u)
                {
                    const std::uint32_t candidate =
                        (static_cast<std::uint32_t>(first & 0x0Fu) << 12)
                        | (static_cast<std::uint32_t>(second & 0x3Fu) << 6)
                        | static_cast<std::uint32_t>(third & 0x3Fu);
                    if (candidate >= 0x800u
                        && (candidate < 0xD800u
                            || candidate > 0xDFFFu))
                    {
                        codePoint = candidate;
                        count = 3;
                    }
                }
            }
            else if ((first & 0xF8u) == 0xF0u
                && index + 3 < value.size())
            {
                const auto second =
                    static_cast<unsigned char>(value[index + 1]);
                const auto third =
                    static_cast<unsigned char>(value[index + 2]);
                const auto fourth =
                    static_cast<unsigned char>(value[index + 3]);
                if ((second & 0xC0u) == 0x80u
                    && (third & 0xC0u) == 0x80u
                    && (fourth & 0xC0u) == 0x80u)
                {
                    const std::uint32_t candidate =
                        (static_cast<std::uint32_t>(first & 0x07u) << 18)
                        | (static_cast<std::uint32_t>(second & 0x3Fu) << 12)
                        | (static_cast<std::uint32_t>(third & 0x3Fu) << 6)
                        | static_cast<std::uint32_t>(fourth & 0x3Fu);
                    if (candidate >= 0x10000u
                        && candidate <= 0x10FFFFu)
                    {
                        codePoint = candidate;
                        count = 4;
                    }
                }
            }

            index += count;
            if (codePoint <= 0xFFFFu)
            {
                result.push_back(
                    static_cast<char16_t>(codePoint)
                );
            }
            else
            {
                codePoint -= 0x10000u;
                result.push_back(
                    static_cast<char16_t>(
                        0xD800u + (codePoint >> 10)
                    )
                );
                result.push_back(
                    static_cast<char16_t>(
                        0xDC00u + (codePoint & 0x3FFu)
                    )
                );
            }
        }

        return result;
    }

    LocalRef<jstring> NewJavaString(
        JNIEnv* env,
        std::string_view value
    )
    {
        const std::u16string utf16 = Utf8ToUtf16(value);
        LocalRef<jstring> result(
            env,
            env->NewString(
                reinterpret_cast<const jchar*>(utf16.data()),
                static_cast<jsize>(utf16.size())
            )
        );
        CheckJavaException(env);
        if (!result)
        {
            throw std::bad_alloc();
        }
        return result;
    }

    LocalRef<jobjectArray> NewJavaStringArray(
        JNIEnv* env,
        const std::vector<std::string>& values
    )
    {
        LocalRef<jclass> stringClass =
            FindClass(env, "java/lang/String");
        LocalRef<jobjectArray> result(
            env,
            env->NewObjectArray(
                static_cast<jsize>(values.size()),
                stringClass.Get(),
                nullptr
            )
        );
        CheckJavaException(env);
        if (!result)
        {
            throw std::bad_alloc();
        }

        for (std::size_t index = 0; index < values.size(); ++index)
        {
            LocalRef<jstring> value =
                NewJavaString(env, values[index]);
            env->SetObjectArrayElement(
                result.Get(),
                static_cast<jsize>(index),
                value.Get()
            );
            CheckJavaException(env);
        }

        return result;
    }

    class ScopedJniEnv final
    {
    public:
        explicit ScopedJniEnv(JavaVM* javaVm)
            : _javaVm(javaVm)
        {
            if (_javaVm == nullptr)
            {
                throw std::runtime_error(
                    "Android Java VM is not available"
                );
            }

            const jint result = _javaVm->GetEnv(
                reinterpret_cast<void**>(&_env),
                JNI_VERSION_1_6
            );
            if (result == JNI_EDETACHED)
            {
                if (_javaVm->AttachCurrentThread(
                        reinterpret_cast<void**>(&_env),
                        nullptr
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
                _javaVm->DetachCurrentThread();
            }
        }

        ScopedJniEnv(const ScopedJniEnv&) = delete;
        ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

        [[nodiscard]] JNIEnv* Get() const noexcept
        {
            return _env;
        }

    private:
        JavaVM* _javaVm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

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

    void DeleteGlobalRefNoThrow(
        JavaVM* javaVm,
        jobject value
    ) noexcept
    {
        if (javaVm == nullptr || value == nullptr)
        {
            return;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        const jint result = javaVm->GetEnv(
            reinterpret_cast<void**>(&env),
            JNI_VERSION_1_6
        );
        if (result == JNI_EDETACHED)
        {
            if (javaVm->AttachCurrentThread(
                    reinterpret_cast<void**>(&env),
                    nullptr
                ) != JNI_OK)
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
            javaVm->DetachCurrentThread();
        }
    }

    std::int32_t UncheckedAdd(
        std::int32_t left,
        std::int32_t right
    ) noexcept
    {
        const std::uint32_t result =
            static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    std::int32_t UncheckedMultiply(
        std::int32_t left,
        std::int32_t right
    ) noexcept
    {
        const std::uint32_t result =
            static_cast<std::uint32_t>(left)
            * static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    LocalRef<jobject> BuildWorkerIntent(
        JNIEnv* env,
        jobject context,
        const MphRead::Droid::PreviewWorkerType& workerType,
        const std::vector<std::string>& rooms,
        const std::string& marker,
        std::int32_t width,
        std::int32_t height
    )
    {
        LocalRef<jclass> intentClass =
            FindClass(env, "android/content/Intent");
        const jmethodID constructor = GetMethodId(
            env,
            intentClass.Get(),
            "<init>",
            "()V"
        );
        const jmethodID setClassName = GetMethodId(
            env,
            intentClass.Get(),
            "setClassName",
            "(Landroid/content/Context;Ljava/lang/String;)Landroid/content/Intent;"
        );
        const jmethodID putStringArray = GetMethodId(
            env,
            intentClass.Get(),
            "putExtra",
            "(Ljava/lang/String;[Ljava/lang/String;)Landroid/content/Intent;"
        );
        const jmethodID putString = GetMethodId(
            env,
            intentClass.Get(),
            "putExtra",
            "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;"
        );
        const jmethodID putInt = GetMethodId(
            env,
            intentClass.Get(),
            "putExtra",
            "(Ljava/lang/String;I)Landroid/content/Intent;"
        );

        LocalRef<jobject> intent(
            env,
            env->NewObject(
                intentClass.Get(),
                constructor
            )
        );
        CheckJavaException(env);
        if (!intent)
        {
            throw std::bad_alloc();
        }

        LocalRef<jstring> componentName =
            NewJavaString(env, workerType.Name);
        LocalRef<jobject> setResult(
            env,
            env->CallObjectMethod(
                intent.Get(),
                setClassName,
                context,
                componentName.Get()
            )
        );
        CheckJavaException(env);

        LocalRef<jstring> roomsKey =
            NewJavaString(env, MphRead::Droid::PreviewService::RoomsExtra);
        LocalRef<jobjectArray> javaRooms =
            NewJavaStringArray(env, rooms);
        LocalRef<jobject> roomsResult(
            env,
            env->CallObjectMethod(
                intent.Get(),
                putStringArray,
                roomsKey.Get(),
                javaRooms.Get()
            )
        );
        CheckJavaException(env);

        LocalRef<jstring> markerKey =
            NewJavaString(env, MphRead::Droid::PreviewService::MarkerExtra);
        LocalRef<jstring> javaMarker =
            NewJavaString(env, marker);
        LocalRef<jobject> markerResult(
            env,
            env->CallObjectMethod(
                intent.Get(),
                putString,
                markerKey.Get(),
                javaMarker.Get()
            )
        );
        CheckJavaException(env);

        LocalRef<jstring> widthKey =
            NewJavaString(env, MphRead::Droid::PreviewService::WidthExtra);
        LocalRef<jobject> widthResult(
            env,
            env->CallObjectMethod(
                intent.Get(),
                putInt,
                widthKey.Get(),
                static_cast<jint>(width)
            )
        );
        CheckJavaException(env);

        LocalRef<jstring> heightKey =
            NewJavaString(env, MphRead::Droid::PreviewService::HeightExtra);
        LocalRef<jobject> heightResult(
            env,
            env->CallObjectMethod(
                intent.Get(),
                putInt,
                heightKey.Get(),
                static_cast<jint>(height)
            )
        );
        CheckJavaException(env);

        return intent;
    }

    void StartService(
        JNIEnv* env,
        jobject context,
        jobject intent
    )
    {
        LocalRef<jclass> contextClass =
            ObjectClass(env, context);
        const jmethodID startService = GetMethodId(
            env,
            contextClass.Get(),
            "startService",
            "(Landroid/content/Intent;)Landroid/content/ComponentName;"
        );
        LocalRef<jobject> component(
            env,
            env->CallObjectMethod(
                context,
                startService,
                intent
            )
        );
        CheckJavaException(env);
    }

    bool FileExists(const std::string& path) noexcept
    {
        if (path.find('\0') != std::string::npos)
        {
            return false;
        }

        std::error_code error;
        const bool exists = std::filesystem::is_regular_file(
            std::filesystem::path(path),
            error
        );
        return !error && exists;
    }

    std::string CombinePath(
        const std::string& directory,
        const std::string& name
    )
    {
        return (
            std::filesystem::path(directory)
            / std::filesystem::path(name)
        ).string();
    }
}

namespace MphRead::Droid
{
    AndroidThumbnailHost::AndroidThumbnailHost(
        JNIEnv* env,
        jobject activity
    )
    {
        if (env == nullptr)
        {
            throw std::invalid_argument(
                "Android JNI environment must not be null"
            );
        }
        if (env->GetJavaVM(&_javaVm) != JNI_OK
            || _javaVm == nullptr)
        {
            throw std::runtime_error(
                "Android Java VM is not available"
            );
        }
        _activity = NewGlobalRefChecked(
            env,
            activity
        );
    }

    AndroidThumbnailHost::~AndroidThumbnailHost()
    {
        DeleteGlobalRefNoThrow(
            _javaVm,
            _activity
        );
    }

    MphRead::Mods::ThumbnailTaskIntRef
        AndroidThumbnailHost::RenderAsync(
            MphRead::Mods::ThumbnailRoomsRef rooms,
            MphRead::Mods::ThumbnailReportRef report
        )
    {
        AndroidMaps::EnsureBuilt();

        if (_activity == nullptr)
        {
            throw std::runtime_error(
                "Object reference not set to an instance of an object."
            );
        }

        ScopedJniEnv scopedEnv(_javaVm);
        return GetAndroidThumbnailHostOwner().RenderPreviews(
            scopedEnv.Get(),
            _activity,
            rooms,
            report
        );
    }

    std::int32_t PreviewWorkers::Count(
        JNIEnv* env,
        jobject context
    )
    {
        if (env == nullptr)
        {
            throw std::invalid_argument(
                "Android JNI environment must not be null"
            );
        }

        LocalRef<jclass> runtimeClass =
            FindClass(env, "java/lang/Runtime");
        const jmethodID getRuntime = GetStaticMethodId(
            env,
            runtimeClass.Get(),
            "getRuntime",
            "()Ljava/lang/Runtime;"
        );
        LocalRef<jobject> runtime(
            env,
            env->CallStaticObjectMethod(
                runtimeClass.Get(),
                getRuntime
            )
        );
        CheckJavaException(env);

        std::int32_t cores = 1;
        if (runtime)
        {
            const jmethodID availableProcessors = GetMethodId(
                env,
                runtimeClass.Get(),
                "availableProcessors",
                "()I"
            );
            const jint value = env->CallIntMethod(
                runtime.Get(),
                availableProcessors
            );
            CheckJavaException(env);
            cores = std::max<std::int32_t>(
                1,
                static_cast<std::int32_t>(value)
            );
        }

        LocalRef<jclass> contextClass =
            ObjectClass(env, context);
        const jmethodID getSystemService = GetMethodId(
            env,
            contextClass.Get(),
            "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;"
        );
        LocalRef<jstring> activityService =
            NewJavaString(env, "activity");
        LocalRef<jobject> service(
            env,
            env->CallObjectMethod(
                context,
                getSystemService,
                activityService.Get()
            )
        );
        CheckJavaException(env);

        std::int32_t heapMb = 128;
        if (service)
        {
            LocalRef<jclass> managerClass =
                FindClass(env, "android/app/ActivityManager");
            const jboolean isManager = env->IsInstanceOf(
                service.Get(),
                managerClass.Get()
            );
            CheckJavaException(env);
            if (isManager == JNI_TRUE)
            {
                const jmethodID getMemoryClass = GetMethodId(
                    env,
                    managerClass.Get(),
                    "getMemoryClass",
                    "()I"
                );
                const jint value = env->CallIntMethod(
                    service.Get(),
                    getMemoryClass
                );
                CheckJavaException(env);
                heapMb = std::max<std::int32_t>(
                    32,
                    static_cast<std::int32_t>(value)
                );
            }
        }

        const std::int32_t doubledCores =
            UncheckedMultiply(cores, 2);
        const std::int32_t byHeap =
            heapMb / 24;
        const std::int32_t requested =
            std::min(doubledCores, byHeap);
        return std::clamp<std::int32_t>(
            requested,
            1,
            static_cast<std::int32_t>(
                PreviewWorkerTypes::All.size()
            )
        );
    }

    std::int32_t PreviewWorkers::Run(
        JNIEnv* env,
        jobject context,
        const std::vector<std::string>& rooms,
        std::int32_t width,
        std::int32_t height,
        const std::function<void(const std::string&)>& report
    )
    {
        const std::int32_t workers = std::min<std::int32_t>(
            Count(env, context),
            static_cast<std::int32_t>(rooms.size())
        );

        std::vector<std::vector<std::string>> shares(
            static_cast<std::size_t>(workers)
        );
        for (std::size_t index = 0; index < rooms.size(); ++index)
        {
            shares[
                index % static_cast<std::size_t>(workers)
            ].push_back(rooms[index]);
        }

        std::vector<std::string> markers;
        std::int32_t started = 0;
        for (std::int32_t index = 0; index < workers; ++index)
        {
            const std::string marker = CombinePath(
                MphRead::Mods::ThumbnailGenerator::CacheDirectory(),
                ".worker" + std::to_string(index) + ".done"
            );
            TryDelete(marker);

            LocalRef<jobject> intent = BuildWorkerIntent(
                env,
                context,
                PreviewWorkerTypes::All[
                    static_cast<std::size_t>(index)
                ],
                shares[static_cast<std::size_t>(index)],
                marker,
                width,
                height
            );

            try
            {
                StartService(
                    env,
                    context,
                    intent.Get()
                );
            }
            catch (const std::exception& ex)
            {
                std::cout
                    << "[thumbnails] worker "
                    << index
                    << " would not start: "
                    << ex.what()
                    << '\n';
                continue;
            }
            catch (...)
            {
                std::cout
                    << "[thumbnails] worker "
                    << index
                    << " would not start: "
                    << "unknown error"
                    << '\n';
                continue;
            }

            markers.push_back(marker);
            ++started;
        }

        if (started == 0)
        {
            return 0;
        }

        report(
            "[thumbnails] rendering "
            + std::to_string(rooms.size())
            + " preview(s) in the background, "
            + std::to_string(started)
            + " at a time"
        );
        Watch(
            rooms,
            markers,
            report
        );
        return CountWritten(rooms);
    }

    void PreviewWorkers::Watch(
        const std::vector<std::string>& rooms,
        const std::vector<std::string>& markers,
        const std::function<void(const std::string&)>& report
    )
    {
        const auto clock =
            std::chrono::steady_clock::now();
        const std::int32_t limitSeconds =
            UncheckedAdd(
                90,
                UncheckedMultiply(
                    30,
                    static_cast<std::int32_t>(rooms.size())
                )
            );
        const std::chrono::seconds limit(
            limitSeconds
        );

        std::int32_t last = -1;
        while (std::chrono::steady_clock::now() - clock < limit)
        {
            bool allDone = true;
            for (const std::string& marker : markers)
            {
                if (!FileExists(marker))
                {
                    allDone = false;
                    break;
                }
            }

            const std::int32_t written =
                CountWritten(rooms);
            if (written != last)
            {
                last = written;
                report(
                    "[thumbnails] "
                    + std::to_string(written)
                    + "/"
                    + std::to_string(rooms.size())
                );
            }

            if (allDone)
            {
                return;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(500)
            );
        }

        report(
            "[thumbnails] the background workers ran out of time; "
            "the rest will be rendered on the next visit"
        );
    }

    std::int32_t PreviewWorkers::CountWritten(
        const std::vector<std::string>& rooms
    )
    {
        std::int32_t written = 0;
        for (const std::string& room : rooms)
        {
            if (MphRead::Mods::ThumbnailGenerator::Exists(room))
            {
                ++written;
            }
        }
        return written;
    }

    void PreviewWorkers::TryDelete(
        const std::string& path
    ) noexcept
    {
        try
        {
            if (path.find('\0') != std::string::npos)
            {
                return;
            }

            std::error_code statusError;
            const std::filesystem::file_status status =
                std::filesystem::symlink_status(
                    std::filesystem::path(path),
                    statusError
                );
            if (statusError)
            {
                return;
            }
            if (std::filesystem::is_directory(status)
                && !std::filesystem::is_symlink(status))
            {
                return;
            }

            std::error_code removeError;
            (void)std::filesystem::remove(
                std::filesystem::path(path),
                removeError
            );
        }
        catch (...)
        {
        }
    }
}

#include "PreviewService.hpp"

#if !defined(__ANDROID__)
#error "PreviewService is only valid for the Android native target."
#endif

#include "AndroidPng.hpp"
#include "OffscreenGl.hpp"
#include "PreviewRun.hpp"

#include "../MphRead.Native/Mods/Launcher/Portable/GameFiles.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"
#include "../MphRead.Native/Mods/ScreenCapture.hpp"
#include "../MphRead.Native/Mods/ThumbnailGenerator.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
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

        for (jsize i = 0; i < length; i++)
        {
            std::uint32_t codePoint = chars[i];
            if (codePoint >= 0xD800u && codePoint <= 0xDBFFu)
            {
                if (i + 1 < length)
                {
                    const std::uint32_t low = chars[i + 1];
                    if (low >= 0xDC00u && low <= 0xDFFFu)
                    {
                        codePoint = 0x10000u
                            + ((codePoint - 0xD800u) << 10)
                            + (low - 0xDC00u);
                        i++;
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
                result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
            else if (codePoint <= 0xFFFFu)
            {
                result.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
                result.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
                result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
                result.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
                result.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
                result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
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
            throw std::runtime_error("could not read Android string length");
        }

        const jchar* chars = env->GetStringChars(value, nullptr);
        if (chars == nullptr)
        {
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
            }
            throw std::bad_alloc();
        }

        try
        {
            std::string result = Utf16ToUtf8(chars, length);
            env->ReleaseStringChars(value, chars);
            return result;
        }
        catch (...)
        {
            env->ReleaseStringChars(value, chars);
            throw;
        }
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
            if (throwableClass && !env->ExceptionCheck())
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
            throw std::runtime_error(
                std::string("Android method not found: ") + name
            );
        }
        return result;
    }

    LocalRef<jstring> NewString(JNIEnv* env, std::string_view value)
    {
        LocalRef<jstring> result(
            env,
            env->NewStringUTF(std::string(value).c_str())
        );
        CheckJavaException(env);
        if (!result)
        {
            throw std::bad_alloc();
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
            if (vm->AttachCurrentThread(
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
            vm->DetachCurrentThread();
        }
    }

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
                if (_vm->AttachCurrentThread(
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

    std::optional<std::vector<std::string>> GetStringArrayExtra(
        JNIEnv* env,
        jobject intent,
        std::string_view key
    )
    {
        LocalRef<jclass> type = ObjectClass(env, intent);
        const jmethodID method = GetMethodId(
            env,
            type.Get(),
            "getStringArrayExtra",
            "(Ljava/lang/String;)[Ljava/lang/String;"
        );
        LocalRef<jstring> name = NewString(env, key);
        LocalRef<jobjectArray> array(
            env,
            static_cast<jobjectArray>(
                env->CallObjectMethod(intent, method, name.Get())
            )
        );
        CheckJavaException(env);
        if (!array)
        {
            return std::nullopt;
        }

        const jsize count = env->GetArrayLength(array.Get());
        CheckJavaException(env);

        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(count));
        for (jsize i = 0; i < count; i++)
        {
            LocalRef<jstring> item(
                env,
                static_cast<jstring>(
                    env->GetObjectArrayElement(array.Get(), i)
                )
            );
            CheckJavaException(env);

            if (!item)
            {
                throw std::runtime_error(
                    "Android rooms extra contained a null string"
                );
            }
            result.push_back(JavaStringToUtf8(env, item.Get()));
        }
        return result;
    }

    std::optional<std::string> GetStringExtra(
        JNIEnv* env,
        jobject intent,
        std::string_view key
    )
    {
        LocalRef<jclass> type = ObjectClass(env, intent);
        const jmethodID method = GetMethodId(
            env,
            type.Get(),
            "getStringExtra",
            "(Ljava/lang/String;)Ljava/lang/String;"
        );
        LocalRef<jstring> name = NewString(env, key);
        LocalRef<jstring> value(
            env,
            static_cast<jstring>(
                env->CallObjectMethod(intent, method, name.Get())
            )
        );
        CheckJavaException(env);
        if (!value)
        {
            return std::nullopt;
        }
        return JavaStringToUtf8(env, value.Get());
    }

    std::int32_t GetIntExtra(
        JNIEnv* env,
        jobject intent,
        std::string_view key,
        std::int32_t defaultValue
    )
    {
        LocalRef<jclass> type = ObjectClass(env, intent);
        const jmethodID method = GetMethodId(
            env,
            type.Get(),
            "getIntExtra",
            "(Ljava/lang/String;I)I"
        );
        LocalRef<jstring> name = NewString(env, key);
        const jint value = env->CallIntMethod(
            intent,
            method,
            name.Get(),
            static_cast<jint>(defaultValue)
        );
        CheckJavaException(env);
        return static_cast<std::int32_t>(value);
    }

    std::optional<std::string> FileAbsolutePath(JNIEnv* env, jobject file)
    {
        if (file == nullptr)
        {
            return std::nullopt;
        }

        LocalRef<jclass> type = ObjectClass(env, file);
        const jmethodID method = GetMethodId(
            env,
            type.Get(),
            "getAbsolutePath",
            "()Ljava/lang/String;"
        );
        LocalRef<jstring> path(
            env,
            static_cast<jstring>(
                env->CallObjectMethod(file, method)
            )
        );
        CheckJavaException(env);
        if (!path)
        {
            return std::nullopt;
        }
        return JavaStringToUtf8(env, path.Get());
    }

    std::string GetServiceRoot(JNIEnv* env, jobject service)
    {
        LocalRef<jclass> type = ObjectClass(env, service);

        const jmethodID externalMethod = GetMethodId(
            env,
            type.Get(),
            "getExternalFilesDir",
            "(Ljava/lang/String;)Ljava/io/File;"
        );
        LocalRef<jobject> external(
            env,
            env->CallObjectMethod(
                service,
                externalMethod,
                static_cast<jobject>(nullptr)
            )
        );
        CheckJavaException(env);

        std::optional<std::string> root = FileAbsolutePath(
            env,
            external.Get()
        );
        if (root.has_value())
        {
            return std::move(*root);
        }

        const jmethodID filesMethod = GetMethodId(
            env,
            type.Get(),
            "getFilesDir",
            "()Ljava/io/File;"
        );
        LocalRef<jobject> files(
            env,
            env->CallObjectMethod(service, filesMethod)
        );
        CheckJavaException(env);

        root = FileAbsolutePath(env, files.Get());
        return root.has_value()
            ? std::move(*root)
            : std::string();
    }

    void StopServiceSelf(
        JNIEnv* env,
        jobject service,
        std::int32_t startId
    )
    {
        LocalRef<jclass> type = ObjectClass(env, service);
        const jmethodID method = GetMethodId(
            env,
            type.Get(),
            "stopSelf",
            "(I)V"
        );
        env->CallVoidMethod(
            service,
            method,
            static_cast<jint>(startId)
        );
        CheckJavaException(env);
    }

    std::string ServiceTypeName(JNIEnv* env, jobject service)
    {
        LocalRef<jclass> type = ObjectClass(env, service);
        const jmethodID getClass = GetMethodId(
            env,
            type.Get(),
            "getClass",
            "()Ljava/lang/Class;"
        );
        LocalRef<jobject> classObject(
            env,
            env->CallObjectMethod(service, getClass)
        );
        CheckJavaException(env);

        LocalRef<jclass> classType = ObjectClass(
            env,
            classObject.Get()
        );
        const jmethodID getSimpleName = GetMethodId(
            env,
            classType.Get(),
            "getSimpleName",
            "()Ljava/lang/String;"
        );
        LocalRef<jstring> name(
            env,
            static_cast<jstring>(
                env->CallObjectMethod(
                    classObject.Get(),
                    getSimpleName
                )
            )
        );
        CheckJavaException(env);
        return JavaStringToUtf8(env, name.Get());
    }
}

namespace MphRead::Droid
{
    class PreviewService::ServiceTarget final
    {
    public:
        ServiceTarget(JNIEnv* env, jobject service)
        {
            if (env == nullptr)
            {
                throw std::invalid_argument(
                    "Android JNI environment must not be null"
                );
            }
            if (service == nullptr)
            {
                throw std::invalid_argument(
                    "Android PreviewService peer must not be null"
                );
            }
            if (env->GetJavaVM(&_vm) != JNI_OK || _vm == nullptr)
            {
                throw std::runtime_error(
                    "Android Java VM is not available"
                );
            }
            _service = NewGlobalRefChecked(env, service);
        }

        ~ServiceTarget()
        {
            DeleteGlobalRefNoThrow(_vm, _service);
        }

        ServiceTarget(const ServiceTarget&) = delete;
        ServiceTarget& operator=(const ServiceTarget&) = delete;

        [[nodiscard]] std::string Root() const
        {
            ScopedJniEnv scoped(_vm);
            return GetServiceRoot(scoped.Get(), _service);
        }

        [[nodiscard]] std::string TypeName() const
        {
            ScopedJniEnv scoped(_vm);
            return ServiceTypeName(scoped.Get(), _service);
        }

        void StopSelf(std::int32_t startId) const
        {
            ScopedJniEnv scoped(_vm);
            StopServiceSelf(
                scoped.Get(),
                _service,
                startId
            );
        }

    private:
        JavaVM* _vm = nullptr;
        jobject _service = nullptr;
    };

    const std::array<PreviewWorkerType, 10>
        PreviewWorkerTypes::All =
    {{
        {
            "fr.livetek.fruityprime.PreviewWorker0",
            ":preview0",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker1",
            ":preview1",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker2",
            ":preview2",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker3",
            ":preview3",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker4",
            ":preview4",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker5",
            ":preview5",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker6",
            ":preview6",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker7",
            ":preview7",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker8",
            ":preview8",
            false
        },
        {
            "fr.livetek.fruityprime.PreviewWorker9",
            ":preview9",
            false
        }
    }};

    PreviewService::PreviewService(
        JNIEnv* env,
        jobject service
    )
        : _service(
            std::make_shared<ServiceTarget>(
                env,
                service
            )
        )
    {
    }

    PreviewService::~PreviewService() = default;

    jobject PreviewService::OnBind(
        JNIEnv* env,
        jobject intent
    ) const noexcept
    {
        (void)env;
        (void)intent;
        return nullptr;
    }

    std::int32_t PreviewService::OnStartCommand(
        JNIEnv* env,
        jobject intent,
        std::int32_t flags,
        std::int32_t startId
    )
    {
        (void)flags;

        std::optional<std::vector<std::string>> rooms;
        std::optional<std::string> marker;
        std::int32_t width = PreviewRun::Width;
        std::int32_t height = PreviewRun::Height;

        if (intent != nullptr)
        {
            rooms = GetStringArrayExtra(
                env,
                intent,
                RoomsExtra
            );
            marker = GetStringExtra(
                env,
                intent,
                MarkerExtra
            );
            width = GetIntExtra(
                env,
                intent,
                WidthExtra,
                PreviewRun::Width
            );
            height = GetIntExtra(
                env,
                intent,
                HeightExtra,
                PreviewRun::Height
            );
        }

        if (!rooms.has_value() || rooms->empty())
        {
            Finish(marker);
            _service->StopSelf(startId);
            return StartNotSticky;
        }

        std::shared_ptr<ServiceTarget> service = _service;
        std::thread thread(
            [
                service,
                rooms = std::move(*rooms),
                marker = std::move(marker),
                width,
                height,
                startId
            ]() mutable
            {
                try
                {
                    try
                    {
                        Run(
                            service,
                            rooms,
                            width,
                            height
                        );
                    }
                    catch (const std::exception& ex)
                    {
                        std::cout
                            << "[preview worker] "
                            << service->TypeName()
                            << " failed: "
                            << ex.what()
                            << '\n';
                    }
                }
                catch (...)
                {
                    Finish(marker);
                    service->StopSelf(startId);
                    throw;
                }

                Finish(marker);
                service->StopSelf(startId);
            }
        );
        thread.detach();

        return StartNotSticky;
    }

    void PreviewService::Run(
        const std::shared_ptr<ServiceTarget>& service,
        const std::vector<std::string>& rooms,
        std::int32_t width,
        std::int32_t height
    )
    {
        std::string root = service->Root();
        if (!root.empty())
        {
            Mods::Launcher::LauncherPrefs::Directory(root);
            Mods::Launcher::GameFiles::Root(root);
            std::filesystem::current_path(root);
        }

        if (!Mods::Launcher::GameFiles::Ready())
        {
            std::cout
                << "[preview worker] no game files in this process; nothing to render\n";
            return;
        }

        Mods::Launcher::GameFiles::ApplyPaths();
        Mods::ThumbnailGenerator::EnsureCacheDirectory();
        Mods::ScreenCapture::PngWriter(AndroidPng::Write);

        std::shared_ptr<OffscreenGl> gl =
            OffscreenGl::Create(width, height);
        try
        {
            (void)PreviewRun::Render(
                rooms,
                width,
                height,
                [](const std::string& line)
                {
                    std::cout << line << '\n';
                }
            );
        }
        catch (...)
        {
            gl->Dispose();
            throw;
        }
        gl->Dispose();
    }

    void PreviewService::Finish(
        const std::optional<std::string>& marker
    )
    {
        if (!marker.has_value() || marker->empty())
        {
            return;
        }

        try
        {
            if (marker->find('\0') != std::string::npos)
            {
                throw std::invalid_argument(
                    "path contains a null character"
                );
            }

            std::ofstream file;
            file.exceptions(
                std::ios::badbit
                | std::ios::failbit
            );
            file.open(
                std::filesystem::path(*marker),
                std::ios::binary
                    | std::ios::out
                    | std::ios::trunc
            );
            file.write("done", 4);
            file.close();
        }
        catch (const std::exception& ex)
        {
            std::cout
                << "[preview worker] could not write "
                << *marker
                << ": "
                << ex.what()
                << '\n';
        }
    }
}

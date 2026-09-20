#include "ApkInstaller.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <new>
#include <optional>
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

    std::string Utf16ToUtf8(std::u16string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            std::uint32_t codePoint = static_cast<std::uint16_t>(value[i]);
            if (codePoint >= 0xD800u && codePoint <= 0xDBFFu)
            {
                if (i + 1 < value.size())
                {
                    const std::uint32_t low = static_cast<std::uint16_t>(value[i + 1]);
                    if (low >= 0xDC00u && low <= 0xDFFFu)
                    {
                        codePoint = 0x10000u + ((codePoint - 0xD800u) << 10)
                            + (low - 0xDC00u);
                        ++i;
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

    std::u16string Utf8ToUtf16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        std::size_t i = 0;
        while (i < value.size())
        {
            const std::uint8_t first = static_cast<std::uint8_t>(value[i]);
            std::uint32_t codePoint = 0;
            std::size_t length = 0;
            std::uint32_t minimum = 0;

            if (first <= 0x7Fu)
            {
                codePoint = first;
                length = 1;
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
                result.push_back(u'\uFFFD');
                ++i;
                continue;
            }

            if (i + length > value.size())
            {
                result.push_back(u'\uFFFD');
                ++i;
                continue;
            }

            bool valid = true;
            for (std::size_t j = 1; j < length; ++j)
            {
                const std::uint8_t next = static_cast<std::uint8_t>(value[i + j]);
                if ((next & 0xC0u) != 0x80u)
                {
                    valid = false;
                    break;
                }
                codePoint = (codePoint << 6) | (next & 0x3Fu);
            }

            if (!valid || codePoint < minimum || codePoint > 0x10FFFFu
                || (codePoint >= 0xD800u && codePoint <= 0xDFFFu))
            {
                result.push_back(u'\uFFFD');
                ++i;
                continue;
            }

            if (codePoint <= 0xFFFFu)
            {
                result.push_back(static_cast<char16_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000u;
                result.push_back(static_cast<char16_t>(0xD800u + (codePoint >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00u + (codePoint & 0x3FFu)));
            }
            i += length;
        }
        return result;
    }

    class ManagedException final : public std::exception
    {
    public:
        explicit ManagedException(std::string message)
            : _message(std::move(message))
        {
        }

        [[nodiscard]] const char* what() const noexcept override
        {
            return _message.c_str();
        }

        [[nodiscard]] const std::string& Message() const noexcept
        {
            return _message;
        }

    private:
        std::string _message;
    };

    class AndroidJavaException final : public std::exception
    {
    public:
        AndroidJavaException(std::string message, std::string display)
            : _message(std::move(message)),
              _display(std::move(display))
        {
            if (_display.empty())
            {
                _display = _message.empty() ? "Android Java exception" : _message;
            }
        }

        [[nodiscard]] const char* what() const noexcept override
        {
            return _display.c_str();
        }

        [[nodiscard]] const std::string& Message() const noexcept
        {
            return _message;
        }

    private:
        std::string _message;
        std::string _display;
    };

    void RequireEnv(JNIEnv* env)
    {
        if (env == nullptr)
        {
            throw std::invalid_argument("Android JNI environment must not be null");
        }
    }

    std::optional<std::string> TryJavaStringToUtf8(JNIEnv* env, jstring value) noexcept
    {
        if (value == nullptr)
        {
            return std::nullopt;
        }

        const jsize length = env->GetStringLength(value);
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return std::nullopt;
        }
        const jchar* chars = env->GetStringChars(value, nullptr);
        if (env->ExceptionCheck() || chars == nullptr)
        {
            env->ExceptionClear();
            return std::nullopt;
        }

        try
        {
            const std::u16string text(
                reinterpret_cast<const char16_t*>(chars),
                reinterpret_cast<const char16_t*>(chars) + length
            );
            env->ReleaseStringChars(value, chars);
            return Utf16ToUtf8(text);
        }
        catch (...)
        {
            env->ReleaseStringChars(value, chars);
            return std::nullopt;
        }
    }

    std::optional<std::string> TryThrowableText(
        JNIEnv* env,
        jthrowable throwable,
        const char* methodName
    ) noexcept
    {
        if (throwable == nullptr)
        {
            return std::nullopt;
        }

        LocalRef<jclass> throwableClass(env, env->GetObjectClass(throwable));
        if (env->ExceptionCheck() || !throwableClass)
        {
            env->ExceptionClear();
            return std::nullopt;
        }

        const jmethodID method = env->GetMethodID(
            throwableClass.Get(), methodName, "()Ljava/lang/String;"
        );
        if (env->ExceptionCheck() || method == nullptr)
        {
            env->ExceptionClear();
            return std::nullopt;
        }

        LocalRef<jstring> text(
            env,
            static_cast<jstring>(env->CallObjectMethod(throwable, method))
        );
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return std::nullopt;
        }
        return TryJavaStringToUtf8(env, text.Get());
    }

    [[noreturn]] void ThrowPendingJavaException(JNIEnv* env)
    {
        LocalRef<jthrowable> throwable(env, env->ExceptionOccurred());
        env->ExceptionClear();

        const std::string message = TryThrowableText(env, throwable.Get(), "getMessage")
            .value_or(std::string{});
        const std::string display = TryThrowableText(env, throwable.Get(), "toString")
            .value_or(message);
        throw AndroidJavaException(message, display);
    }

    void CheckJavaException(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            ThrowPendingJavaException(env);
        }
    }

    [[noreturn]] void ThrowNullReference()
    {
        throw ManagedException("Object reference not set to an instance of an object.");
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
            throw std::runtime_error(std::string("Android static field not found: ") + name);
        }
        return result;
    }

    jint GetStaticIntField(JNIEnv* env, jclass type, const char* name)
    {
        const jfieldID field = GetStaticFieldId(env, type, name, "I");
        const jint result = env->GetStaticIntField(type, field);
        CheckJavaException(env);
        return result;
    }

    LocalRef<jstring> GetStaticStringField(JNIEnv* env, jclass type, const char* name)
    {
        const jfieldID field = GetStaticFieldId(env, type, name, "Ljava/lang/String;");
        LocalRef<jstring> result(
            env,
            static_cast<jstring>(env->GetStaticObjectField(type, field))
        );
        CheckJavaException(env);
        return result;
    }

    LocalRef<jstring> NewJavaString(JNIEnv* env, std::string_view value)
    {
        const std::u16string utf16 = Utf8ToUtf16(value);
        if (utf16.size() > static_cast<std::size_t>(std::numeric_limits<jsize>::max()))
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

    std::string JavaStringToUtf8(JNIEnv* env, jstring value)
    {
        if (value == nullptr)
        {
            ThrowNullReference();
        }

        const jsize length = env->GetStringLength(value);
        CheckJavaException(env);
        const jchar* chars = env->GetStringChars(value, nullptr);
        CheckJavaException(env);
        if (chars == nullptr)
        {
            throw std::bad_alloc();
        }

        try
        {
            const std::u16string text(
                reinterpret_cast<const char16_t*>(chars),
                reinterpret_cast<const char16_t*>(chars) + length
            );
            env->ReleaseStringChars(value, chars);
            return Utf16ToUtf8(text);
        }
        catch (...)
        {
            env->ReleaseStringChars(value, chars);
            throw;
        }
    }

    jint AndroidSdkInt(JNIEnv* env)
    {
        LocalRef<jclass> versionClass = FindClass(env, "android/os/Build$VERSION");
        return GetStaticIntField(env, versionClass.Get(), "SDK_INT");
    }

    LocalRef<jobject> PackageManager(JNIEnv* env, jobject context)
    {
        if (context == nullptr)
        {
            ThrowNullReference();
        }

        LocalRef<jclass> contextClass = FindClass(env, "android/content/Context");
        const jmethodID getPackageManager = GetMethodId(
            env,
            contextClass.Get(),
            "getPackageManager",
            "()Landroid/content/pm/PackageManager;"
        );
        LocalRef<jobject> result(
            env,
            env->CallObjectMethod(context, getPackageManager)
        );
        CheckJavaException(env);
        return result;
    }

    LocalRef<jstring> PackageName(JNIEnv* env, jobject context)
    {
        if (context == nullptr)
        {
            ThrowNullReference();
        }

        LocalRef<jclass> contextClass = FindClass(env, "android/content/Context");
        const jmethodID getPackageName = GetMethodId(
            env,
            contextClass.Get(),
            "getPackageName",
            "()Ljava/lang/String;"
        );
        LocalRef<jstring> result(
            env,
            static_cast<jstring>(env->CallObjectMethod(context, getPackageName))
        );
        CheckJavaException(env);
        return result;
    }

    std::string DotNetTempPath()
    {
        const char* value = std::getenv("TMPDIR");
        if (value == nullptr || value[0] == '\0')
        {
            return "/tmp/";
        }

        std::string result(value);
        if (result.empty() || result.back() != '/')
        {
            result.push_back('/');
        }
        return result;
    }

    std::string CacheDirectory(JNIEnv* env, jobject context)
    {
        if (context == nullptr)
        {
            ThrowNullReference();
        }

        LocalRef<jclass> contextClass = FindClass(env, "android/content/Context");
        const jmethodID getCacheDir = GetMethodId(
            env,
            contextClass.Get(),
            "getCacheDir",
            "()Ljava/io/File;"
        );
        LocalRef<jobject> cacheDir(
            env,
            env->CallObjectMethod(context, getCacheDir)
        );
        CheckJavaException(env);

        if (cacheDir)
        {
            LocalRef<jclass> fileClass = FindClass(env, "java/io/File");
            const jmethodID getAbsolutePath = GetMethodId(
                env,
                fileClass.Get(),
                "getAbsolutePath",
                "()Ljava/lang/String;"
            );
            LocalRef<jstring> absolutePath(
                env,
                static_cast<jstring>(
                    env->CallObjectMethod(cacheDir.Get(), getAbsolutePath)
                )
            );
            CheckJavaException(env);
            if (absolutePath)
            {
                return JavaStringToUtf8(env, absolutePath.Get());
            }
        }

        return DotNetTempPath();
    }

    std::string CombinePath(std::string_view left, std::string_view right)
    {
        std::filesystem::path result{std::string(left)};
        result /= std::filesystem::path{std::string(right)};
        return result.string();
    }

    bool FileExists(std::string_view path) noexcept
    {
        if (path.empty())
        {
            return false;
        }

        std::error_code error;
        return std::filesystem::is_regular_file(
            std::filesystem::path(std::string(path)),
            error
        ) && !error;
    }

    std::int64_t FileLength(std::string_view path)
    {
        const std::uintmax_t length = std::filesystem::file_size(
            std::filesystem::path(std::string(path))
        );
        if (length > static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max()))
        {
            throw std::overflow_error("file length is too large");
        }
        return static_cast<std::int64_t>(length);
    }

    void CloseJavaObject(JNIEnv* env, jobject value)
    {
        if (value == nullptr)
        {
            return;
        }

        LocalRef<jclass> type(env, env->GetObjectClass(value));
        CheckJavaException(env);
        if (!type)
        {
            ThrowNullReference();
        }
        const jmethodID close = GetMethodId(env, type.Get(), "close", "()V");
        env->CallVoidMethod(value, close);
        CheckJavaException(env);
    }

    void CopyPackageToSession(
        JNIEnv* env,
        jobject session,
        std::string_view apkPath
    )
    {
        LocalRef<jclass> sessionClass = FindClass(
            env, "android/content/pm/PackageInstaller$Session"
        );
        const jmethodID openWrite = GetMethodId(
            env,
            sessionClass.Get(),
            "openWrite",
            "(Ljava/lang/String;JJ)Ljava/io/OutputStream;"
        );
        LocalRef<jstring> packageName = NewJavaString(env, "package");
        const jlong length = static_cast<jlong>(FileLength(apkPath));
        LocalRef<jobject> into(
            env,
            env->CallObjectMethod(
                session,
                openWrite,
                packageName.Get(),
                static_cast<jlong>(0),
                length
            )
        );
        CheckJavaException(env);
        if (!into)
        {
            ThrowNullReference();
        }

        LocalRef<jobject> from;
        std::exception_ptr pending;

        try
        {
            LocalRef<jclass> inputClass = FindClass(env, "java/io/FileInputStream");
            const jmethodID inputConstructor = GetMethodId(
                env,
                inputClass.Get(),
                "<init>",
                "(Ljava/lang/String;)V"
            );
            LocalRef<jstring> javaPath = NewJavaString(env, apkPath);
            from = LocalRef<jobject>(
                env,
                env->NewObject(
                    inputClass.Get(),
                    inputConstructor,
                    javaPath.Get()
                )
            );
            CheckJavaException(env);
            if (!from)
            {
                throw std::bad_alloc();
            }

            constexpr jsize BufferSize = 256 * 1024;
            LocalRef<jbyteArray> buffer(env, env->NewByteArray(BufferSize));
            CheckJavaException(env);
            if (!buffer)
            {
                throw std::bad_alloc();
            }

            const jmethodID read = GetMethodId(
                env,
                inputClass.Get(),
                "read",
                "([B)I"
            );
            LocalRef<jclass> outputClass(env, env->GetObjectClass(into.Get()));
            CheckJavaException(env);
            if (!outputClass)
            {
                ThrowNullReference();
            }
            const jmethodID write = GetMethodId(
                env,
                outputClass.Get(),
                "write",
                "([BII)V"
            );

            for (;;)
            {
                const jint count = env->CallIntMethod(from.Get(), read, buffer.Get());
                CheckJavaException(env);
                if (count < 0)
                {
                    break;
                }
                if (count == 0)
                {
                    continue;
                }

                env->CallVoidMethod(
                    into.Get(),
                    write,
                    buffer.Get(),
                    static_cast<jint>(0),
                    count
                );
                CheckJavaException(env);
            }

            const jmethodID fsync = GetMethodId(
                env,
                sessionClass.Get(),
                "fsync",
                "(Ljava/io/OutputStream;)V"
            );
            env->CallVoidMethod(session, fsync, into.Get());
            CheckJavaException(env);
        }
        catch (...)
        {
            pending = std::current_exception();
        }

        if (from)
        {
            try
            {
                CloseJavaObject(env, from.Get());
            }
            catch (...)
            {
                pending = std::current_exception();
            }
        }

        try
        {
            CloseJavaObject(env, into.Get());
        }
        catch (...)
        {
            pending = std::current_exception();
        }

        if (pending)
        {
            std::rethrow_exception(pending);
        }
    }

    void LogRequestPermissionFailure(const std::string& message)
    {
        std::cout << "[update] could not open the install-source setting: "
                  << message << '\n';
    }

    void LogSignatureFailure(const std::string& message)
    {
        std::cout << "[update] could not compare signatures: "
                  << message << '\n';
    }

    void LogInstallFailure(const std::string& display)
    {
        std::cout << "[update] the install session failed: "
                  << display << '\n';
    }

    std::string ExceptionMessage(const std::exception& ex)
    {
        if (const auto* java = dynamic_cast<const AndroidJavaException*>(&ex))
        {
            return java->Message();
        }
        if (const auto* managed = dynamic_cast<const ManagedException*>(&ex))
        {
            return managed->Message();
        }
        return ex.what();
    }

    LocalRef<jobject> GetPackageInstaller(JNIEnv* env, jobject context)
    {
        LocalRef<jobject> manager = PackageManager(env, context);
        if (!manager)
        {
            return {};
        }

        LocalRef<jclass> managerClass = FindClass(
            env, "android/content/pm/PackageManager"
        );
        const jmethodID getInstaller = GetMethodId(
            env,
            managerClass.Get(),
            "getPackageInstaller",
            "()Landroid/content/pm/PackageInstaller;"
        );
        LocalRef<jobject> installer(
            env,
            env->CallObjectMethod(manager.Get(), getInstaller)
        );
        CheckJavaException(env);
        return installer;
    }

    void StartActivity(JNIEnv* env, jobject context, jobject intent)
    {
        if (context == nullptr)
        {
            ThrowNullReference();
        }

        LocalRef<jclass> contextClass = FindClass(env, "android/content/Context");
        const jmethodID startActivity = GetMethodId(
            env,
            contextClass.Get(),
            "startActivity",
            "(Landroid/content/Intent;)V"
        );
        env->CallVoidMethod(context, startActivity, intent);
        CheckJavaException(env);
    }

    constexpr jint StatusPendingUserAction = -1;
    constexpr jint StatusSuccess = 0;
    constexpr jint StatusFailure = 1;
    constexpr jint StatusFailureBlocked = 2;
    constexpr jint StatusFailureAborted = 3;
    constexpr jint StatusFailureInvalid = 4;
    constexpr jint StatusFailureConflict = 5;
    constexpr jint StatusFailureStorage = 6;
    constexpr jint StatusFailureIncompatible = 7;
}

namespace MphRead::Droid
{
    ApkInstaller::FinishedHandler ApkInstaller::_finished{};
    jclass InstallResultReceiver::_javaClass = nullptr;

    ApkInstaller::FinishedHandler ApkInstaller::Finished()
    {
        return _finished;
    }

    void ApkInstaller::Finished(FinishedHandler value)
    {
        _finished = std::move(value);
    }

    std::string ApkInstaller::StagingPath(JNIEnv* env, jobject context)
    {
        RequireEnv(env);

        const std::string directory = CombinePath(
            CacheDirectory(env, context),
            "update"
        );
        std::filesystem::create_directories(std::filesystem::path(directory));
        return CombinePath(directory, "update.apk");
    }

    bool ApkInstaller::Allowed(JNIEnv* env, jobject context)
    {
        RequireEnv(env);

        if (AndroidSdkInt(env) < 26)
        {
            return true;
        }

        LocalRef<jobject> manager = PackageManager(env, context);
        if (!manager)
        {
            return false;
        }

        LocalRef<jclass> managerClass = FindClass(
            env, "android/content/pm/PackageManager"
        );
        const jmethodID canRequest = GetMethodId(
            env,
            managerClass.Get(),
            "canRequestPackageInstalls",
            "()Z"
        );
        const jboolean result = env->CallBooleanMethod(manager.Get(), canRequest);
        CheckJavaException(env);
        return result == JNI_TRUE;
    }

    bool ApkInstaller::RequestPermission(JNIEnv* env, jobject activity)
    {
        RequireEnv(env);

        try
        {
            LocalRef<jstring> packageName = PackageName(env, activity);
            std::string packageUri = "package:";
            if (packageName)
            {
                packageUri += JavaStringToUtf8(env, packageName.Get());
            }

            LocalRef<jclass> uriClass = FindClass(env, "android/net/Uri");
            const jmethodID parse = GetStaticMethodId(
                env,
                uriClass.Get(),
                "parse",
                "(Ljava/lang/String;)Landroid/net/Uri;"
            );
            LocalRef<jstring> javaPackageUri = NewJavaString(env, packageUri);
            LocalRef<jobject> uri(
                env,
                env->CallStaticObjectMethod(
                    uriClass.Get(),
                    parse,
                    javaPackageUri.Get()
                )
            );
            CheckJavaException(env);

            LocalRef<jclass> intentClass = FindClass(env, "android/content/Intent");
            const jmethodID constructor = GetMethodId(
                env,
                intentClass.Get(),
                "<init>",
                "(Ljava/lang/String;Landroid/net/Uri;)V"
            );
            LocalRef<jstring> action = NewJavaString(
                env, "android.settings.MANAGE_UNKNOWN_APP_SOURCES"
            );
            LocalRef<jobject> intent(
                env,
                env->NewObject(
                    intentClass.Get(),
                    constructor,
                    action.Get(),
                    uri.Get()
                )
            );
            CheckJavaException(env);
            if (!intent)
            {
                throw std::bad_alloc();
            }

            StartActivity(env, activity, intent.Get());
            return true;
        }
        catch (const std::exception& ex)
        {
            LogRequestPermissionFailure(ExceptionMessage(ex));
            return false;
        }
    }

    bool ApkInstaller::SameSigner(
        JNIEnv* env,
        jobject context,
        const std::string& apkPath,
        std::optional<std::string>& mismatch
    )
    {
        RequireEnv(env);
        mismatch.reset();

        try
        {
            LocalRef<jobject> manager = PackageManager(env, context);
            if (!manager)
            {
                return true;
            }

            LocalRef<jclass> managerClass = FindClass(
                env, "android/content/pm/PackageManager"
            );
            const jint signaturesFlag = GetStaticIntField(
                env, managerClass.Get(), "GET_SIGNATURES"
            );
            const jmethodID getPackageInfo = GetMethodId(
                env,
                managerClass.Get(),
                "getPackageInfo",
                "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;"
            );
            const jmethodID getPackageArchiveInfo = GetMethodId(
                env,
                managerClass.Get(),
                "getPackageArchiveInfo",
                "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;"
            );

            LocalRef<jstring> packageName = PackageName(env, context);
            LocalRef<jobject> installed(
                env,
                env->CallObjectMethod(
                    manager.Get(),
                    getPackageInfo,
                    packageName.Get(),
                    signaturesFlag
                )
            );
            CheckJavaException(env);

            LocalRef<jstring> javaApkPath = NewJavaString(env, apkPath);
            LocalRef<jobject> downloaded(
                env,
                env->CallObjectMethod(
                    manager.Get(),
                    getPackageArchiveInfo,
                    javaApkPath.Get(),
                    signaturesFlag
                )
            );
            CheckJavaException(env);

            if (!installed || !downloaded)
            {
                return true;
            }

            LocalRef<jclass> packageInfoClass = FindClass(
                env, "android/content/pm/PackageInfo"
            );
            const jfieldID signatures = GetFieldId(
                env,
                packageInfoClass.Get(),
                "signatures",
                "[Landroid/content/pm/Signature;"
            );

            LocalRef<jobjectArray> mine(
                env,
                static_cast<jobjectArray>(
                    env->GetObjectField(installed.Get(), signatures)
                )
            );
            CheckJavaException(env);
            LocalRef<jobjectArray> theirs(
                env,
                static_cast<jobjectArray>(
                    env->GetObjectField(downloaded.Get(), signatures)
                )
            );
            CheckJavaException(env);

            if (!mine || !theirs)
            {
                return true;
            }

            const jsize mineCount = env->GetArrayLength(mine.Get());
            CheckJavaException(env);
            const jsize theirCount = env->GetArrayLength(theirs.Get());
            CheckJavaException(env);
            if (mineCount == 0 || theirCount == 0)
            {
                return true;
            }

            LocalRef<jclass> signatureClass = FindClass(
                env, "android/content/pm/Signature"
            );
            const jmethodID equals = GetMethodId(
                env,
                signatureClass.Get(),
                "equals",
                "(Ljava/lang/Object;)Z"
            );

            for (jsize i = 0; i < mineCount; ++i)
            {
                LocalRef<jobject> ours(
                    env,
                    env->GetObjectArrayElement(mine.Get(), i)
                );
                CheckJavaException(env);
                if (!ours)
                {
                    ThrowNullReference();
                }

                for (jsize j = 0; j < theirCount; ++j)
                {
                    LocalRef<jobject> other(
                        env,
                        env->GetObjectArrayElement(theirs.Get(), j)
                    );
                    CheckJavaException(env);

                    const jboolean same = env->CallBooleanMethod(
                        ours.Get(),
                        equals,
                        other.Get()
                    );
                    CheckJavaException(env);
                    if (same == JNI_TRUE)
                    {
                        return true;
                    }
                }
            }

            mismatch =
                "that download is signed with a different key, so Android "
                "will not install it over this copy. Install it by hand once.";
            return false;
        }
        catch (const std::exception& ex)
        {
            LogSignatureFailure(ExceptionMessage(ex));
            return true;
        }
    }

    bool ApkInstaller::Commit(
        JNIEnv* env,
        jobject context,
        const std::string& apkPath,
        std::string& error
    )
    {
        RequireEnv(env);
        error.clear();

        if (!FileExists(apkPath))
        {
            error = "the download is not there";
            return false;
        }

        // This property access is deliberately outside the try block, matching
        // the C# source exactly.
        LocalRef<jobject> installer = GetPackageInstaller(env, context);
        if (!installer)
        {
            error = "this device has no package installer";
            return false;
        }

        LocalRef<jobject> session;
        bool result = false;

        try
        {
            LocalRef<jclass> parametersClass = FindClass(
                env, "android/content/pm/PackageInstaller$SessionParams"
            );
            const jint fullInstall = GetStaticIntField(
                env, parametersClass.Get(), "MODE_FULL_INSTALL"
            );
            const jmethodID parametersConstructor = GetMethodId(
                env,
                parametersClass.Get(),
                "<init>",
                "(I)V"
            );
            LocalRef<jobject> parameters(
                env,
                env->NewObject(
                    parametersClass.Get(),
                    parametersConstructor,
                    fullInstall
                )
            );
            CheckJavaException(env);
            if (!parameters)
            {
                throw std::bad_alloc();
            }

            const jmethodID setAppPackageName = GetMethodId(
                env,
                parametersClass.Get(),
                "setAppPackageName",
                "(Ljava/lang/String;)V"
            );
            LocalRef<jstring> packageName = PackageName(env, context);
            env->CallVoidMethod(
                parameters.Get(),
                setAppPackageName,
                packageName.Get()
            );
            CheckJavaException(env);

            LocalRef<jclass> installerClass = FindClass(
                env, "android/content/pm/PackageInstaller"
            );
            const jmethodID createSession = GetMethodId(
                env,
                installerClass.Get(),
                "createSession",
                "(Landroid/content/pm/PackageInstaller$SessionParams;)I"
            );
            const jint sessionId = env->CallIntMethod(
                installer.Get(),
                createSession,
                parameters.Get()
            );
            CheckJavaException(env);

            const jmethodID openSession = GetMethodId(
                env,
                installerClass.Get(),
                "openSession",
                "(I)Landroid/content/pm/PackageInstaller$Session;"
            );
            session = LocalRef<jobject>(
                env,
                env->CallObjectMethod(
                    installer.Get(),
                    openSession,
                    sessionId
                )
            );
            CheckJavaException(env);
            if (!session)
            {
                ThrowNullReference();
            }

            CopyPackageToSession(env, session.Get(), apkPath);

            jclass receiverClass = InstallResultReceiver::JavaClass();
            if (receiverClass == nullptr)
            {
                throw std::runtime_error(
                    "Android InstallResultReceiver Java owner is not bound"
                );
            }

            LocalRef<jclass> intentClass = FindClass(env, "android/content/Intent");
            const jmethodID intentConstructor = GetMethodId(
                env,
                intentClass.Get(),
                "<init>",
                "(Landroid/content/Context;Ljava/lang/Class;)V"
            );
            LocalRef<jobject> intent(
                env,
                env->NewObject(
                    intentClass.Get(),
                    intentConstructor,
                    context,
                    receiverClass
                )
            );
            CheckJavaException(env);
            if (!intent)
            {
                throw std::bad_alloc();
            }

            LocalRef<jclass> pendingIntentClass = FindClass(
                env, "android/app/PendingIntent"
            );
            jint flags = GetStaticIntField(
                env, pendingIntentClass.Get(), "FLAG_UPDATE_CURRENT"
            );
            if (AndroidSdkInt(env) >= 31)
            {
                flags |= GetStaticIntField(
                    env, pendingIntentClass.Get(), "FLAG_MUTABLE"
                );
            }

            const jmethodID getBroadcast = GetStaticMethodId(
                env,
                pendingIntentClass.Get(),
                "getBroadcast",
                "(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;"
            );
            LocalRef<jobject> pending(
                env,
                env->CallStaticObjectMethod(
                    pendingIntentClass.Get(),
                    getBroadcast,
                    context,
                    sessionId,
                    intent.Get(),
                    flags
                )
            );
            CheckJavaException(env);
            if (!pending)
            {
                ThrowNullReference();
            }

            const jmethodID getIntentSender = GetMethodId(
                env,
                pendingIntentClass.Get(),
                "getIntentSender",
                "()Landroid/content/IntentSender;"
            );
            LocalRef<jobject> intentSender(
                env,
                env->CallObjectMethod(pending.Get(), getIntentSender)
            );
            CheckJavaException(env);
            if (!intentSender)
            {
                ThrowNullReference();
            }

            LocalRef<jclass> sessionClass = FindClass(
                env, "android/content/pm/PackageInstaller$Session"
            );
            const jmethodID commit = GetMethodId(
                env,
                sessionClass.Get(),
                "commit",
                "(Landroid/content/IntentSender;)V"
            );
            env->CallVoidMethod(
                session.Get(),
                commit,
                intentSender.Get()
            );
            CheckJavaException(env);
            result = true;
        }
        catch (const std::exception& ex)
        {
            error = ExceptionMessage(ex);
            LogInstallFailure(ex.what());
            result = false;
        }

        // The C# finally can itself throw and thereby replace the true/false
        // return from the try/catch. Do not swallow a Session.Close failure.
        if (session)
        {
            CloseJavaObject(env, session.Get());
        }

        return result;
    }

    void InstallResultReceiver::JavaClass(JNIEnv* env, jclass receiverClass)
    {
        RequireEnv(env);

        jclass replacement = nullptr;
        if (receiverClass != nullptr)
        {
            replacement = static_cast<jclass>(env->NewGlobalRef(receiverClass));
            CheckJavaException(env);
            if (replacement == nullptr)
            {
                throw std::bad_alloc();
            }
        }

        if (_javaClass != nullptr)
        {
            env->DeleteGlobalRef(_javaClass);
        }
        _javaClass = replacement;
    }

    jclass InstallResultReceiver::JavaClass()
    {
        return _javaClass;
    }

    void InstallResultReceiver::OnReceive(
        JNIEnv* env,
        jobject context,
        jobject intent
    )
    {
        RequireEnv(env);

        if (intent == nullptr)
        {
            return;
        }

        LocalRef<jclass> packageInstallerClass = FindClass(
            env, "android/content/pm/PackageInstaller"
        );
        LocalRef<jstring> extraStatus = GetStaticStringField(
            env, packageInstallerClass.Get(), "EXTRA_STATUS"
        );
        LocalRef<jstring> extraStatusMessage = GetStaticStringField(
            env, packageInstallerClass.Get(), "EXTRA_STATUS_MESSAGE"
        );

        LocalRef<jclass> intentClass = FindClass(env, "android/content/Intent");
        const jmethodID getIntExtra = GetMethodId(
            env,
            intentClass.Get(),
            "getIntExtra",
            "(Ljava/lang/String;I)I"
        );
        const jint status = env->CallIntMethod(
            intent,
            getIntExtra,
            extraStatus.Get(),
            StatusFailure
        );
        CheckJavaException(env);

        const jmethodID getStringExtra = GetMethodId(
            env,
            intentClass.Get(),
            "getStringExtra",
            "(Ljava/lang/String;)Ljava/lang/String;"
        );
        LocalRef<jstring> javaMessage(
            env,
            static_cast<jstring>(
                env->CallObjectMethod(
                    intent,
                    getStringExtra,
                    extraStatusMessage.Get()
                )
            )
        );
        CheckJavaException(env);
        const std::string message = javaMessage
            ? JavaStringToUtf8(env, javaMessage.Get())
            : std::string{};

        if (status == StatusPendingUserAction)
        {
            LocalRef<jstring> extraIntent = GetStaticStringField(
                env, intentClass.Get(), "EXTRA_INTENT"
            );
            LocalRef<jobject> confirm;

            if (AndroidSdkInt(env) >= 33)
            {
                const jmethodID getParcelableExtra = GetMethodId(
                    env,
                    intentClass.Get(),
                    "getParcelableExtra",
                    "(Ljava/lang/String;Ljava/lang/Class;)Ljava/lang/Object;"
                );
                confirm = LocalRef<jobject>(
                    env,
                    env->CallObjectMethod(
                        intent,
                        getParcelableExtra,
                        extraIntent.Get(),
                        intentClass.Get()
                    )
                );
                CheckJavaException(env);
            }
            else
            {
                const jmethodID getParcelableExtra = GetMethodId(
                    env,
                    intentClass.Get(),
                    "getParcelableExtra",
                    "(Ljava/lang/String;)Landroid/os/Parcelable;"
                );
                confirm = LocalRef<jobject>(
                    env,
                    env->CallObjectMethod(
                        intent,
                        getParcelableExtra,
                        extraIntent.Get()
                    )
                );
                CheckJavaException(env);
            }

            if (confirm && env->IsInstanceOf(confirm.Get(), intentClass.Get()) != JNI_TRUE)
            {
                confirm.Reset();
            }
            CheckJavaException(env);

            if (!confirm)
            {
                Report(false, "Android did not offer the install dialog");
                return;
            }

            const jmethodID addFlags = GetMethodId(
                env,
                intentClass.Get(),
                "addFlags",
                "(I)Landroid/content/Intent;"
            );
            const jint newTask = GetStaticIntField(
                env, intentClass.Get(), "FLAG_ACTIVITY_NEW_TASK"
            );
            LocalRef<jobject> addFlagsResult(
                env,
                env->CallObjectMethod(
                    confirm.Get(),
                    addFlags,
                    newTask
                )
            );
            CheckJavaException(env);

            try
            {
                if (context != nullptr)
                {
                    StartActivity(env, context, confirm.Get());
                }
            }
            catch (const std::exception& ex)
            {
                Report(false, ExceptionMessage(ex));
            }
            return;
        }

        if (status == StatusSuccess)
        {
            Report(true, "installed");
            return;
        }

        Report(false, Explain(status, message));
    }

    std::string InstallResultReceiver::Explain(
        jint status,
        const std::string& message
    )
    {
        switch (status)
        {
        case StatusFailureAborted:
            return "the update was cancelled";
        case StatusFailureConflict:
            return
                "Android refused it: the download is signed with a different key "
                "than the copy installed. Install it by hand once.";
        case StatusFailureStorage:
            return "there is not enough room for it";
        case StatusFailureIncompatible:
            return "that package is not for this device";
        case StatusFailureBlocked:
            return "the device blocked the install";
        case StatusFailureInvalid:
            return message.empty() ? "the package could not be read" : message;
        default:
            return message.empty() ? "the install failed" : message;
        }
    }

    void InstallResultReceiver::Report(
        bool ok,
        const std::string& message
    )
    {
        std::cout << "[update] install: "
                  << (ok ? "ok" : "failed")
                  << " -- "
                  << message
                  << '\n';

        ApkInstaller::FinishedHandler finished = ApkInstaller::Finished();
        if (!finished)
        {
            return;
        }

        // Android delivers manifest BroadcastReceiver.onReceive on the main
        // thread by default. The external Java owner must preserve that normal
        // receiver dispatch; with that owner in place this is the same UI-thread
        // call the C# MainActivity.RunOnUiThread path produces.
        finished(ok, message);
    }
}

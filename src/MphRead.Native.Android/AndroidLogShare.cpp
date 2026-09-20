#include "AndroidLogShare.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

        T Get() const noexcept
        {
            return _value;
        }

        explicit operator bool() const noexcept
        {
            return _value != nullptr;
        }

        T Release() noexcept
        {
            T value = _value;
            _env = nullptr;
            _value = nullptr;
            return value;
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
        ManagedException(std::u16string message, std::string text)
            : _message(std::move(message)), _text(std::move(text))
        {
        }

        const char* what() const noexcept override
        {
            return _text.c_str();
        }

        const std::u16string& Message() const noexcept
        {
            return _message;
        }

    private:
        std::u16string _message;
        std::string _text;
    };

    class AndroidJavaException final : public std::exception
    {
    public:
        AndroidJavaException(std::u16string message, std::u16string display)
            : _message(std::move(message)),
              _text(Utf16ToUtf8(display.empty() ? _message : display))
        {
            if (_text.empty())
            {
                _text = "Android Java exception";
            }
        }

        const char* what() const noexcept override
        {
            return _text.c_str();
        }

        const std::u16string& Message() const noexcept
        {
            return _message;
        }

    private:
        std::u16string _message;
        std::string _text;
    };

    class ScopedJniEnv final
    {
    public:
        explicit ScopedJniEnv(JavaVM* javaVm)
            : _javaVm(javaVm)
        {
            if (_javaVm == nullptr)
            {
                throw std::runtime_error("Android Java VM is not available");
            }

            const jint result = _javaVm->GetEnv(
                reinterpret_cast<void**>(&_env), JNI_VERSION_1_6
            );
            if (result == JNI_EDETACHED)
            {
                if (_javaVm->AttachCurrentThread(&_env, nullptr) != JNI_OK)
                {
                    throw std::runtime_error(
                        "could not attach the current thread to the Android Java VM"
                    );
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
                _javaVm->DetachCurrentThread();
            }
        }

        ScopedJniEnv(const ScopedJniEnv&) = delete;
        ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

        JNIEnv* Get() const noexcept
        {
            return _env;
        }

    private:
        JavaVM* _javaVm;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    std::optional<std::u16string> TryReadJavaString(JNIEnv* env, jstring value) noexcept
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
            std::u16string result(
                reinterpret_cast<const char16_t*>(chars),
                reinterpret_cast<const char16_t*>(chars) + length
            );
            env->ReleaseStringChars(value, chars);
            return result;
        }
        catch (...)
        {
            env->ReleaseStringChars(value, chars);
            return std::nullopt;
        }
    }

    std::optional<std::u16string> TryThrowableText(
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
        return TryReadJavaString(env, text.Get());
    }

    [[noreturn]] void ThrowPendingJavaException(JNIEnv* env)
    {
        LocalRef<jthrowable> throwable(env, env->ExceptionOccurred());
        env->ExceptionClear();

        std::u16string display = TryThrowableText(env, throwable.Get(), "toString")
            .value_or(u"Android Java exception");
        std::u16string message = TryThrowableText(env, throwable.Get(), "getMessage")
            .value_or(display);
        throw AndroidJavaException(std::move(message), std::move(display));
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
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<jsize>::max()))
        {
            throw std::length_error("string is too long for Android JNI");
        }
        static const jchar empty = 0;
        const jchar* chars = value.empty()
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

    std::u16string JavaStringToUtf16(JNIEnv* env, jstring value)
    {
        if (value == nullptr)
        {
            throw ManagedException(
                u"Object reference not set to an instance of an object.",
                "System.NullReferenceException: Object reference not set to an instance of an object."
            );
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
            std::u16string result(
                reinterpret_cast<const char16_t*>(chars),
                reinterpret_cast<const char16_t*>(chars) + length
            );
            env->ReleaseStringChars(value, chars);
            return result;
        }
        catch (...)
        {
            env->ReleaseStringChars(value, chars);
            throw;
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

    LocalRef<jstring> GetStaticStringField(JNIEnv* env, jclass type, const char* name)
    {
        const jfieldID field = GetStaticFieldId(
            env, type, name, "Ljava/lang/String;"
        );
        LocalRef<jstring> result(
            env,
            static_cast<jstring>(env->GetStaticObjectField(type, field))
        );
        CheckJavaException(env);
        return result;
    }

    jint GetStaticIntField(JNIEnv* env, jclass type, const char* name)
    {
        const jfieldID field = GetStaticFieldId(env, type, name, "I");
        const jint result = env->GetStaticIntField(type, field);
        CheckJavaException(env);
        return result;
    }

    [[noreturn]] void ThrowNullReference()
    {
        throw ManagedException(
            u"Object reference not set to an instance of an object.",
            "System.NullReferenceException: Object reference not set to an instance of an object."
        );
    }

    LocalRef<jclass> LoadFileProviderClass(JNIEnv* env, jobject context)
    {
        if (context == nullptr)
        {
            ThrowNullReference();
        }

        LocalRef<jclass> contextClass = FindClass(env, "android/content/Context");
        const jmethodID getClassLoader = GetMethodId(
            env,
            contextClass.Get(),
            "getClassLoader",
            "()Ljava/lang/ClassLoader;"
        );
        LocalRef<jobject> classLoader(
            env,
            env->CallObjectMethod(context, getClassLoader)
        );
        CheckJavaException(env);
        if (!classLoader)
        {
            throw std::runtime_error("the Android context class loader is not available");
        }

        LocalRef<jclass> classLoaderClass = FindClass(env, "java/lang/ClassLoader");
        const jmethodID loadClass = GetMethodId(
            env,
            classLoaderClass.Get(),
            "loadClass",
            "(Ljava/lang/String;)Ljava/lang/Class;"
        );
        LocalRef<jstring> className = NewJavaString(
            env, u"androidx.core.content.FileProvider"
        );
        LocalRef<jobject> loaded(
            env,
            env->CallObjectMethod(classLoader.Get(), loadClass, className.Get())
        );
        CheckJavaException(env);
        if (!loaded)
        {
            throw std::runtime_error("Android class not found: androidx/core/content/FileProvider");
        }
        return LocalRef<jclass>(env, static_cast<jclass>(loaded.Release()));
    }

    std::u16string GetDotNetTempPath()
    {
        const char* value = std::getenv("TMPDIR");
        if (value == nullptr || value[0] == '\0')
        {
            return u"/tmp/";
        }
        std::u16string result = Utf8ToUtf16(value);
        if (result.empty() || result.back() != u'/')
        {
            result.push_back(u'/');
        }
        return result;
    }

    std::u16string GetCacheDirectory(JNIEnv* env, jobject context)
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
                return JavaStringToUtf16(env, absolutePath.Get());
            }
        }
        return GetDotNetTempPath();
    }

    std::u16string CombinePath(
        std::u16string_view left,
        std::u16string_view right
    )
    {
        if (right.empty())
        {
            return std::u16string(left);
        }
        std::filesystem::path result{std::u16string(left)};
        result /= std::filesystem::path{std::u16string(right)};
        return result.u16string();
    }

    void LogShareException(std::string_view text)
    {
        std::cout << "[logs] could not share: " << text << '\n';
    }
}

namespace MphRead::Droid
{
    AndroidLogShare::AndroidLogShare(JNIEnv* env, jobject context)
    {
        if (env == nullptr)
        {
            throw std::invalid_argument("Android JNI environment must not be null");
        }
        if (env->GetJavaVM(&_javaVm) != JNI_OK || _javaVm == nullptr)
        {
            throw std::runtime_error("Android Java VM is not available");
        }

        _context = env->NewGlobalRef(context);
        CheckJavaException(env);
        if (context != nullptr && _context == nullptr)
        {
            throw std::bad_alloc();
        }
    }

    AndroidLogShare::~AndroidLogShare()
    {
        if (_javaVm == nullptr || _context == nullptr)
        {
            return;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        const jint result = _javaVm->GetEnv(
            reinterpret_cast<void**>(&env), JNI_VERSION_1_6
        );
        if (result == JNI_EDETACHED)
        {
            if (_javaVm->AttachCurrentThread(&env, nullptr) == JNI_OK)
            {
                attached = true;
            }
            else
            {
                return;
            }
        }
        else if (result != JNI_OK || env == nullptr)
        {
            return;
        }

        env->DeleteGlobalRef(_context);
        if (attached)
        {
            _javaVm->DetachCurrentThread();
        }
    }

    std::u16string AndroidLogShare::StagingPath(std::u16string_view fileName)
    {
        ScopedJniEnv scopedEnv(_javaVm);
        JNIEnv* const env = scopedEnv.Get();

        const std::u16string directory = CombinePath(
            GetCacheDirectory(env, _context), _folder
        );
        std::filesystem::create_directories(std::filesystem::path(directory));

        try
        {
            std::vector<std::filesystem::path> oldFiles;
            for (const std::filesystem::directory_entry& entry
                : std::filesystem::directory_iterator(std::filesystem::path(directory)))
            {
                const std::u16string name = entry.path().filename().u16string();
                if (!entry.is_directory()
                    && name.size() >= 4
                    && name.compare(name.size() - 4, 4, u".zip") == 0)
                {
                    oldFiles.push_back(entry.path());
                }
            }
            for (const std::filesystem::path& old : oldFiles)
            {
                std::filesystem::remove(old);
            }
        }
        catch (const std::exception&)
        {
        }

        return CombinePath(directory, fileName);
    }

    bool AndroidLogShare::Share(
        std::u16string_view path,
        std::u16string_view subject,
        std::u16string& error
    )
    {
        std::u16string pathStorage;
        std::u16string subjectStorage;
        if (path.data() == error.data())
        {
            pathStorage.assign(path);
            path = pathStorage;
        }
        if (subject.data() == error.data())
        {
            subjectStorage.assign(subject);
            subject = subjectStorage;
        }
        error.clear();
        try
        {
            ScopedJniEnv scopedEnv(_javaVm);
            JNIEnv* const env = scopedEnv.Get();

            LocalRef<jclass> fileClass = FindClass(env, "java/io/File");
            const jmethodID fileConstructor = GetMethodId(
                env,
                fileClass.Get(),
                "<init>",
                "(Ljava/lang/String;)V"
            );
            LocalRef<jstring> javaPath = NewJavaString(env, path);
            LocalRef<jobject> file(
                env,
                env->NewObject(fileClass.Get(), fileConstructor, javaPath.Get())
            );
            CheckJavaException(env);
            if (!file)
            {
                throw std::bad_alloc();
            }

            if (_context == nullptr)
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
            LocalRef<jstring> packageName(
                env,
                static_cast<jstring>(
                    env->CallObjectMethod(_context, getPackageName)
                )
            );
            CheckJavaException(env);
            std::u16string authority;
            if (packageName)
            {
                authority = JavaStringToUtf16(env, packageName.Get());
            }
            authority += u".logs";
            LocalRef<jstring> javaAuthority = NewJavaString(env, authority);

            LocalRef<jclass> fileProviderClass = LoadFileProviderClass(env, _context);
            const jmethodID getUriForFile = GetStaticMethodId(
                env,
                fileProviderClass.Get(),
                "getUriForFile",
                "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;"
            );
            LocalRef<jobject> uri(
                env,
                env->CallStaticObjectMethod(
                    fileProviderClass.Get(),
                    getUriForFile,
                    _context,
                    javaAuthority.Get(),
                    file.Get()
                )
            );
            CheckJavaException(env);
            if (!uri)
            {
                error = u"the file could not be offered";
                return false;
            }

            LocalRef<jclass> intentClass = FindClass(env, "android/content/Intent");
            LocalRef<jstring> actionSend = GetStaticStringField(
                env, intentClass.Get(), "ACTION_SEND"
            );
            const jmethodID intentConstructor = GetMethodId(
                env,
                intentClass.Get(),
                "<init>",
                "(Ljava/lang/String;)V"
            );
            LocalRef<jobject> intent(
                env,
                env->NewObject(
                    intentClass.Get(), intentConstructor, actionSend.Get()
                )
            );
            CheckJavaException(env);
            if (!intent)
            {
                throw std::bad_alloc();
            }

            const jmethodID setType = GetMethodId(
                env,
                intentClass.Get(),
                "setType",
                "(Ljava/lang/String;)Landroid/content/Intent;"
            );
            LocalRef<jstring> mimeType = NewJavaString(env, u"application/zip");
            LocalRef<jobject> setTypeResult(
                env,
                env->CallObjectMethod(intent.Get(), setType, mimeType.Get())
            );
            CheckJavaException(env);

            LocalRef<jstring> extraStream = GetStaticStringField(
                env, intentClass.Get(), "EXTRA_STREAM"
            );
            const jmethodID putParcelableExtra = GetMethodId(
                env,
                intentClass.Get(),
                "putExtra",
                "(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;"
            );
            LocalRef<jobject> streamExtraResult(
                env,
                env->CallObjectMethod(
                    intent.Get(), putParcelableExtra, extraStream.Get(), uri.Get()
                )
            );
            CheckJavaException(env);

            LocalRef<jstring> extraSubject = GetStaticStringField(
                env, intentClass.Get(), "EXTRA_SUBJECT"
            );
            LocalRef<jstring> javaSubject = NewJavaString(env, subject);
            const jmethodID putStringExtra = GetMethodId(
                env,
                intentClass.Get(),
                "putExtra",
                "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;"
            );
            LocalRef<jobject> subjectExtraResult(
                env,
                env->CallObjectMethod(
                    intent.Get(), putStringExtra, extraSubject.Get(), javaSubject.Get()
                )
            );
            CheckJavaException(env);

            const jmethodID addFlags = GetMethodId(
                env,
                intentClass.Get(),
                "addFlags",
                "(I)Landroid/content/Intent;"
            );
            const jint grantReadUriPermission = GetStaticIntField(
                env, intentClass.Get(), "FLAG_GRANT_READ_URI_PERMISSION"
            );
            LocalRef<jobject> grantResult(
                env,
                env->CallObjectMethod(
                    intent.Get(), addFlags, grantReadUriPermission
                )
            );
            CheckJavaException(env);

            const jmethodID createChooser = GetStaticMethodId(
                env,
                intentClass.Get(),
                "createChooser",
                "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;"
            );
            LocalRef<jstring> chooserTitle = NewJavaString(env, u"Share logs");
            LocalRef<jobject> chooser(
                env,
                env->CallStaticObjectMethod(
                    intentClass.Get(),
                    createChooser,
                    intent.Get(),
                    chooserTitle.Get()
                )
            );
            CheckJavaException(env);
            if (!chooser)
            {
                ThrowNullReference();
            }

            const jint newTask = GetStaticIntField(
                env, intentClass.Get(), "FLAG_ACTIVITY_NEW_TASK"
            );
            LocalRef<jobject> newTaskResult(
                env,
                env->CallObjectMethod(chooser.Get(), addFlags, newTask)
            );
            CheckJavaException(env);

            const jmethodID startActivity = GetMethodId(
                env,
                contextClass.Get(),
                "startActivity",
                "(Landroid/content/Intent;)V"
            );
            env->CallVoidMethod(_context, startActivity, chooser.Get());
            CheckJavaException(env);
            return true;
        }
        catch (const AndroidJavaException& ex)
        {
            LogShareException(ex.what());
            error = ex.Message();
            return false;
        }
        catch (const ManagedException& ex)
        {
            LogShareException(ex.what());
            error = ex.Message();
            return false;
        }
        catch (const std::exception& ex)
        {
            LogShareException(ex.what());
            error = Utf8ToUtf16(ex.what());
            return false;
        }
    }
}

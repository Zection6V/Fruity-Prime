#include "TagList.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__ANDROID__)
#include <dlfcn.h>
#include <jni.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#else
#include <unicode/ucol.h>
#endif

namespace
{
    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was out of range. Must be non-negative and less than the size of the collection.");
    }

    [[noreturn]] void ThrowInsertIndexOutOfRange()
    {
        throw std::out_of_range("Index must be within the bounds of the collection.");
    }

    [[noreturn]] void ThrowArgumentNullKey()
    {
        throw std::invalid_argument("Value cannot be null. (Parameter 'key')");
    }

    [[noreturn]] void ThrowArgumentNullObject()
    {
        throw std::invalid_argument("Value cannot be null. (Parameter 'obj')");
    }

    [[noreturn]] void ThrowDuplicateKey()
    {
        throw std::invalid_argument("An item with the same key has already been added.");
    }

    [[noreturn]] void ThrowKeyNotFound()
    {
        throw std::out_of_range("The given key was not present in the collection.");
    }

    [[noreturn]] void ThrowEnumerationModified()
    {
        throw std::logic_error("Collection was modified; enumeration operation may not execute.");
    }

    [[nodiscard]] std::int32_t HashBytes(const std::uint8_t* data, std::size_t size) noexcept
    {
        std::uint32_t hash = 2166136261U;
        for (std::size_t index = 0; index < size; ++index)
        {
            hash ^= data[index];
            hash *= 16777619U;
        }
        return std::bit_cast<std::int32_t>(hash);
    }

#if defined(_WIN32)
    [[nodiscard]] bool InvariantCultureIgnoreCaseEquals(
        std::u16string_view left, std::u16string_view right)
    {
        if (left.data() == right.data() && left.size() == right.size())
        {
            return true;
        }
        if (left.empty() || right.empty())
        {
            return left.empty() && right.empty();
        }
        if (left.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            || right.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("String length exceeds the native comparison limit.");
        }

        const int result = CompareStringEx(
            LOCALE_NAME_INVARIANT,
            NORM_IGNORECASE | NORM_LINGUISTIC_CASING,
            reinterpret_cast<const wchar_t*>(left.data()),
            static_cast<int>(left.size()),
            reinterpret_cast<const wchar_t*>(right.data()),
            static_cast<int>(right.size()),
            nullptr,
            nullptr,
            0);
        if (result == 0)
        {
            throw std::runtime_error("Invariant culture string comparison failed.");
        }
        return result == CSTR_EQUAL;
    }

    [[nodiscard]] std::int32_t InvariantCultureIgnoreCaseHash(std::u16string_view value)
    {
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("String length exceeds the native comparison limit.");
        }

        static constexpr wchar_t Empty[] = L"";
        const wchar_t* source = value.empty()
            ? Empty
            : reinterpret_cast<const wchar_t*>(value.data());
        const int sourceLength = value.empty() ? -1 : static_cast<int>(value.size());
        constexpr DWORD Flags = LCMAP_SORTKEY | NORM_IGNORECASE | NORM_LINGUISTIC_CASING;

        const int required = LCMapStringEx(
            LOCALE_NAME_INVARIANT,
            Flags,
            source,
            sourceLength,
            nullptr,
            0,
            nullptr,
            nullptr,
            0);
        if (required == 0)
        {
            throw std::runtime_error("Invariant culture sort-key generation failed.");
        }

        std::vector<wchar_t> storage((static_cast<std::size_t>(required) + sizeof(wchar_t) - 1) / sizeof(wchar_t));
        if (LCMapStringEx(
                LOCALE_NAME_INVARIANT,
                Flags,
                source,
                sourceLength,
                storage.data(),
                required,
                nullptr,
                nullptr,
                0) != required)
        {
            throw std::runtime_error("Invariant culture sort-key generation failed.");
        }
        return HashBytes(
            reinterpret_cast<const std::uint8_t*>(storage.data()),
            static_cast<std::size_t>(required));
    }
#elif defined(__ANDROID__)
    template <typename T>
    class LocalJavaRef final
    {
    public:
        LocalJavaRef() noexcept = default;

        LocalJavaRef(JNIEnv* env, T value) noexcept
            : _env(env),
              _value(value)
        {
        }

        ~LocalJavaRef()
        {
            Reset();
        }

        LocalJavaRef(const LocalJavaRef&) = delete;
        LocalJavaRef& operator=(const LocalJavaRef&) = delete;

        LocalJavaRef(LocalJavaRef&& other) noexcept
            : _env(other._env),
              _value(other._value)
        {
            other._env = nullptr;
            other._value = nullptr;
        }

        LocalJavaRef& operator=(LocalJavaRef&& other) noexcept
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

    [[noreturn]] void ThrowAndroidCollationFailure(
        JNIEnv* env,
        const char* message)
    {
        if (env != nullptr && env->ExceptionCheck())
        {
            env->ExceptionClear();
        }
        throw std::runtime_error(message);
    }

    void CheckAndroidJavaException(JNIEnv* env, const char* message)
    {
        if (env->ExceptionCheck())
        {
            ThrowAndroidCollationFailure(env, message);
        }
    }

    class ScopedAndroidJniEnv final
    {
    public:
        ScopedAndroidJniEnv()
        {
            using GetCreatedJavaVMs = jint (*)(JavaVM**, jsize, jsize*);
            const auto getCreatedJavaVMs = reinterpret_cast<GetCreatedJavaVMs>(
                dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs"));
            if (getCreatedJavaVMs == nullptr)
            {
                throw std::runtime_error(
                    "Android Java VM discovery is unavailable.");
            }

            jsize count = 0;
            if (getCreatedJavaVMs(&_javaVm, 1, &count) != JNI_OK
                || count != 1
                || _javaVm == nullptr)
            {
                throw std::runtime_error(
                    "Android Java VM is not available.");
            }

            const jint result = _javaVm->GetEnv(
                reinterpret_cast<void**>(&_env),
                JNI_VERSION_1_6);
            if (result == JNI_EDETACHED)
            {
                if (_javaVm->AttachCurrentThread(
                        reinterpret_cast<void**>(&_env),
                        nullptr) != JNI_OK)
                {
                    throw std::runtime_error(
                        "Could not attach the current thread to the Android Java VM.");
                }
                _attached = true;
            }
            else if (result != JNI_OK || _env == nullptr)
            {
                throw std::runtime_error(
                    "Could not obtain the Android JNI environment.");
            }
        }

        ~ScopedAndroidJniEnv()
        {
            if (_attached)
            {
                _javaVm->DetachCurrentThread();
            }
        }

        ScopedAndroidJniEnv(const ScopedAndroidJniEnv&) = delete;
        ScopedAndroidJniEnv& operator=(const ScopedAndroidJniEnv&) = delete;

        [[nodiscard]] JNIEnv* Get() const noexcept
        {
            return _env;
        }

    private:
        JavaVM* _javaVm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    [[nodiscard]] LocalJavaRef<jobject> CreateAndroidInvariantCollator(
        JNIEnv* env,
        LocalJavaRef<jclass>& collatorClass)
    {
        LocalJavaRef<jclass> localeClass(
            env,
            env->FindClass("java/util/Locale"));
        CheckAndroidJavaException(
            env,
            "Could not resolve java.util.Locale for invariant collation.");
        if (!localeClass)
        {
            throw std::runtime_error(
                "Could not resolve java.util.Locale for invariant collation.");
        }

        const jfieldID rootField = env->GetStaticFieldID(
            localeClass.Get(),
            "ROOT",
            "Ljava/util/Locale;");
        CheckAndroidJavaException(
            env,
            "Could not resolve Locale.ROOT for invariant collation.");
        if (rootField == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Locale.ROOT for invariant collation.");
        }

        LocalJavaRef<jobject> rootLocale(
            env,
            env->GetStaticObjectField(localeClass.Get(), rootField));
        CheckAndroidJavaException(
            env,
            "Could not read Locale.ROOT for invariant collation.");
        if (!rootLocale)
        {
            throw std::runtime_error(
                "Could not read Locale.ROOT for invariant collation.");
        }

        collatorClass = LocalJavaRef<jclass>(
            env,
            env->FindClass("java/text/Collator"));
        CheckAndroidJavaException(
            env,
            "Could not resolve java.text.Collator.");
        if (!collatorClass)
        {
            throw std::runtime_error(
                "Could not resolve java.text.Collator.");
        }

        const jmethodID getInstance = env->GetStaticMethodID(
            collatorClass.Get(),
            "getInstance",
            "(Ljava/util/Locale;)Ljava/text/Collator;");
        CheckAndroidJavaException(
            env,
            "Could not resolve Collator.getInstance(Locale).");
        if (getInstance == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Collator.getInstance(Locale).");
        }

        LocalJavaRef<jobject> collator(
            env,
            env->CallStaticObjectMethod(
                collatorClass.Get(),
                getInstance,
                rootLocale.Get()));
        CheckAndroidJavaException(
            env,
            "Could not create the Android invariant collator.");
        if (!collator)
        {
            throw std::runtime_error(
                "Could not create the Android invariant collator.");
        }

        const jfieldID secondaryField = env->GetStaticFieldID(
            collatorClass.Get(),
            "SECONDARY",
            "I");
        const jfieldID canonicalField = env->GetStaticFieldID(
            collatorClass.Get(),
            "CANONICAL_DECOMPOSITION",
            "I");
        CheckAndroidJavaException(
            env,
            "Could not resolve Android Collator constants.");
        if (secondaryField == nullptr || canonicalField == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Android Collator constants.");
        }

        const jint secondary = env->GetStaticIntField(
            collatorClass.Get(),
            secondaryField);
        const jint canonical = env->GetStaticIntField(
            collatorClass.Get(),
            canonicalField);
        CheckAndroidJavaException(
            env,
            "Could not read Android Collator constants.");

        const jmethodID setStrength = env->GetMethodID(
            collatorClass.Get(),
            "setStrength",
            "(I)V");
        const jmethodID setDecomposition = env->GetMethodID(
            collatorClass.Get(),
            "setDecomposition",
            "(I)V");
        CheckAndroidJavaException(
            env,
            "Could not resolve Android Collator configuration methods.");
        if (setStrength == nullptr || setDecomposition == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Android Collator configuration methods.");
        }

        env->CallVoidMethod(collator.Get(), setStrength, secondary);
        CheckAndroidJavaException(
            env,
            "Could not configure Android invariant collation strength.");
        env->CallVoidMethod(collator.Get(), setDecomposition, canonical);
        CheckAndroidJavaException(
            env,
            "Could not configure Android invariant collation normalization.");
        return collator;
    }

    [[nodiscard]] LocalJavaRef<jstring> CreateAndroidString(
        JNIEnv* env,
        std::u16string_view value)
    {
        if (value.size()
            > static_cast<std::size_t>(
                std::numeric_limits<jsize>::max()))
        {
            throw std::length_error(
                "String length exceeds the native comparison limit.");
        }

        static constexpr jchar Empty = 0;
        const jchar* chars = value.empty()
            ? &Empty
            : reinterpret_cast<const jchar*>(value.data());
        static_assert(sizeof(jchar) == sizeof(char16_t));

        LocalJavaRef<jstring> result(
            env,
            env->NewString(
                chars,
                static_cast<jsize>(value.size())));
        CheckAndroidJavaException(
            env,
            "Could not create an Android UTF-16 string.");
        if (!result)
        {
            throw std::runtime_error(
                "Could not create an Android UTF-16 string.");
        }
        return result;
    }

    [[nodiscard]] bool InvariantCultureIgnoreCaseEquals(
        std::u16string_view left,
        std::u16string_view right)
    {
        if (left.data() == right.data() && left.size() == right.size())
        {
            return true;
        }

        ScopedAndroidJniEnv scope;
        JNIEnv* env = scope.Get();
        LocalJavaRef<jclass> collatorClass;
        LocalJavaRef<jobject> collator =
            CreateAndroidInvariantCollator(env, collatorClass);
        LocalJavaRef<jstring> leftString =
            CreateAndroidString(env, left);
        LocalJavaRef<jstring> rightString =
            CreateAndroidString(env, right);

        const jmethodID compare = env->GetMethodID(
            collatorClass.Get(),
            "compare",
            "(Ljava/lang/String;Ljava/lang/String;)I");
        CheckAndroidJavaException(
            env,
            "Could not resolve Collator.compare(String, String).");
        if (compare == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Collator.compare(String, String).");
        }

        const jint result = env->CallIntMethod(
            collator.Get(),
            compare,
            leftString.Get(),
            rightString.Get());
        CheckAndroidJavaException(
            env,
            "Android invariant string comparison failed.");
        return result == 0;
    }

    [[nodiscard]] std::int32_t InvariantCultureIgnoreCaseHash(
        std::u16string_view value)
    {
        ScopedAndroidJniEnv scope;
        JNIEnv* env = scope.Get();
        LocalJavaRef<jclass> collatorClass;
        LocalJavaRef<jobject> collator =
            CreateAndroidInvariantCollator(env, collatorClass);
        LocalJavaRef<jstring> stringValue =
            CreateAndroidString(env, value);

        const jmethodID getCollationKey = env->GetMethodID(
            collatorClass.Get(),
            "getCollationKey",
            "(Ljava/lang/String;)Ljava/text/CollationKey;");
        CheckAndroidJavaException(
            env,
            "Could not resolve Collator.getCollationKey(String).");
        if (getCollationKey == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Collator.getCollationKey(String).");
        }

        LocalJavaRef<jobject> key(
            env,
            env->CallObjectMethod(
                collator.Get(),
                getCollationKey,
                stringValue.Get()));
        CheckAndroidJavaException(
            env,
            "Android invariant sort-key generation failed.");
        if (!key)
        {
            throw std::runtime_error(
                "Android invariant sort-key generation failed.");
        }

        LocalJavaRef<jclass> keyClass(
            env,
            env->FindClass("java/text/CollationKey"));
        CheckAndroidJavaException(
            env,
            "Could not resolve java.text.CollationKey.");
        if (!keyClass)
        {
            throw std::runtime_error(
                "Could not resolve java.text.CollationKey.");
        }

        const jmethodID toByteArray = env->GetMethodID(
            keyClass.Get(),
            "toByteArray",
            "()[B");
        CheckAndroidJavaException(
            env,
            "Could not resolve CollationKey.toByteArray().");
        if (toByteArray == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve CollationKey.toByteArray().");
        }

        LocalJavaRef<jbyteArray> bytes(
            env,
            static_cast<jbyteArray>(
                env->CallObjectMethod(key.Get(), toByteArray)));
        CheckAndroidJavaException(
            env,
            "Android invariant sort-key generation failed.");
        if (!bytes)
        {
            throw std::runtime_error(
                "Android invariant sort-key generation failed.");
        }

        const jsize length = env->GetArrayLength(bytes.Get());
        CheckAndroidJavaException(
            env,
            "Could not read the Android invariant sort key.");
        std::vector<std::uint8_t> storage(
            static_cast<std::size_t>(length));
        if (length != 0)
        {
            env->GetByteArrayRegion(
                bytes.Get(),
                0,
                length,
                reinterpret_cast<jbyte*>(storage.data()));
            CheckAndroidJavaException(
                env,
                "Could not read the Android invariant sort key.");
        }
        return HashBytes(storage.data(), storage.size());
    }
#elif defined(__APPLE__)
    struct AppleUCollator;

    class AppleIcuLibrary final
    {
    public:
        AppleIcuLibrary()
            : _handle(dlopen(
                  "/usr/lib/libicucore.A.dylib",
                  RTLD_LAZY | RTLD_LOCAL))
        {
            if (_handle == nullptr)
            {
                throw std::runtime_error(
                    "Unable to open the Apple ICU runtime.");
            }
        }

        ~AppleIcuLibrary()
        {
            dlclose(_handle);
        }

        AppleIcuLibrary(const AppleIcuLibrary&) = delete;
        AppleIcuLibrary& operator=(const AppleIcuLibrary&) = delete;

        template <typename Function>
        [[nodiscard]] Function Load(const char* name) const
        {
            void* symbol = dlsym(_handle, name);
            if (symbol == nullptr)
            {
                throw std::runtime_error(
                    "Unable to resolve an Apple ICU collation symbol.");
            }
            return reinterpret_cast<Function>(symbol);
        }

    private:
        void* _handle = nullptr;
    };

    class InvariantCollator final
    {
    public:
        InvariantCollator()
            : _open(_library.Load<OpenFunction>("ucol_open")),
              _setStrength(
                  _library.Load<SetStrengthFunction>(
                      "ucol_setStrength")),
              _setAttribute(
                  _library.Load<SetAttributeFunction>(
                      "ucol_setAttribute")),
              _close(_library.Load<CloseFunction>("ucol_close")),
              _strcoll(_library.Load<StrcollFunction>("ucol_strcoll")),
              _getSortKey(
                  _library.Load<GetSortKeyFunction>(
                      "ucol_getSortKey"))
        {
            std::int32_t status = 0;
            _collator = _open("root", &status);
            if (status > 0 || _collator == nullptr)
            {
                throw std::runtime_error(
                    "Unable to create the Apple ICU invariant collator.");
            }

            _setStrength(_collator, SecondaryStrength);
            status = 0;
            _setAttribute(
                _collator,
                NormalizationMode,
                AttributeOn,
                &status);
            if (status > 0)
            {
                _close(_collator);
                _collator = nullptr;
                throw std::runtime_error(
                    "Unable to configure the Apple ICU invariant collator.");
            }
        }

        InvariantCollator(const InvariantCollator&) = delete;
        InvariantCollator& operator=(const InvariantCollator&) = delete;

        ~InvariantCollator()
        {
            if (_collator != nullptr)
            {
                _close(_collator);
            }
        }

        [[nodiscard]] std::int32_t Compare(
            std::u16string_view left,
            std::u16string_view right) const
        {
            return _strcoll(
                _collator,
                left.data(),
                static_cast<std::int32_t>(left.size()),
                right.data(),
                static_cast<std::int32_t>(right.size()));
        }

        [[nodiscard]] std::int32_t GetSortKey(
            std::u16string_view value,
            std::uint8_t* result,
            std::int32_t capacity) const
        {
            return _getSortKey(
                _collator,
                value.data(),
                static_cast<std::int32_t>(value.size()),
                result,
                capacity);
        }

    private:
        using OpenFunction =
            AppleUCollator* (*)(const char*, std::int32_t*);
        using SetStrengthFunction =
            void (*)(AppleUCollator*, std::int32_t);
        using SetAttributeFunction =
            void (*)(
                AppleUCollator*,
                std::int32_t,
                std::int32_t,
                std::int32_t*);
        using CloseFunction = void (*)(AppleUCollator*);
        using StrcollFunction =
            std::int32_t (*)(
                const AppleUCollator*,
                const char16_t*,
                std::int32_t,
                const char16_t*,
                std::int32_t);
        using GetSortKeyFunction =
            std::int32_t (*)(
                const AppleUCollator*,
                const char16_t*,
                std::int32_t,
                std::uint8_t*,
                std::int32_t);

        static constexpr std::int32_t SecondaryStrength = 1;
        static constexpr std::int32_t NormalizationMode = 4;
        static constexpr std::int32_t AttributeOn = 17;

        AppleIcuLibrary _library;
        OpenFunction _open;
        SetStrengthFunction _setStrength;
        SetAttributeFunction _setAttribute;
        CloseFunction _close;
        StrcollFunction _strcoll;
        GetSortKeyFunction _getSortKey;
        AppleUCollator* _collator = nullptr;
    };

    [[nodiscard]] const InvariantCollator& GetInvariantCollator()
    {
        static const InvariantCollator collator;
        return collator;
    }

    [[nodiscard]] bool InvariantCultureIgnoreCaseEquals(
        std::u16string_view left,
        std::u16string_view right)
    {
        if (left.data() == right.data() && left.size() == right.size())
        {
            return true;
        }
        if (left.size()
                > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max())
            || right.size()
                > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error(
                "String length exceeds the native comparison limit.");
        }

        return GetInvariantCollator().Compare(left, right) == 0;
    }

    [[nodiscard]] std::int32_t InvariantCultureIgnoreCaseHash(
        std::u16string_view value)
    {
        if (value.size()
            > static_cast<std::size_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error(
                "String length exceeds the native comparison limit.");
        }

        const InvariantCollator& collator = GetInvariantCollator();
        const std::int32_t required =
            collator.GetSortKey(value, nullptr, 0);
        if (required <= 0)
        {
            throw std::runtime_error(
                "Unable to create the Apple ICU invariant sort key.");
        }

        std::vector<std::uint8_t> sortKey(
            static_cast<std::size_t>(required));
        const std::int32_t written = collator.GetSortKey(
            value,
            sortKey.data(),
            required);
        if (written != required)
        {
            throw std::runtime_error(
                "Unable to create the Apple ICU invariant sort key.");
        }
        return HashBytes(sortKey.data(), sortKey.size());
    }
#else
    class InvariantCollator final
    {
    public:
        InvariantCollator()
        {
            UErrorCode status = U_ZERO_ERROR;
            _collator = ucol_open("root", &status);
            if (U_FAILURE(status) || _collator == nullptr)
            {
                throw std::runtime_error(
                    "Unable to create the ICU invariant collator.");
            }

            ucol_setStrength(_collator, UCOL_SECONDARY);
            status = U_ZERO_ERROR;
            ucol_setAttribute(
                _collator,
                UCOL_NORMALIZATION_MODE,
                UCOL_ON,
                &status);
            if (U_FAILURE(status))
            {
                ucol_close(_collator);
                _collator = nullptr;
                throw std::runtime_error(
                    "Unable to configure the ICU invariant collator.");
            }
        }

        InvariantCollator(const InvariantCollator&) = delete;
        InvariantCollator& operator=(const InvariantCollator&) = delete;

        ~InvariantCollator()
        {
            ucol_close(_collator);
        }

        [[nodiscard]] const UCollator* Get() const noexcept
        {
            return _collator;
        }

    private:
        UCollator* _collator = nullptr;
    };

    [[nodiscard]] const InvariantCollator& GetInvariantCollator()
    {
        static const InvariantCollator collator;
        return collator;
    }

    [[nodiscard]] bool InvariantCultureIgnoreCaseEquals(
        std::u16string_view left,
        std::u16string_view right)
    {
        if (left.data() == right.data() && left.size() == right.size())
        {
            return true;
        }
        if (left.size()
                > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max())
            || right.size()
                > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error(
                "String length exceeds the native comparison limit.");
        }

        return ucol_strcoll(
            GetInvariantCollator().Get(),
            reinterpret_cast<const UChar*>(left.data()),
            static_cast<std::int32_t>(left.size()),
            reinterpret_cast<const UChar*>(right.data()),
            static_cast<std::int32_t>(right.size())) == UCOL_EQUAL;
    }

    [[nodiscard]] std::int32_t InvariantCultureIgnoreCaseHash(
        std::u16string_view value)
    {
        if (value.size()
            > static_cast<std::size_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error(
                "String length exceeds the native comparison limit.");
        }

        const UCollator* collator = GetInvariantCollator().Get();
        const std::int32_t required = ucol_getSortKey(
            collator,
            reinterpret_cast<const UChar*>(value.data()),
            static_cast<std::int32_t>(value.size()),
            nullptr,
            0);
        if (required <= 0)
        {
            throw std::runtime_error(
                "Unable to create the ICU invariant sort key.");
        }

        std::vector<std::uint8_t> sortKey(
            static_cast<std::size_t>(required));
        const std::int32_t written = ucol_getSortKey(
            collator,
            reinterpret_cast<const UChar*>(value.data()),
            static_cast<std::int32_t>(value.size()),
            sortKey.data(),
            required);
        if (written != required)
        {
            throw std::runtime_error(
                "Unable to create the ICU invariant sort key.");
        }
        return HashBytes(sortKey.data(), sortKey.size());
    }
#endif
}

namespace NCSFCommon
{
    std::shared_ptr<const std::u16string> TagList::String::Create(std::u16string value)
    {
        return std::make_shared<const std::u16string>(std::move(value));
    }

    TagList::String::String(std::nullptr_t) noexcept
    {
    }

    TagList::String::String(std::u16string value)
        : _value(Create(std::move(value)))
    {
    }

    TagList::String::String(std::u16string_view value)
        : _value(Create(std::u16string(value)))
    {
    }

    TagList::String& TagList::String::operator=(std::nullptr_t) noexcept
    {
        _value.reset();
        return *this;
    }

    TagList::String& TagList::String::operator=(std::u16string value)
    {
        _value = Create(std::move(value));
        return *this;
    }

    TagList::String& TagList::String::operator=(std::u16string_view value)
    {
        _value = Create(std::u16string(value));
        return *this;
    }

    bool TagList::String::IsNull() const noexcept
    {
        return _value == nullptr;
    }

    std::u16string_view TagList::String::View() const noexcept
    {
        if (_value == nullptr)
        {
            return {};
        }
        return *_value;
    }

    const char16_t* TagList::String::data() const noexcept
    {
        return View().data();
    }

    std::size_t TagList::String::size() const noexcept
    {
        return View().size();
    }

    bool TagList::String::empty() const noexcept
    {
        return View().empty();
    }

    TagList::String::const_iterator TagList::String::begin() const noexcept
    {
        return View().begin();
    }

    TagList::String::const_iterator TagList::String::end() const noexcept
    {
        return View().end();
    }

    TagList::String::operator std::u16string_view() const noexcept
    {
        return View();
    }

    TagList::String::operator std::u16string() const
    {
        return std::u16string(View());
    }

    bool operator==(const TagList::String& left, const TagList::String& right) noexcept
    {
        if (left._value == right._value)
        {
            return true;
        }
        if (left._value == nullptr || right._value == nullptr)
        {
            return false;
        }
        return *left._value == *right._value;
    }

    TagList::KeyComparer::KeyComparer()
    {
#if !defined(_WIN32) && !defined(__ANDROID__)
        static_cast<void>(GetInvariantCollator());
#endif
    }

    bool TagList::KeyComparer::Equals(const String& left, const String& right) const
    {
        if (left._value == right._value)
        {
            return true;
        }
        if (left._value == nullptr || right._value == nullptr)
        {
            return false;
        }
        return InvariantCultureIgnoreCaseEquals(left.View(), right.View());
    }

    std::int32_t TagList::KeyComparer::GetHashCode(const String& value) const
    {
        if (value.IsNull())
        {
            ThrowArgumentNullObject();
        }
        return InvariantCultureIgnoreCaseHash(value.View());
    }

    TagList::ConstIterator::ConstIterator(std::shared_ptr<const State> state, std::size_t index) noexcept
        : _state(std::move(state)),
          _index(index),
          _version(_state == nullptr ? 0 : _state->Version)
    {
    }

    void TagList::ConstIterator::VerifyVersion() const
    {
        if (_state != nullptr && _version != _state->Version)
        {
            ThrowEnumerationModified();
        }
    }

    TagList::ConstIterator::reference TagList::ConstIterator::operator*() const
    {
        VerifyVersion();
        if (_state == nullptr || _index >= _state->Items.size())
        {
            throw std::out_of_range("Enumeration has either not started or has already finished.");
        }
        return _state->Items[_index];
    }

    TagList::ConstIterator::pointer TagList::ConstIterator::operator->() const
    {
        return &operator*();
    }

    TagList::ConstIterator& TagList::ConstIterator::operator++()
    {
        VerifyVersion();
        if (_state != nullptr && _index < _state->Items.size())
        {
            ++_index;
        }
        return *this;
    }

    TagList::ConstIterator TagList::ConstIterator::operator++(int)
    {
        ConstIterator copy = *this;
        ++(*this);
        return copy;
    }

    bool operator==(const TagList::ConstIterator& left, const TagList::ConstIterator& right)
    {
        left.VerifyVersion();
        right.VerifyVersion();
        return left._state == right._state && left._index == right._index;
    }

    TagList::Enumerator::Enumerator(std::shared_ptr<const State> state) noexcept
        : _state(std::move(state)),
          _version(_state == nullptr ? 0 : _state->Version)
    {
    }

    void TagList::Enumerator::VerifyVersion() const
    {
        if (_state != nullptr && _version != _state->Version)
        {
            ThrowEnumerationModified();
        }
    }

    bool TagList::Enumerator::MoveNext()
    {
        VerifyVersion();
        if (_state != nullptr && _nextIndex < _state->Items.size())
        {
            _current = _state->Items[_nextIndex];
            ++_nextIndex;
            return true;
        }

        _current = Item{};
        if (_state != nullptr)
        {
            _nextIndex = _state->Items.size() + 1;
        }
        return false;
    }

    TagList::Item TagList::Enumerator::Current() const
    {
        return _current;
    }

    void TagList::Enumerator::Reset()
    {
        VerifyVersion();
        _nextIndex = 0;
        _current = Item{};
    }

    const TagList::KeyComparer& TagList::StaticComparer()
    {
        static const KeyComparer comparer;
        return comparer;
    }

    TagList::TagList()
        : _state((static_cast<void>(StaticComparer()), std::make_shared<State>()))
    {
    }

    TagList::TagList(TagList&& other) noexcept
        : _state(other._state)
    {
    }

    TagList& TagList::operator=(TagList&& other) noexcept
    {
        _state = other._state;
        return *this;
    }

    std::int32_t TagList::Count() const noexcept
    {
        return static_cast<std::int32_t>(_state->Items.size());
    }

    bool TagList::IsReadOnly() const noexcept
    {
        return false;
    }

    const TagList::KeyComparer& TagList::Comparer() const
    {
        return StaticComparer();
    }

    std::size_t TagList::CheckedIndex(std::int32_t index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= _state->Items.size())
        {
            ThrowIndexOutOfRange();
        }
        return static_cast<std::size_t>(index);
    }

    std::size_t TagList::CheckedInsertIndex(std::int32_t index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) > _state->Items.size())
        {
            ThrowInsertIndexOutOfRange();
        }
        return static_cast<std::size_t>(index);
    }

    TagList::Item TagList::operator[](std::int32_t index) const
    {
        return _state->Items[CheckedIndex(index)];
    }

    TagList::Item TagList::operator[](const String& key) const
    {
        Item item;
        if (!TryGetValue(key, item))
        {
            ThrowKeyNotFound();
        }
        return item;
    }

    void TagList::Set(std::int32_t index, Item item)
    {
        const std::size_t checkedIndex = CheckedIndex(index);
        SetItem(static_cast<std::int32_t>(checkedIndex), std::move(item));
    }

    void TagList::Add(Item item)
    {
        const std::int32_t index = Count();
        InsertItem(index, std::move(item));
    }

    void TagList::Insert(std::int32_t index, Item item)
    {
        const std::size_t checkedIndex = CheckedInsertIndex(index);
        InsertItem(static_cast<std::int32_t>(checkedIndex), std::move(item));
    }

    bool TagList::Remove(const Item& item)
    {
        const std::int32_t index = IndexOf(item);
        if (index < 0)
        {
            return false;
        }
        RemoveItem(index);
        return true;
    }

    bool TagList::Remove(const String& key)
    {
        if (key.IsNull())
        {
            ThrowArgumentNullKey();
        }

        if (_state->DictionaryCreated)
        {
            const std::size_t dictionaryIndex = FindDictionaryIndex(key);
            if (dictionaryIndex == MissingIndex)
            {
                return false;
            }
            const Item item = _state->Dictionary[dictionaryIndex].Value;
            return Remove(item);
        }

        for (std::size_t index = 0; index < _state->Items.size(); ++index)
        {
            if (Comparer().Equals(GetKeyForItem(_state->Items[index]), key))
            {
                RemoveItem(static_cast<std::int32_t>(index));
                return true;
            }
        }
        return false;
    }

    void TagList::RemoveAt(std::int32_t index)
    {
        const std::size_t checkedIndex = CheckedIndex(index);
        RemoveItem(static_cast<std::int32_t>(checkedIndex));
    }

    void TagList::Clear()
    {
        ClearItems();
    }

    bool TagList::Contains(const Item& item) const noexcept
    {
        return IndexOf(item) >= 0;
    }

    bool TagList::Contains(const String& key) const
    {
        if (key.IsNull())
        {
            ThrowArgumentNullKey();
        }

        if (_state->DictionaryCreated)
        {
            return FindDictionaryIndex(key) != MissingIndex;
        }

        for (const Item& item : _state->Items)
        {
            if (Comparer().Equals(GetKeyForItem(item), key))
            {
                return true;
            }
        }
        return false;
    }

    bool TagList::TryGetValue(const String& key, Item& item) const
    {
        if (key.IsNull())
        {
            ThrowArgumentNullKey();
        }

        if (_state->DictionaryCreated)
        {
            const std::size_t index = FindDictionaryIndex(key);
            if (index == MissingIndex)
            {
                item = Item{};
                return false;
            }
            item = _state->Dictionary[index].Value;
            return true;
        }

        for (const Item& itemInItems : _state->Items)
        {
            const String keyInItems = GetKeyForItem(itemInItems);
            if (!keyInItems.IsNull() && Comparer().Equals(key, keyInItems))
            {
                item = itemInItems;
                return true;
            }
        }

        item = Item{};
        return false;
    }

    std::int32_t TagList::IndexOf(const Item& item) const noexcept
    {
        const auto iterator = std::find(_state->Items.begin(), _state->Items.end(), item);
        if (iterator == _state->Items.end())
        {
            return -1;
        }
        return static_cast<std::int32_t>(std::distance(_state->Items.begin(), iterator));
    }

    void TagList::CopyTo(std::span<Item> array, std::int32_t arrayIndex) const
    {
        if (arrayIndex < 0)
        {
            throw std::out_of_range("Number was less than the array's lower bound in the first dimension.");
        }

        const std::size_t index = static_cast<std::size_t>(arrayIndex);
        if (index > array.size() || _state->Items.size() > array.size() - index)
        {
            throw std::invalid_argument("Destination array was not long enough.");
        }
        std::copy(_state->Items.begin(), _state->Items.end(), array.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void TagList::CopyTo(std::vector<Item>& array, std::int32_t arrayIndex) const
    {
        CopyTo(std::span<Item>(array.data(), array.size()), arrayIndex);
    }

    TagList::ConstIterator TagList::begin() const noexcept
    {
        return ConstIterator(_state, 0);
    }

    TagList::ConstIterator TagList::end() const noexcept
    {
        return ConstIterator(_state, _state->Items.size());
    }

    TagList::Enumerator TagList::GetEnumerator() const noexcept
    {
        return Enumerator(_state);
    }

    void TagList::AddOrReplace(Item item)
    {
        std::int32_t index = -1;
        Item existingItem;
        if (TryGetValue(item.Name, existingItem))
        {
            index = IndexOf(existingItem);
            static_cast<void>(Remove(existingItem));
        }

        if (index == -1)
        {
            Add(std::move(item));
        }
        else
        {
            Insert(index, std::move(item));
        }
    }

    TagList TagList::Clone() const
    {
        TagList clone;
        for (const Item& item : *this)
        {
            clone.Add(item);
        }
        return clone;
    }

    TagList::String TagList::GetKeyForItem(const Item& item) const
    {
        return item.Name;
    }

    void TagList::ChangeItemKey(const Item& item, const String& newKey)
    {
        if (!ContainsItem(item))
        {
            throw std::invalid_argument("The specified item does not exist in this KeyedCollection.");
        }

        const String oldKey = GetKeyForItem(item);
        if (!Comparer().Equals(oldKey, newKey))
        {
            if (!newKey.IsNull())
            {
                AddKey(newKey, item);
            }
            if (!oldKey.IsNull())
            {
                RemoveKey(oldKey);
            }
        }
    }

    void TagList::ClearItems()
    {
        _state->Items.clear();
        IncrementVersion();
        if (_state->DictionaryCreated)
        {
            _state->Dictionary.clear();
        }
        _state->KeyCount = 0;
    }

    void TagList::InsertItem(std::int32_t index, Item item)
    {
        const String key = GetKeyForItem(item);
        if (!key.IsNull())
        {
            AddKey(key, item);
        }

        const std::size_t itemIndex = CheckedInsertIndex(index);
        if (_state->Items.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("Collection was too large.");
        }
        _state->Items.insert(
            _state->Items.begin() + static_cast<std::ptrdiff_t>(itemIndex),
            std::move(item));
        IncrementVersion();
    }

    void TagList::RemoveItem(std::int32_t index)
    {
        const std::size_t itemIndex = CheckedIndex(index);
        const String key = GetKeyForItem(_state->Items[itemIndex]);
        if (!key.IsNull())
        {
            RemoveKey(key);
        }
        _state->Items.erase(_state->Items.begin() + static_cast<std::ptrdiff_t>(itemIndex));
        IncrementVersion();
    }

    void TagList::SetItem(std::int32_t index, Item item)
    {
        const String newKey = GetKeyForItem(item);
        const std::size_t itemIndex = CheckedIndex(index);
        const String oldKey = GetKeyForItem(_state->Items[itemIndex]);

        if (Comparer().Equals(oldKey, newKey))
        {
            if (!newKey.IsNull() && _state->DictionaryCreated)
            {
                const std::size_t dictionaryIndex = FindDictionaryIndex(newKey);
                if (dictionaryIndex == MissingIndex)
                {
                    _state->Dictionary.push_back(DictionaryEntry{ newKey, item });
                }
                else
                {
                    _state->Dictionary[dictionaryIndex].Value = item;
                }
            }
        }
        else
        {
            if (!newKey.IsNull())
            {
                AddKey(newKey, item);
            }
            if (!oldKey.IsNull())
            {
                RemoveKey(oldKey);
            }
        }

        _state->Items[itemIndex] = std::move(item);
        IncrementVersion();
    }

    std::size_t TagList::FindKeyIndex(const String& key) const
    {
        for (std::size_t index = 0; index < _state->Items.size(); ++index)
        {
            if (Comparer().Equals(GetKeyForItem(_state->Items[index]), key))
            {
                return index;
            }
        }
        return MissingIndex;
    }

    std::size_t TagList::FindDictionaryIndex(const String& key) const
    {
        for (std::size_t index = 0; index < _state->Dictionary.size(); ++index)
        {
            if (Comparer().Equals(key, _state->Dictionary[index].Key))
            {
                return index;
            }
        }
        return MissingIndex;
    }

    bool TagList::ContainsItem(const Item& item) const
    {
        if (!_state->DictionaryCreated)
        {
            return Contains(item);
        }

        const String key = GetKeyForItem(item);
        if (key.IsNull())
        {
            return Contains(item);
        }

        const std::size_t index = FindDictionaryIndex(key);
        return index != MissingIndex && _state->Dictionary[index].Value == item;
    }

    void TagList::EnsureUniqueKey(const String& key) const
    {
        if (_state->DictionaryCreated)
        {
            if (FindDictionaryIndex(key) != MissingIndex)
            {
                ThrowDuplicateKey();
            }
            return;
        }

        if (FindKeyIndex(key) != MissingIndex)
        {
            ThrowDuplicateKey();
        }
    }

    void TagList::AddKey(const String& key, const Item& item)
    {
        if (_state->DictionaryCreated)
        {
            EnsureUniqueKey(key);
            _state->Dictionary.push_back(DictionaryEntry{ key, item });
            return;
        }

        if (_state->KeyCount == DictionaryCreationThreshold)
        {
            CreateDictionary();
            EnsureUniqueKey(key);
            _state->Dictionary.push_back(DictionaryEntry{ key, item });
            return;
        }

        EnsureUniqueKey(key);
        ++_state->KeyCount;
    }

    void TagList::RemoveKey(const String& key)
    {
        if (_state->DictionaryCreated)
        {
            const std::size_t index = FindDictionaryIndex(key);
            if (index != MissingIndex)
            {
                _state->Dictionary.erase(_state->Dictionary.begin() + static_cast<std::ptrdiff_t>(index));
            }
        }
        else
        {
            --_state->KeyCount;
        }
    }

    void TagList::CreateDictionary()
    {
        _state->Dictionary.clear();
        _state->DictionaryCreated = true;

        for (const Item& item : _state->Items)
        {
            const String key = GetKeyForItem(item);
            if (key.IsNull())
            {
                continue;
            }
            EnsureUniqueKey(key);
            _state->Dictionary.push_back(DictionaryEntry{ key, item });
        }
    }

    void TagList::IncrementVersion() noexcept
    {
        ++_state->Version;
    }
}

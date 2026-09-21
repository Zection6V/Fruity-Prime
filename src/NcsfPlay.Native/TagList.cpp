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


#if !defined(_WIN32)
    static constexpr char16_t HalfFullLowerChars[] = {
        0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
            0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
            0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d,
            0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005d,
            0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c,
            0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b,
            0x007c, 0x007d, 0x007e, 0x00a2, 0x00a3, 0x00ac, 0x00af, 0x00a6, 0x00a5, 0x20a9,
            0x3002, 0x300c, 0x300d, 0x3001, 0x30fb, 0x30f2, 0x30a1, 0x30a3, 0x30a5, 0x30a7, 0x30a9, 0x30e3, 0x30e5, 0x30e7, 0x30c3,
            0x30a2, 0x30a4, 0x30a6, 0x30a8, 0x30aa, 0x30ab, 0x30ad, 0x30af, 0x30b1, 0x30b3, 0x30b5, 0x30b7, 0x30b9, 0x30bb, 0x30bd,
            0x30bf, 0x30c1, 0x30c4, 0x30c6, 0x30c8, 0x30ca, 0x30cb, 0x30cc, 0x30cd, 0x30ce, 0x30cf, 0x30d2, 0x30d5, 0x30d8, 0x30db,
            0x30de, 0x30df, 0x30e0, 0x30e1, 0x30e2, 0x30e4, 0x30e6, 0x30e8, 0x30e9, 0x30ea, 0x30eb, 0x30ec, 0x30ed, 0x30ef, 0x30f3,
            0x3164, 0x3131, 0x3132, 0x3133, 0x3134, 0x3135, 0x3136, 0x3137, 0x3138, 0x3139, 0x313a, 0x313b, 0x313c, 0x313d, 0x313e,
            0x313f, 0x3140, 0x3141, 0x3142, 0x3143, 0x3144, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314a, 0x314b, 0x314c, 0x314d,
            0x314e, 0x314f, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155, 0x3156, 0x3157, 0x3158, 0x3159, 0x315a, 0x315b, 0x315c,
            0x315d, 0x315e, 0x315f, 0x3160, 0x3161, 0x3162, 0x3163
    };

    static constexpr char16_t HalfFullHigherChars[] = {
        0xff01, 0xff02, 0xff03, 0xff04, 0xff05, 0xff06, 0xff07, 0xff08, 0xff09, 0xff0a, 0xff0b, 0xff0c, 0xff0d, 0xff0e, 0xff0f,
            0xff10, 0xff11, 0xff12, 0xff13, 0xff14, 0xff15, 0xff16, 0xff17, 0xff18, 0xff19, 0xff1a, 0xff1b, 0xff1c, 0xff1d, 0xff1e,
            0xff1f, 0xff20, 0xff21, 0xff22, 0xff23, 0xff24, 0xff25, 0xff26, 0xff27, 0xff28, 0xff29, 0xff2a, 0xff2b, 0xff2c, 0xff2d,
            0xff2e, 0xff2f, 0xff30, 0xff31, 0xff32, 0xff33, 0xff34, 0xff35, 0xff36, 0xff37, 0xff38, 0xff39, 0xff3a, 0xff3b, 0xff3d,
            0xff3e, 0xff3f, 0xff40, 0xff41, 0xff42, 0xff43, 0xff44, 0xff45, 0xff46, 0xff47, 0xff48, 0xff49, 0xff4a, 0xff4b, 0xff4c,
            0xff4d, 0xff4e, 0xff4f, 0xff50, 0xff51, 0xff52, 0xff53, 0xff54, 0xff55, 0xff56, 0xff57, 0xff58, 0xff59, 0xff5a, 0xff5b,
            0xff5c, 0xff5d, 0xff5e, 0xffe0, 0xffe1, 0xffe2, 0xffe3, 0xffe4, 0xffe5, 0xffe6,
            0xff61, 0xff62, 0xff63, 0xff64, 0xff65, 0xff66, 0xff67, 0xff68, 0xff69, 0xff6a, 0xff6b, 0xff6c, 0xff6d, 0xff6e, 0xff6f,
            0xff71, 0xff72, 0xff73, 0xff74, 0xff75, 0xff76, 0xff77, 0xff78, 0xff79, 0xff7a, 0xff7b, 0xff7c, 0xff7d, 0xff7e, 0xff7f,
            0xff80, 0xff81, 0xff82, 0xff83, 0xff84, 0xff85, 0xff86, 0xff87, 0xff88, 0xff89, 0xff8a, 0xff8b, 0xff8c, 0xff8d, 0xff8e,
            0xff8f, 0xff90, 0xff91, 0xff92, 0xff93, 0xff94, 0xff95, 0xff96, 0xff97, 0xff98, 0xff99, 0xff9a, 0xff9b, 0xff9c, 0xff9d,
            0xffa0, 0xffa1, 0xffa2, 0xffa3, 0xffa4, 0xffa5, 0xffa6, 0xffa7, 0xffa8, 0xffa9, 0xffaa, 0xffab, 0xffac, 0xffad, 0xffae,
            0xffaf, 0xffb0, 0xffb1, 0xffb2, 0xffb3, 0xffb4, 0xffb5, 0xffb6, 0xffb7, 0xffb8, 0xffb9, 0xffba, 0xffbb, 0xffbc, 0xffbd,
            0xffbe, 0xffc2, 0xffc3, 0xffc4, 0xffc5, 0xffc6, 0xffc7, 0xffca, 0xffcb, 0xffcc, 0xffcd, 0xffce, 0xffcf, 0xffd2, 0xffd3,
            0xffd4, 0xffd5, 0xffd6, 0xffd7, 0xffda, 0xffdb, 0xffdc
    };

    static constexpr char16_t HiraganaWithoutVoicedSoundMarkChars[] = {
        0x3041, 0x3042, 0x3043, 0x3044, 0x3045, 0x3046, 0x3047, 0x3048, 0x3049, 0x304A, 0x304B, 0x304D, 0x304F, 0x3051, 0x3053,
            0x3055, 0x3057, 0x3059, 0x305B, 0x305D, 0x305F, 0x3061, 0x3063, 0x3064, 0x3066, 0x3068, 0x306A, 0x306B, 0x306C, 0x306D,
            0x306E, 0x306F, 0x3072, 0x3075, 0x3078, 0x307B, 0x307E, 0x307F, 0x3080, 0x3081, 0x3082, 0x3083, 0x3084, 0x3085, 0x3086,
            0x3087, 0x3088, 0x3089, 0x308A, 0x308B, 0x308C, 0x308D, 0x308E, 0x308F, 0x3090, 0x3091, 0x3092, 0x3093, 0x3095, 0x3096, 0x309D,
    };

    [[nodiscard]] constexpr bool NeedsIcuRuleEscape(char16_t character) noexcept
    {
        return (character >= 0x21 && character <= 0x2F)
            || (character >= 0x3A && character <= 0x40)
            || (character >= 0x5B && character <= 0x60)
            || (character >= 0x7B && character <= 0x7E);
    }

    void AppendDotNetInvariantIgnoreCaseRules(std::u16string& rules)
    {
        static constexpr char16_t HiraganaToKatakanaOffset = 0x30A1 - 0x3041;

        for (char16_t hiragana : HiraganaWithoutVoicedSoundMarkChars)
        {
            rules.push_back(u'&');
            rules.push_back(hiragana);
            rules.push_back(u'<');
            rules.push_back(static_cast<char16_t>(
                hiragana + HiraganaToKatakanaOffset));
        }

        static_assert(
            std::size(HalfFullLowerChars) == std::size(HalfFullHigherChars));
        for (std::size_t index = 0;
             index < std::size(HalfFullLowerChars);
             ++index)
        {
            const char16_t lower = HalfFullLowerChars[index];
            rules.push_back(u'&');
            if (NeedsIcuRuleEscape(lower))
            {
                rules.push_back(u'\\');
            }
            rules.push_back(lower);
            rules.push_back(u'<');
            rules.push_back(HalfFullHigherChars[index]);
        }

        static constexpr char16_t UpperCaseToLowerCaseOffset = 0xFF41 - 0xFF21;
        for (char16_t upper = 0xFF21; upper <= 0xFF3A; ++upper)
        {
            rules.push_back(u'&');
            rules.push_back(static_cast<char16_t>(
                upper + UpperCaseToLowerCaseOffset));
            rules.push_back(u'=');
            rules.push_back(upper);
        }

        rules.append(u"&a=a");
    }
#endif

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
                        &_env,
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
            env->FindClass("android/icu/util/ULocale"));
        CheckAndroidJavaException(
            env,
            "Could not resolve android.icu.util.ULocale for invariant collation.");
        if (!localeClass)
        {
            throw std::runtime_error(
                "Could not resolve android.icu.util.ULocale for invariant collation.");
        }

        const jfieldID rootField = env->GetStaticFieldID(
            localeClass.Get(),
            "ROOT",
            "Landroid/icu/util/ULocale;");
        CheckAndroidJavaException(
            env,
            "Could not resolve ULocale.ROOT for invariant collation.");
        if (rootField == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve ULocale.ROOT for invariant collation.");
        }

        LocalJavaRef<jobject> rootLocale(
            env,
            env->GetStaticObjectField(localeClass.Get(), rootField));
        CheckAndroidJavaException(
            env,
            "Could not read ULocale.ROOT for invariant collation.");
        if (!rootLocale)
        {
            throw std::runtime_error(
                "Could not read ULocale.ROOT for invariant collation.");
        }

        collatorClass = LocalJavaRef<jclass>(
            env,
            env->FindClass("android/icu/text/Collator"));
        CheckAndroidJavaException(
            env,
            "Could not resolve android.icu.text.Collator.");
        if (!collatorClass)
        {
            throw std::runtime_error(
                "Could not resolve android.icu.text.Collator.");
        }

        const jmethodID getInstance = env->GetStaticMethodID(
            collatorClass.Get(),
            "getInstance",
            "(Landroid/icu/util/ULocale;)Landroid/icu/text/Collator;");
        CheckAndroidJavaException(
            env,
            "Could not resolve Collator.getInstance(ULocale).");
        if (getInstance == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Collator.getInstance(ULocale).");
        }

        LocalJavaRef<jobject> baseCollator(
            env,
            env->CallStaticObjectMethod(
                collatorClass.Get(),
                getInstance,
                rootLocale.Get()));
        CheckAndroidJavaException(
            env,
            "Could not create the Android ICU root collator.");
        if (!baseCollator)
        {
            throw std::runtime_error(
                "Could not create the Android ICU root collator.");
        }

        LocalJavaRef<jclass> ruleBasedClass(
            env,
            env->FindClass("android/icu/text/RuleBasedCollator"));
        CheckAndroidJavaException(
            env,
            "Could not resolve android.icu.text.RuleBasedCollator.");
        if (!ruleBasedClass)
        {
            throw std::runtime_error(
                "Could not resolve android.icu.text.RuleBasedCollator.");
        }

        if (env->IsInstanceOf(baseCollator.Get(), ruleBasedClass.Get()) != JNI_TRUE)
        {
            throw std::runtime_error(
                "Android ICU root collator is not rule based.");
        }

        const jmethodID getRules = env->GetMethodID(
            ruleBasedClass.Get(),
            "getRules",
            "()Ljava/lang/String;");
        CheckAndroidJavaException(
            env,
            "Could not resolve RuleBasedCollator.getRules().");
        if (getRules == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve RuleBasedCollator.getRules().");
        }

        LocalJavaRef<jstring> baseRules(
            env,
            static_cast<jstring>(
                env->CallObjectMethod(baseCollator.Get(), getRules)));
        CheckAndroidJavaException(
            env,
            "Could not read the Android ICU root collation rules.");
        if (!baseRules)
        {
            throw std::runtime_error(
                "Could not read the Android ICU root collation rules.");
        }

        const jsize baseRuleLength = env->GetStringLength(baseRules.Get());
        CheckAndroidJavaException(
            env,
            "Could not read the Android ICU root collation rule length.");

        const jchar* baseRuleChars =
            env->GetStringChars(baseRules.Get(), nullptr);
        CheckAndroidJavaException(
            env,
            "Could not read the Android ICU root collation rules.");
        if (baseRuleChars == nullptr && baseRuleLength != 0)
        {
            throw std::runtime_error(
                "Could not read the Android ICU root collation rules.");
        }

        std::u16string rules;
        if (baseRuleLength != 0)
        {
            static_assert(sizeof(jchar) == sizeof(char16_t));
            rules.assign(
                reinterpret_cast<const char16_t*>(baseRuleChars),
                static_cast<std::size_t>(baseRuleLength));
        }
        if (baseRuleChars != nullptr)
        {
            env->ReleaseStringChars(baseRules.Get(), baseRuleChars);
        }

        AppendDotNetInvariantIgnoreCaseRules(rules);
        if (rules.size()
            > static_cast<std::size_t>(
                std::numeric_limits<jsize>::max()))
        {
            throw std::length_error(
                "Invariant collation rule length exceeds the Android JNI limit.");
        }

        static constexpr jchar Empty = 0;
        const jchar* ruleChars = rules.empty()
            ? &Empty
            : reinterpret_cast<const jchar*>(rules.data());
        LocalJavaRef<jstring> ruleString(
            env,
            env->NewString(
                ruleChars,
                static_cast<jsize>(rules.size())));
        CheckAndroidJavaException(
            env,
            "Could not create the Android ICU invariant collation rules.");
        if (!ruleString)
        {
            throw std::runtime_error(
                "Could not create the Android ICU invariant collation rules.");
        }

        const jmethodID constructor = env->GetMethodID(
            ruleBasedClass.Get(),
            "<init>",
            "(Ljava/lang/String;)V");
        CheckAndroidJavaException(
            env,
            "Could not resolve RuleBasedCollator(String).");
        if (constructor == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve RuleBasedCollator(String).");
        }

        LocalJavaRef<jobject> collator(
            env,
            env->NewObject(
                ruleBasedClass.Get(),
                constructor,
                ruleString.Get()));
        CheckAndroidJavaException(
            env,
            "Could not create the Android ICU invariant collator.");
        if (!collator)
        {
            throw std::runtime_error(
                "Could not create the Android ICU invariant collator.");
        }

        const jfieldID secondaryField = env->GetStaticFieldID(
            collatorClass.Get(),
            "SECONDARY",
            "I");
        CheckAndroidJavaException(
            env,
            "Could not resolve Android ICU Collator.SECONDARY.");
        if (secondaryField == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Android ICU Collator.SECONDARY.");
        }

        const jint secondary =
            env->GetStaticIntField(collatorClass.Get(), secondaryField);
        CheckAndroidJavaException(
            env,
            "Could not read Android ICU Collator.SECONDARY.");

        const jmethodID setStrength = env->GetMethodID(
            collatorClass.Get(),
            "setStrength",
            "(I)V");
        CheckAndroidJavaException(
            env,
            "Could not resolve Android ICU Collator.setStrength(int).");
        if (setStrength == nullptr)
        {
            throw std::runtime_error(
                "Could not resolve Android ICU Collator.setStrength(int).");
        }

        env->CallVoidMethod(collator.Get(), setStrength, secondary);
        CheckAndroidJavaException(
            env,
            "Could not configure Android ICU invariant collation strength.");
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
            "(Ljava/lang/String;)Landroid/icu/text/CollationKey;");
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
            env->FindClass("android/icu/text/CollationKey"));
        CheckAndroidJavaException(
            env,
            "Could not resolve android.icu.text.CollationKey.");
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
              _openRules(
                  _library.Load<OpenRulesFunction>("ucol_openRules")),
              _getRules(
                  _library.Load<GetRulesFunction>("ucol_getRules")),
              _close(_library.Load<CloseFunction>("ucol_close")),
              _strcoll(_library.Load<StrcollFunction>("ucol_strcoll")),
              _getSortKey(
                  _library.Load<GetSortKeyFunction>("ucol_getSortKey"))
        {
            std::int32_t status = 0;
            AppleUCollator* base = _open("root", &status);
            if (status > 0 || base == nullptr)
            {
                throw std::runtime_error(
                    "Unable to create the Apple ICU root collator.");
            }

            std::int32_t baseRuleLength = 0;
            const char16_t* baseRules =
                _getRules(base, &baseRuleLength);
            std::u16string rules;
            if (baseRules != nullptr && baseRuleLength > 0)
            {
                rules.assign(
                    baseRules,
                    baseRules + baseRuleLength);
            }
            AppendDotNetInvariantIgnoreCaseRules(rules);

            status = 0;
            _collator = _openRules(
                rules.data(),
                static_cast<std::int32_t>(rules.size()),
                DefaultNormalization,
                SecondaryStrength,
                nullptr,
                &status);
            _close(base);
            if (status > 0 || _collator == nullptr)
            {
                throw std::runtime_error(
                    "Unable to create the Apple ICU invariant ignore-case collator.");
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
        using OpenRulesFunction =
            AppleUCollator* (*)(
                const char16_t*,
                std::int32_t,
                std::int32_t,
                std::int32_t,
                void*,
                std::int32_t*);
        using GetRulesFunction =
            const char16_t* (*)(const AppleUCollator*, std::int32_t*);
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

        static constexpr std::int32_t DefaultNormalization = -1;
        static constexpr std::int32_t SecondaryStrength = 1;

        AppleIcuLibrary _library;
        OpenFunction _open;
        OpenRulesFunction _openRules;
        GetRulesFunction _getRules;
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
            UCollator* base = ucol_open("root", &status);
            if (U_FAILURE(status) || base == nullptr)
            {
                throw std::runtime_error(
                    "Unable to create the ICU root collator.");
            }

            std::int32_t baseRuleLength = 0;
            const UChar* baseRules =
                ucol_getRules(base, &baseRuleLength);
            static_assert(sizeof(UChar) == sizeof(char16_t));
            std::u16string rules;
            if (baseRules != nullptr && baseRuleLength > 0)
            {
                rules.assign(
                    reinterpret_cast<const char16_t*>(baseRules),
                    reinterpret_cast<const char16_t*>(baseRules)
                        + baseRuleLength);
            }
            AppendDotNetInvariantIgnoreCaseRules(rules);

            status = U_ZERO_ERROR;
            _collator = ucol_openRules(
                reinterpret_cast<const UChar*>(rules.data()),
                static_cast<std::int32_t>(rules.size()),
                UCOL_DEFAULT,
                UCOL_SECONDARY,
                nullptr,
                &status);
            ucol_close(base);
            if (U_FAILURE(status) || _collator == nullptr)
            {
                throw std::runtime_error(
                    "Unable to create the ICU invariant ignore-case collator.");
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

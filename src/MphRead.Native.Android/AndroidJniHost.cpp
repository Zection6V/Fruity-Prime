#include "MainActivity.hpp"

#include "AndroidMatch.hpp"
#include "AndroidUiSurface.hpp"
#include "ApkInstaller.hpp"
#include "GameView.hpp"
#include "PreviewRun.hpp"
#include "TouchOverlayView.hpp"

#include "../MphRead.Native/Formats/Types.hpp"
#include "../MphRead.Native/Renderer.hpp"
#include "../MphRead.Native/NativeRuntime/System/Exceptions.hpp"

#include <android/bitmap.h>
#include <android/input.h>
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    template <typename T>
    class LocalRef final
    {
    public:
        LocalRef() = default;
        LocalRef(JNIEnv* env, T value) noexcept : _env(env), _value(value) {}
        ~LocalRef()
        {
            if (_env != nullptr && _value != nullptr)
            {
                _env->DeleteLocalRef(_value);
            }
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
                if (_env != nullptr && _value != nullptr)
                {
                    _env->DeleteLocalRef(_value);
                }
                _env = other._env;
                _value = other._value;
                other._env = nullptr;
                other._value = nullptr;
            }
            return *this;
        }

        [[nodiscard]] T Get() const noexcept { return _value; }
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return _value != nullptr;
        }

    private:
        JNIEnv* _env = nullptr;
        T _value = nullptr;
    };

    class ScopedEnv final
    {
    public:
        explicit ScopedEnv(JavaVM* vm) : _vm(vm)
        {
            if (_vm == nullptr)
            {
                throw std::runtime_error("Android Java VM is unavailable");
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
                        "could not attach to the Android Java VM"
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

        ~ScopedEnv()
        {
            if (_attached)
            {
                _vm->DetachCurrentThread();
            }
        }

        ScopedEnv(const ScopedEnv&) = delete;
        ScopedEnv& operator=(const ScopedEnv&) = delete;

        [[nodiscard]] JNIEnv* Get() const noexcept { return _env; }

    private:
        JavaVM* _vm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    [[nodiscard]] std::string JavaString(JNIEnv* env, jstring value)
    {
        if (value == nullptr)
        {
            return {};
        }
        const char* chars = env->GetStringUTFChars(value, nullptr);
        if (chars == nullptr)
        {
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
            }
            throw std::bad_alloc();
        }
        std::string result(chars);
        env->ReleaseStringUTFChars(value, chars);
        return result;
    }

    [[noreturn]] void ThrowPendingJava(JNIEnv* env)
    {
        std::string message = "Android Java call failed";
        LocalRef<jthrowable> throwable(env, env->ExceptionOccurred());
        env->ExceptionClear();
        if (throwable)
        {
            LocalRef<jclass> type(env, env->GetObjectClass(throwable.Get()));
            if (type)
            {
                const jmethodID toString = env->GetMethodID(
                    type.Get(),
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
                        message = JavaString(env, text.Get());
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

    void CheckJava(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            ThrowPendingJava(env);
        }
    }

    [[nodiscard]] LocalRef<jclass> ObjectClass(JNIEnv* env, jobject object)
    {
        if (object == nullptr)
        {
            throw System::NullReferenceException();
        }
        LocalRef<jclass> type(env, env->GetObjectClass(object));
        CheckJava(env);
        if (!type)
        {
            throw std::runtime_error("Android object class is unavailable");
        }
        return type;
    }

    [[nodiscard]] LocalRef<jclass> FindClass(JNIEnv* env, const char* name)
    {
        LocalRef<jclass> type(env, env->FindClass(name));
        CheckJava(env);
        if (!type)
        {
            throw std::runtime_error(std::string("Android class not found: ") + name);
        }
        return type;
    }

    [[nodiscard]] jmethodID Method(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature
    )
    {
        LocalRef<jclass> type = ObjectClass(env, object);
        const jmethodID method = env->GetMethodID(type.Get(), name, signature);
        CheckJava(env);
        if (method == nullptr)
        {
            throw std::runtime_error(
                std::string("Android method not found: ") + name
            );
        }
        return method;
    }

    template <typename... Args>
    jobject CallObject(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        Args... args
    )
    {
        const jmethodID method = Method(env, object, name, signature);
        jobject result = env->CallObjectMethod(object, method, args...);
        CheckJava(env);
        return result;
    }

    template <typename... Args>
    void CallVoid(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        Args... args
    )
    {
        const jmethodID method = Method(env, object, name, signature);
        env->CallVoidMethod(object, method, args...);
        CheckJava(env);
    }

    template <typename... Args>
    jint CallInt(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        Args... args
    )
    {
        const jmethodID method = Method(env, object, name, signature);
        const jint result = env->CallIntMethod(object, method, args...);
        CheckJava(env);
        return result;
    }

    template <typename... Args>
    jboolean CallBool(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        Args... args
    )
    {
        const jmethodID method = Method(env, object, name, signature);
        const jboolean result = env->CallBooleanMethod(object, method, args...);
        CheckJava(env);
        return result;
    }

    [[nodiscard]] LocalRef<jstring> NewString(JNIEnv* env, std::string_view value)
    {
        std::string text(value);
        LocalRef<jstring> result(env, env->NewStringUTF(text.c_str()));
        CheckJava(env);
        if (!result)
        {
            throw std::bad_alloc();
        }
        return result;
    }

    [[nodiscard]] jobject GlobalRef(JNIEnv* env, jobject object)
    {
        if (object == nullptr)
        {
            return nullptr;
        }
        jobject result = env->NewGlobalRef(object);
        CheckJava(env);
        if (result == nullptr)
        {
            throw std::bad_alloc();
        }
        return result;
    }

    void DeleteGlobal(JavaVM* vm, jobject object) noexcept
    {
        if (vm == nullptr || object == nullptr)
        {
            return;
        }
        try
        {
            ScopedEnv scoped(vm);
            scoped.Get()->DeleteGlobalRef(object);
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] std::string ExceptionMessage(std::exception_ptr error)
    {
        if (!error)
        {
            return "Unknown error";
        }
        try
        {
            std::rethrow_exception(error);
        }
        catch (const std::exception& ex)
        {
            return ex.what();
        }
        catch (...)
        {
            return "Unknown error";
        }
    }

    void ThrowJavaRuntime(JNIEnv* env, std::exception_ptr error) noexcept
    {
        if (env == nullptr || env->ExceptionCheck())
        {
            return;
        }
        const std::string message = ExceptionMessage(error);
        jclass type = env->FindClass("java/lang/RuntimeException");
        if (type != nullptr)
        {
            env->ThrowNew(type, message.c_str());
            env->DeleteLocalRef(type);
        }
    }

    std::mutex TaskGate;
    std::unordered_map<std::int64_t, std::function<void()>> Tasks;
    std::atomic<std::int64_t> NextTask{1};

    [[nodiscard]] std::int64_t AddTask(std::function<void()> action)
    {
        const std::int64_t token =
            NextTask.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(TaskGate);
        Tasks.emplace(token, std::move(action));
        return token;
    }

    [[nodiscard]] std::function<void()> TakeTask(std::int64_t token)
    {
        std::lock_guard lock(TaskGate);
        const auto found = Tasks.find(token);
        if (found == Tasks.end())
        {
            return {};
        }
        std::function<void()> action = std::move(found->second);
        Tasks.erase(found);
        return action;
    }

    struct ActivityState final
    {
        JavaVM* Vm = nullptr;
        jobject Activity = nullptr;
        jobject Root = nullptr;
        jobject Launcher = nullptr;

        ActivityState(
            JNIEnv* env,
            jobject activity,
            jobject root,
            jobject launcher
        )
        {
            if (env->GetJavaVM(&Vm) != JNI_OK || Vm == nullptr)
            {
                throw std::runtime_error("Android Java VM is unavailable");
            }
            Activity = GlobalRef(env, activity);
            try
            {
                Root = GlobalRef(env, root);
                Launcher = GlobalRef(env, launcher);
            }
            catch (...)
            {
                DeleteGlobal(Vm, Root);
                DeleteGlobal(Vm, Activity);
                Root = nullptr;
                Activity = nullptr;
                throw;
            }
        }

        ~ActivityState()
        {
            DeleteGlobal(Vm, Launcher);
            DeleteGlobal(Vm, Root);
            DeleteGlobal(Vm, Activity);
        }
    };

    struct JavaObject final
    {
        JavaVM* Vm = nullptr;
        jobject Object = nullptr;
        std::shared_ptr<MphRead::Droid::GameView> Game{};
        std::shared_ptr<MphRead::Droid::TouchOverlayView> Touch{};

        ~JavaObject()
        {
            if (Object != nullptr && (Game != nullptr || Touch != nullptr))
            {
                try
                {
                    ScopedEnv scoped(Vm);
                    CallVoid(
                        scoped.Get(),
                        Object,
                        "setNativePeer",
                        "(J)V",
                        static_cast<jlong>(0)
                    );
                }
                catch (...)
                {
                }
            }
            Touch.reset();
            Game.reset();
            DeleteGlobal(Vm, Object);
        }
    };

    [[nodiscard]] std::shared_ptr<JavaObject> MakeJavaObject(
        JNIEnv* env,
        jobject object
    )
    {
        if (object == nullptr)
        {
            return nullptr;
        }
        JavaVM* vm = nullptr;
        if (env->GetJavaVM(&vm) != JNI_OK || vm == nullptr)
        {
            throw std::runtime_error("Android Java VM is unavailable");
        }
        auto result = std::make_shared<JavaObject>();
        result->Vm = vm;
        result->Object = GlobalRef(env, object);
        return result;
    }

    [[nodiscard]] MphRead::Droid::MainActivityObjectRef Wrap(
        JNIEnv* env,
        jobject object
    )
    {
        return MphRead::Droid::MainActivityObjectRef{
            MakeJavaObject(env, object)
        };
    }

    [[nodiscard]] std::shared_ptr<JavaObject> Holder(
        const MphRead::Droid::MainActivityObjectRef& value
    )
    {
        if (!value.Native)
        {
            return nullptr;
        }
        return std::static_pointer_cast<JavaObject>(value.Native);
    }

    [[nodiscard]] jobject Object(
        const MphRead::Droid::MainActivityObjectRef& value
    )
    {
        const std::shared_ptr<JavaObject> holder = Holder(value);
        return holder == nullptr ? nullptr : holder->Object;
    }

    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        std::size_t index = 0;
        while (index < value.size())
        {
            const std::uint8_t first =
                static_cast<std::uint8_t>(value[index]);
            std::uint32_t codePoint = 0xFFFDU;
            std::size_t count = 1;
            if (first <= 0x7FU)
            {
                codePoint = first;
            }
            else if ((first & 0xE0U) == 0xC0U
                && index + 1 < value.size())
            {
                codePoint =
                    ((first & 0x1FU) << 6)
                    | (static_cast<std::uint8_t>(value[index + 1]) & 0x3FU);
                count = 2;
            }
            else if ((first & 0xF0U) == 0xE0U
                && index + 2 < value.size())
            {
                codePoint =
                    ((first & 0x0FU) << 12)
                    | ((static_cast<std::uint8_t>(value[index + 1]) & 0x3FU) << 6)
                    | (static_cast<std::uint8_t>(value[index + 2]) & 0x3FU);
                count = 3;
            }
            else if ((first & 0xF8U) == 0xF0U
                && index + 3 < value.size())
            {
                codePoint =
                    ((first & 0x07U) << 18)
                    | ((static_cast<std::uint8_t>(value[index + 1]) & 0x3FU) << 12)
                    | ((static_cast<std::uint8_t>(value[index + 2]) & 0x3FU) << 6)
                    | (static_cast<std::uint8_t>(value[index + 3]) & 0x3FU);
                count = 4;
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

    class JniMainActivityOwner final
        : public MphRead::Droid::MainActivityOwner
    {
    public:
        void Register(
            MphRead::Droid::MainActivity& activity,
            JNIEnv* env,
            jobject javaActivity,
            jobject root,
            jobject launcher
        )
        {
            auto state = std::make_shared<ActivityState>(
                env,
                javaActivity,
                root,
                launcher
            );
            std::lock_guard lock(_gate);
            _states[&activity] = std::move(state);
        }

        void Unregister(MphRead::Droid::MainActivity& activity)
        {
            std::lock_guard lock(_gate);
            _states.erase(&activity);
        }

        [[nodiscard]] std::shared_ptr<ActivityState> State(
            MphRead::Droid::MainActivity& activity
        )
        {
            std::lock_guard lock(_gate);
            const auto found = _states.find(&activity);
            if (found == _states.end())
            {
                throw std::runtime_error(
                    "Android MainActivity host state is unavailable"
                );
            }
            return found->second;
        }

        [[nodiscard]] std::shared_ptr<ActivityState> CurrentState()
        {
            std::lock_guard lock(_gate);
            if (MphRead::Droid::MainActivity* activity =
                    MphRead::Droid::MainActivity::Instance();
                activity != nullptr)
            {
                const auto found = _states.find(activity);
                if (found != _states.end())
                {
                    return found->second;
                }
            }
            if (_states.size() == 1)
            {
                return _states.begin()->second;
            }
            throw std::runtime_error(
                "Android MainActivity host state is unavailable"
            );
        }

        void AddUnhandledExceptionRaiser(
            MphRead::Droid::AndroidApp&,
            MphRead::Droid::AndroidUnhandledExceptionHandler handler
        ) override
        {
            std::lock_guard lock(_gate);
            _unhandled = std::move(handler);
        }

        void AddFluentTheme(MphRead::Droid::AndroidApp&) override {}
        void SetRequestedThemeVariantDark(
            MphRead::Droid::AndroidApp&) override {}
        void BaseInitialize(MphRead::Droid::AndroidApp&) override {}

        [[nodiscard]] MphRead::Droid::AndroidActivityLifetime
            ActivityApplicationLifetime(
                MphRead::Droid::AndroidApp&) override
        {
            (void)CurrentState();
            return MphRead::Droid::AndroidActivityLifetime{
                std::make_shared<int>(1)
            };
        }

        void SetActivityMainViewFactory(
            const MphRead::Droid::AndroidActivityLifetime&,
            MphRead::Droid::AndroidMainViewFactory factory
        ) override
        {
            std::lock_guard lock(_gate);
            _mainViewFactory = std::move(factory);
        }

        [[nodiscard]] MphRead::Droid::AndroidSingleViewLifetime
            SingleViewApplicationLifetime(
                MphRead::Droid::AndroidApp&) override
        {
            return {};
        }

        void SetSingleViewMainView(
            const MphRead::Droid::AndroidSingleViewLifetime&,
            MphRead::Droid::Av::Controls::ControlPtr) override
        {
        }

        void BaseOnFrameworkInitializationCompleted(
            MphRead::Droid::AndroidApp&) override
        {
        }

        [[nodiscard]] MphRead::Droid::MainActivityAppBuilderRef
            BaseCustomizeAppBuilder(
                MphRead::Droid::MainActivity&,
                MphRead::Droid::MainActivityAppBuilderRef builder
            ) override
        {
            return builder;
        }

        [[nodiscard]] MphRead::Droid::MainActivityAppBuilderRef
            WithInterFont(
                MphRead::Droid::MainActivityAppBuilderRef builder
            ) override
        {
            return builder;
        }

        [[nodiscard]] std::optional<std::string> ExternalFilesPath(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jstring> path(
                env,
                static_cast<jstring>(
                    CallObject(
                        env,
                        state->Activity,
                        "fruityExternalFilesPath",
                        "()Ljava/lang/String;"
                    )
                )
            );
            return path
                ? std::optional<std::string>(JavaString(env, path.Get()))
                : std::nullopt;
        }

        [[nodiscard]] std::optional<std::string> InternalFilesPath(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jstring> path(
                env,
                static_cast<jstring>(
                    CallObject(
                        env,
                        state->Activity,
                        "fruityInternalFilesPath",
                        "()Ljava/lang/String;"
                    )
                )
            );
            return path
                ? std::optional<std::string>(JavaString(env, path.Get()))
                : std::nullopt;
        }

        void CreateDirectory(std::string_view path) override
        {
            std::filesystem::create_directories(
                std::filesystem::path(path)
            );
        }

        [[nodiscard]] std::string CombinePath(
            std::string_view left,
            std::string_view right
        ) override
        {
            return (
                std::filesystem::path(left)
                / std::filesystem::path(right)
            ).string();
        }

        void WriteEmptyFile(std::string_view path) override
        {
            std::ofstream file(
                std::filesystem::path(path),
                std::ios::binary | std::ios::trunc
            );
            if (!file)
            {
                throw std::runtime_error(
                    "could not create " + std::string(path)
                );
            }
        }

        void DeleteFile(std::string_view path) override
        {
            std::error_code error;
            (void)std::filesystem::remove(
                std::filesystem::path(path),
                error
            );
            if (error)
            {
                throw std::filesystem::filesystem_error(
                    "could not delete file",
                    std::filesystem::path(path),
                    error
                );
            }
        }

        [[nodiscard]] bool FileExists(std::string_view path) override
        {
            std::error_code error;
            const bool exists = std::filesystem::is_regular_file(
                std::filesystem::path(path),
                error
            );
            return !error && exists;
        }

        void SetCurrentDirectory(std::string_view path) override
        {
            std::filesystem::current_path(std::filesystem::path(path));

            const auto state = CurrentState();
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jstring> javaPath = NewString(env, path);
            CallVoid(
                env,
                state->Activity,
                "fruityPrepareAssets",
                "(Ljava/lang/String;)V",
                javaPath.Get()
            );
        }

        [[nodiscard]] std::u16string ToUtf16(
            std::string_view value
        ) override
        {
            return Utf8ToUtf16(value);
        }

        [[nodiscard]] jobject Assets(
            JNIEnv* env,
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            return CallObject(
                env,
                state->Activity,
                "getAssets",
                "()Landroid/content/res/AssetManager;"
            );
        }

        [[nodiscard]] std::string ExceptionToString(
            std::exception_ptr error
        ) override
        {
            return ExceptionMessage(error);
        }

        [[nodiscard]] std::string ExceptionMessage(
            std::exception_ptr error
        ) override
        {
            return ::ExceptionMessage(error);
        }

        [[nodiscard]] std::string FormatInt32Current(
            std::int32_t value
        ) override
        {
            return std::to_string(value);
        }

        [[nodiscard]] std::string FormatInt64Current(
            std::int64_t value
        ) override
        {
            return std::to_string(value);
        }

        [[nodiscard]] std::string FormatDoubleFixed1Current(
            double value
        ) override
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1) << value;
            return stream.str();
        }

        [[noreturn]] void ThrowNullReference() override
        {
            throw System::NullReferenceException();
        }

        void RunBackground(Action action) override
        {
            MphRead::Droid::AndroidUnhandledExceptionHandler handler;
            {
                std::lock_guard lock(_gate);
                handler = _unhandled;
            }
            std::thread(
                [action = std::move(action), handler = std::move(handler)]()
                {
                    try
                    {
                        action();
                    }
                    catch (...)
                    {
                        if (handler)
                        {
                            handler(std::current_exception());
                        }
                    }
                }
            ).detach();
        }

        void RunOnUiThread(Action action) override
        {
            const auto state = CurrentState();
            const std::int64_t token = AddTask(std::move(action));
            try
            {
                ScopedEnv scoped(state->Vm);
                CallVoid(
                    scoped.Get(),
                    state->Activity,
                    "fruityPostNativeTask",
                    "(J)V",
                    static_cast<jlong>(token)
                );
            }
            catch (...)
            {
                (void)TakeTask(token);
                throw;
            }
        }

        [[nodiscard]] std::shared_future<int> CompletedIntTask(
            int result
        ) override
        {
            std::promise<int> promise;
            promise.set_value(result);
            return promise.get_future().share();
        }

        [[nodiscard]] std::shared_future<int> RunBackgroundInt(
            std::function<int()> action
        ) override
        {
            return std::async(
                std::launch::async,
                std::move(action)
            ).share();
        }

        [[nodiscard]] std::int32_t PreviewRunRender(
            const std::vector<std::string>& rooms,
            std::int32_t width,
            std::int32_t height,
            const std::function<void(const std::string&)>& report,
            const std::function<bool()>& cancelled
        ) override
        {
            return MphRead::Droid::PreviewRun::Render(
                rooms,
                width,
                height,
                report,
                cancelled
            );
        }

        void BaseOnCreate(
            MphRead::Droid::MainActivity& activity,
            jobject
        ) override
        {
            MphRead::Droid::AndroidMainViewFactory factory;
            {
                std::lock_guard lock(_gate);
                factory = _mainViewFactory;
            }
            if (!factory)
            {
                throw std::runtime_error(
                    "Android launcher view factory is unavailable"
                );
            }

            const std::shared_ptr<MphRead::Droid::AndroidUiSurface> surface =
                MphRead::Droid::AndroidUiSurface::Ensure();
            if (surface == nullptr)
            {
                throw std::runtime_error(
                    "could not create the Android launcher surface"
                );
            }
            surface->Show(factory());

            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            const jint width = CallInt(
                env, state->Launcher, "getWidth", "()I"
            );
            const jint height = CallInt(
                env, state->Launcher, "getHeight", "()I"
            );
            if (width > 0 && height > 0)
            {
                surface->Resize(width, height);
            }
            CallVoid(
                env,
                state->Launcher,
                "invalidate",
                "()V"
            );
        }

        void BaseOnConfigurationChanged(
            MphRead::Droid::MainActivity&,
            jobject) override
        {
        }

        void BaseOnPause(MphRead::Droid::MainActivity&) override {}
        void BaseOnResume(MphRead::Droid::MainActivity&) override {}
        void BaseOnWindowFocusChanged(
            MphRead::Droid::MainActivity&,
            bool) override
        {
        }

        void BaseOnDestroy(MphRead::Droid::MainActivity&) override
        {
            if (const auto surface =
                    MphRead::Droid::AndroidUiSurface::Current();
                surface != nullptr)
            {
                surface->Hide();
            }
        }

        void BaseOnBackPressed(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            CallVoid(
                scoped.Get(),
                state->Activity,
                "fruitySuperOnBackPressed",
                "()V"
            );
        }

        [[nodiscard]] bool BaseDispatchKeyEvent(
            MphRead::Droid::MainActivity&,
            jobject) override
        {
            return false;
        }

        [[nodiscard]] bool BaseDispatchTouchEvent(
            MphRead::Droid::MainActivity&,
            jobject) override
        {
            return false;
        }

        [[nodiscard]] bool BaseDispatchGenericMotionEvent(
            MphRead::Droid::MainActivity&,
            jobject) override
        {
            return false;
        }

        void Finish(MphRead::Droid::MainActivity& activity) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            CallVoid(
                scoped.Get(),
                state->Activity,
                "finish",
                "()V"
            );
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef
            ContentViewGroup(
                MphRead::Droid::MainActivity& activity
            ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            return Wrap(scoped.Get(), state->Root);
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef ChildAt(
            const MphRead::Droid::MainActivityObjectRef& parent,
            std::int32_t index
        ) override
        {
            const std::shared_ptr<JavaObject> holder = Holder(parent);
            if (holder == nullptr)
            {
                return {};
            }
            ScopedEnv scoped(holder->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> child(
                env,
                CallObject(
                    env,
                    holder->Object,
                    "getChildAt",
                    "(I)Landroid/view/View;",
                    static_cast<jint>(index)
                )
            );
            return Wrap(env, child.Get());
        }

        [[nodiscard]] MphRead::Droid::MainActivitySize ViewSize(
            const MphRead::Droid::MainActivityObjectRef& view
        ) override
        {
            const std::shared_ptr<JavaObject> holder = Holder(view);
            if (holder == nullptr)
            {
                return {};
            }
            ScopedEnv scoped(holder->Vm);
            return {
                CallInt(
                    scoped.Get(),
                    holder->Object,
                    "getWidth",
                    "()I"
                ),
                CallInt(
                    scoped.Get(),
                    holder->Object,
                    "getHeight",
                    "()I"
                )
            };
        }

        void SetViewVisible(
            const MphRead::Droid::MainActivityObjectRef& view
        ) override
        {
            SetVisibility(view, 0);
        }

        void SetViewGone(
            const MphRead::Droid::MainActivityObjectRef& view
        ) override
        {
            SetVisibility(view, 8);
        }

        void AddView(
            const MphRead::Droid::MainActivityObjectRef& parent,
            const MphRead::Droid::MainActivityObjectRef& child
        ) override
        {
            const auto p = Holder(parent);
            const auto c = Holder(child);
            if (p == nullptr || c == nullptr)
            {
                return;
            }
            ScopedEnv scoped(p->Vm);
            CallVoid(
                scoped.Get(),
                p->Object,
                "addView",
                "(Landroid/view/View;II)V",
                c->Object,
                static_cast<jint>(-1),
                static_cast<jint>(-1)
            );
        }

        void RemoveView(
            const MphRead::Droid::MainActivityObjectRef& parent,
            const MphRead::Droid::MainActivityObjectRef& child
        ) override
        {
            const auto p = Holder(parent);
            const auto c = Holder(child);
            if (p == nullptr || c == nullptr)
            {
                return;
            }
            ScopedEnv scoped(p->Vm);
            CallVoid(
                scoped.Get(),
                p->Object,
                "removeView",
                "(Landroid/view/View;)V",
                c->Object
            );
        }

        void BringToFront(
            const MphRead::Droid::MainActivityObjectRef& view
        ) override
        {
            CallViewVoid(view, "bringToFront");
        }

        void RequestLayout(
            const MphRead::Droid::MainActivityObjectRef& view
        ) override
        {
            CallViewVoid(view, "requestLayout");
        }

        void Invalidate(
            const MphRead::Droid::MainActivityObjectRef& view
        ) override
        {
            CallViewVoid(view, "invalidate");
        }

        void RequestDecorLayout(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const MphRead::Droid::MainActivityObjectRef window =
                Window(activity);
            const auto holder = Holder(window);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> decor(
                env,
                CallObject(
                    env,
                    holder->Object,
                    "getDecorView",
                    "()Landroid/view/View;"
                )
            );
            if (decor)
            {
                CallVoid(env, decor.Get(), "requestLayout", "()V");
            }
        }

        void PostDelayed(
            const MphRead::Droid::MainActivityObjectRef& view,
            Action action,
            std::int64_t milliseconds
        ) override
        {
            const auto holder = Holder(view);
            const auto state = CurrentState();
            if (holder == nullptr)
            {
                return;
            }
            const std::int64_t token = AddTask(std::move(action));
            try
            {
                ScopedEnv scoped(state->Vm);
                CallVoid(
                    scoped.Get(),
                    state->Activity,
                    "fruityPostNativeTaskDelayed",
                    "(Landroid/view/View;JJ)V",
                    holder->Object,
                    static_cast<jlong>(token),
                    static_cast<jlong>(milliseconds)
                );
            }
            catch (...)
            {
                (void)TakeTask(token);
                throw;
            }
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef CreateGameView(
            MphRead::Droid::MainActivity& activity,
            MphRead::Droid::TouchControls& controls,
            std::shared_ptr<MphRead::Droid::AndroidInput> input,
            const MphRead::Mods::Launcher::LaunchPlan& plan,
            Action onBuildClose,
            Action onEnd,
            Action onLoaded,
            ErrorAction onError,
            Action onPauseMenu,
            BoolAction onSoftKeyboard
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> view(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "fruityCreateGameSurfaceView",
                    "()Lfr/livetek/fruityprime/GameSurfaceView;"
                )
            );
            if (!view)
            {
                throw std::runtime_error(
                    "could not create the Android game SurfaceView"
                );
            }

            auto build = [
                plan,
                onBuildClose = std::move(onBuildClose)
            ](
                MphRead::Droid::AndroidInput& gameInput,
                OpenTK::Mathematics::Vector2i size
            ) mutable
            {
                return MphRead::Droid::AndroidMatch::Build(
                    gameInput,
                    size,
                    plan,
                    onBuildClose
                );
            };

            auto game = std::make_shared<MphRead::Droid::GameView>(
                env,
                view.Get(),
                controls,
                std::move(input),
                std::move(build),
                std::move(onEnd),
                std::move(onLoaded),
                std::move(onError),
                std::move(onPauseMenu),
                std::move(onSoftKeyboard)
            );

            auto holder = MakeJavaObject(env, view.Get());
            holder->Game = game;
            CallVoid(
                env,
                view.Get(),
                "setNativePeer",
                "(J)V",
                reinterpret_cast<jlong>(game.get())
            );
            return MphRead::Droid::MainActivityObjectRef{
                std::move(holder)
            };
        }

        void GameViewSetZOrderMediaOverlay(
            const MphRead::Droid::MainActivityObjectRef& gameView,
            bool value
        ) override
        {
            const auto holder = Holder(gameView);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            CallVoid(
                scoped.Get(),
                holder->Object,
                "setZOrderMediaOverlay",
                "(Z)V",
                value ? JNI_TRUE : JNI_FALSE
            );
        }

        void GameViewOnPause(
            const MphRead::Droid::MainActivityObjectRef& gameView
        ) override
        {
            if (const auto holder = Holder(gameView);
                holder != nullptr && holder->Game != nullptr)
            {
                holder->Game->OnPause();
            }
        }

        void GameViewOnResume(
            const MphRead::Droid::MainActivityObjectRef& gameView
        ) override
        {
            if (const auto holder = Holder(gameView);
                holder != nullptr && holder->Game != nullptr)
            {
                holder->Game->OnResume();
            }
        }

        void GameViewStop(
            const MphRead::Droid::MainActivityObjectRef& gameView
        ) override
        {
            if (const auto holder = Holder(gameView);
                holder != nullptr && holder->Game != nullptr)
            {
                holder->Game->Stop();
            }
        }

        void GameViewRequestFocus(
            const MphRead::Droid::MainActivityObjectRef& gameView
        ) override
        {
            const auto holder = Holder(gameView);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            (void)CallBool(
                scoped.Get(),
                holder->Object,
                "requestFocus",
                "()Z"
            );
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef
            InputMethodManager(
                MphRead::Droid::MainActivity& activity
            ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jstring> name = NewString(env, "input_method");
            LocalRef<jobject> manager(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "getSystemService",
                    "(Ljava/lang/String;)Ljava/lang/Object;",
                    name.Get()
                )
            );
            return Wrap(env, manager.Get());
        }

        void ShowSoftInput(
            const MphRead::Droid::MainActivityObjectRef& inputMethodManager,
            const MphRead::Droid::MainActivityObjectRef& gameView
        ) override
        {
            const auto manager = Holder(inputMethodManager);
            const auto view = Holder(gameView);
            if (manager == nullptr || view == nullptr)
            {
                return;
            }
            ScopedEnv scoped(manager->Vm);
            (void)CallBool(
                scoped.Get(),
                manager->Object,
                "showSoftInput",
                "(Landroid/view/View;I)Z",
                view->Object,
                static_cast<jint>(1)
            );
        }

        void HideSoftInput(
            const MphRead::Droid::MainActivityObjectRef& inputMethodManager,
            const MphRead::Droid::MainActivityObjectRef& gameView
        ) override
        {
            const auto manager = Holder(inputMethodManager);
            const auto view = Holder(gameView);
            if (manager == nullptr || view == nullptr)
            {
                return;
            }
            ScopedEnv scoped(manager->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> token(
                env,
                CallObject(
                    env,
                    view->Object,
                    "getWindowToken",
                    "()Landroid/os/IBinder;"
                )
            );
            if (token)
            {
                (void)CallBool(
                    env,
                    manager->Object,
                    "hideSoftInputFromWindow",
                    "(Landroid/os/IBinder;I)Z",
                    token.Get(),
                    static_cast<jint>(0)
                );
            }
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef
            CreateTouchOverlayView(
                MphRead::Droid::MainActivity& activity,
                MphRead::Droid::TouchControls& controls
            ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> view(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "fruityCreateTouchOverlayView",
                    "()Lfr/livetek/fruityprime/TouchOverlayView;"
                )
            );
            if (!view)
            {
                throw std::runtime_error(
                    "could not create the Android touch overlay"
                );
            }

            auto touch =
                std::make_shared<MphRead::Droid::TouchOverlayView>(
                    env,
                    view.Get(),
                    &controls
                );
            auto holder = MakeJavaObject(env, view.Get());
            holder->Touch = touch;
            CallVoid(
                env,
                view.Get(),
                "setNativePeer",
                "(J)V",
                reinterpret_cast<jlong>(touch.get())
            );
            return MphRead::Droid::MainActivityObjectRef{
                std::move(holder)
            };
        }

        void TouchOverlayRefresh(
            const MphRead::Droid::MainActivityObjectRef& overlay
        ) override
        {
            if (const auto holder = Holder(overlay);
                holder != nullptr && holder->Touch != nullptr)
            {
                holder->Touch->Refresh();
            }
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef
            CreateNoticeTextView(
                MphRead::Droid::MainActivity& activity,
                std::string text,
                std::uint32_t textArgb,
                std::uint32_t backgroundArgb
            ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jstring> javaText = NewString(env, text);
            LocalRef<jobject> view(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "fruityCreateNoticeTextView",
                    "(Ljava/lang/String;II)Landroid/widget/TextView;",
                    javaText.Get(),
                    static_cast<jint>(textArgb),
                    static_cast<jint>(backgroundArgb)
                )
            );
            return Wrap(env, view.Get());
        }

        void SetNoticeText(
            const MphRead::Droid::MainActivityObjectRef& notice,
            std::string text
        ) override
        {
            const auto holder = Holder(notice);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jstring> javaText = NewString(env, text);
            CallVoid(
                env,
                holder->Object,
                "setText",
                "(Ljava/lang/CharSequence;)V",
                javaText.Get()
            );
        }

        void ToastLong(
            MphRead::Droid::MainActivity& activity,
            std::string text
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jstring> javaText = NewString(env, text);
            CallVoid(
                env,
                state->Activity,
                "fruityShowToast",
                "(Ljava/lang/String;)V",
                javaText.Get()
            );
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef Window(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> window(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "getWindow",
                    "()Landroid/view/Window;"
                )
            );
            return Wrap(env, window.Get());
        }

        void AddKeepScreenOn(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            WindowFlag(activity, true);
        }

        void ClearKeepScreenOn(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            WindowFlag(activity, false);
        }

        [[nodiscard]] MphRead::Droid::MainActivityOrientation
            RequestedOrientation(
                MphRead::Droid::MainActivity& activity
            ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            return static_cast<MphRead::Droid::MainActivityOrientation>(
                CallInt(
                    scoped.Get(),
                    state->Activity,
                    "getRequestedOrientation",
                    "()I"
                )
            );
        }

        void RequestedOrientation(
            MphRead::Droid::MainActivity& activity,
            MphRead::Droid::MainActivityOrientation orientation
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            CallVoid(
                scoped.Get(),
                state->Activity,
                "setRequestedOrientation",
                "(I)V",
                static_cast<jint>(orientation)
            );
        }

        [[nodiscard]] std::int64_t UptimeMillis() override
        {
            const auto state = CurrentState();
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jclass> type =
                FindClass(env, "android/os/SystemClock");
            const jmethodID method = env->GetStaticMethodID(
                type.Get(),
                "uptimeMillis",
                "()J"
            );
            CheckJava(env);
            const jlong value = env->CallStaticLongMethod(
                type.Get(),
                method
            );
            CheckJava(env);
            return static_cast<std::int64_t>(value);
        }

        [[nodiscard]] bool IsAndroidVersionAtLeast30() override
        {
            const auto state = CurrentState();
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jclass> type =
                FindClass(env, "android/os/Build$VERSION");
            const jfieldID field = env->GetStaticFieldID(
                type.Get(),
                "SDK_INT",
                "I"
            );
            CheckJava(env);
            const jint sdk = env->GetStaticIntField(type.Get(), field);
            CheckJava(env);
            return sdk >= 30;
        }

        [[nodiscard]] std::int32_t DisplayRotation(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> display(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "getDisplay",
                    "()Landroid/view/Display;"
                )
            );
            return display
                ? CallInt(env, display.Get(), "getRotation", "()I")
                : -1;
        }

        [[nodiscard]] std::int32_t DefaultDisplayRotation(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> manager(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "getWindowManager",
                    "()Landroid/view/WindowManager;"
                )
            );
            if (!manager)
            {
                return -1;
            }
            LocalRef<jobject> display(
                env,
                CallObject(
                    env,
                    manager.Get(),
                    "getDefaultDisplay",
                    "()Landroid/view/Display;"
                )
            );
            return display
                ? CallInt(env, display.Get(), "getRotation", "()I")
                : -1;
        }

        [[nodiscard]] double DisplayDensity(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> resources(
                env,
                CallObject(
                    env,
                    state->Activity,
                    "getResources",
                    "()Landroid/content/res/Resources;"
                )
            );
            LocalRef<jobject> metrics(
                env,
                CallObject(
                    env,
                    resources.Get(),
                    "getDisplayMetrics",
                    "()Landroid/util/DisplayMetrics;"
                )
            );
            LocalRef<jclass> type = ObjectClass(env, metrics.Get());
            const jfieldID field = env->GetFieldID(
                type.Get(),
                "density",
                "F"
            );
            CheckJava(env);
            const jfloat density =
                env->GetFloatField(metrics.Get(), field);
            CheckJava(env);
            return static_cast<double>(density);
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef DisplayManager(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            return Wrap(scoped.Get(), state->Activity);
        }

        void RegisterDisplayListener(
            const MphRead::Droid::MainActivityObjectRef&,
            MphRead::Droid::MainActivity& activity
        ) override
        {
            SetListener(activity, "fruitySetDisplayListenerEnabled", true);
        }

        void UnregisterDisplayListener(
            const MphRead::Droid::MainActivityObjectRef&,
            MphRead::Droid::MainActivity& activity
        ) override
        {
            SetListener(activity, "fruitySetDisplayListenerEnabled", false);
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef InputManager(
            MphRead::Droid::MainActivity& activity
        ) override
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            return Wrap(scoped.Get(), state->Activity);
        }

        void RegisterInputDeviceListener(
            const MphRead::Droid::MainActivityObjectRef&,
            MphRead::Droid::MainActivity& activity
        ) override
        {
            SetListener(activity, "fruitySetInputListenerEnabled", true);
        }

        void UnregisterInputDeviceListener(
            const MphRead::Droid::MainActivityObjectRef&,
            MphRead::Droid::MainActivity& activity
        ) override
        {
            SetListener(activity, "fruitySetInputListenerEnabled", false);
        }

        [[nodiscard]] std::int32_t KeyEventAction(jobject event) override
        {
            return EventInt(event, "getAction");
        }

        [[nodiscard]] std::int32_t KeyEventKeyCode(jobject event) override
        {
            return EventInt(event, "getKeyCode");
        }

        [[nodiscard]] std::int32_t MotionEventActionMasked(
            jobject event
        ) override
        {
            return EventInt(event, "getActionMasked");
        }

        [[nodiscard]] MphRead::Droid::MainActivityObjectRef
            WindowInsetsController(
                const MphRead::Droid::MainActivityObjectRef& window
            ) override
        {
            const auto holder = Holder(window);
            if (holder == nullptr)
            {
                return {};
            }
            ScopedEnv scoped(holder->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> controller(
                env,
                CallObject(
                    env,
                    holder->Object,
                    "getInsetsController",
                    "()Landroid/view/WindowInsetsController;"
                )
            );
            return Wrap(env, controller.Get());
        }

        void SetTransientBarsBySwipe(
            const MphRead::Droid::MainActivityObjectRef& controller
        ) override
        {
            const auto holder = Holder(controller);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            CallVoid(
                scoped.Get(),
                holder->Object,
                "setSystemBarsBehavior",
                "(I)V",
                static_cast<jint>(2)
            );
        }

        void HideSystemBars(
            const MphRead::Droid::MainActivityObjectRef& controller
        ) override
        {
            SetSystemBars(controller, false);
        }

        void ShowSystemBars(
            const MphRead::Droid::MainActivityObjectRef& controller
        ) override
        {
            SetSystemBars(controller, true);
        }

        void SetLegacySystemUiVisibility(
            const MphRead::Droid::MainActivityObjectRef& window,
            bool immersive
        ) override
        {
            const auto holder = Holder(window);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jobject> decor(
                env,
                CallObject(
                    env,
                    holder->Object,
                    "getDecorView",
                    "()Landroid/view/View;"
                )
            );
            if (decor)
            {
                CallVoid(
                    env,
                    decor.Get(),
                    "setSystemUiVisibility",
                    "(I)V",
                    static_cast<jint>(immersive ? 4358 : 0)
                );
            }
        }

    private:
        void SetVisibility(
            const MphRead::Droid::MainActivityObjectRef& view,
            jint visibility
        )
        {
            const auto holder = Holder(view);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            CallVoid(
                scoped.Get(),
                holder->Object,
                "setVisibility",
                "(I)V",
                visibility
            );
        }

        void CallViewVoid(
            const MphRead::Droid::MainActivityObjectRef& view,
            const char* method
        )
        {
            const auto holder = Holder(view);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            CallVoid(scoped.Get(), holder->Object, method, "()V");
        }

        void WindowFlag(
            MphRead::Droid::MainActivity& activity,
            bool set
        )
        {
            const MphRead::Droid::MainActivityObjectRef window =
                Window(activity);
            const auto holder = Holder(window);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            CallVoid(
                scoped.Get(),
                holder->Object,
                set ? "addFlags" : "clearFlags",
                "(I)V",
                static_cast<jint>(128)
            );
        }

        void SetListener(
            MphRead::Droid::MainActivity& activity,
            const char* method,
            bool enabled
        )
        {
            const auto state = State(activity);
            ScopedEnv scoped(state->Vm);
            CallVoid(
                scoped.Get(),
                state->Activity,
                method,
                "(Z)V",
                enabled ? JNI_TRUE : JNI_FALSE
            );
        }

        [[nodiscard]] std::int32_t EventInt(
            jobject event,
            const char* method
        )
        {
            if (event == nullptr)
            {
                return 0;
            }
            const auto state = CurrentState();
            ScopedEnv scoped(state->Vm);
            return CallInt(
                scoped.Get(),
                event,
                method,
                "()I"
            );
        }

        void SetSystemBars(
            const MphRead::Droid::MainActivityObjectRef& controller,
            bool show
        )
        {
            const auto holder = Holder(controller);
            if (holder == nullptr)
            {
                return;
            }
            ScopedEnv scoped(holder->Vm);
            JNIEnv* env = scoped.Get();
            LocalRef<jclass> type =
                FindClass(env, "android/view/WindowInsets$Type");
            const jmethodID systemBars = env->GetStaticMethodID(
                type.Get(),
                "systemBars",
                "()I"
            );
            CheckJava(env);
            const jint mask = env->CallStaticIntMethod(
                type.Get(),
                systemBars
            );
            CheckJava(env);
            CallVoid(
                env,
                holder->Object,
                show ? "show" : "hide",
                "(I)V",
                mask
            );
        }

        std::mutex _gate;
        std::unordered_map<
            MphRead::Droid::MainActivity*,
            std::shared_ptr<ActivityState>> _states;
        MphRead::Droid::AndroidMainViewFactory _mainViewFactory{};
        MphRead::Droid::AndroidUnhandledExceptionHandler _unhandled{};
    };

    [[nodiscard]] JniMainActivityOwner& HostOwner()
    {
        static JniMainActivityOwner owner;
        return owner;
    }

    struct NativeActivityHandle final
    {
        std::unique_ptr<MphRead::Droid::MainActivity> Activity;
    };

    MphRead::Droid::AndroidApp NativeApplication;
    std::once_flag NativeApplicationOnce;

    [[nodiscard]] NativeActivityHandle& RequireHandle(jlong handle)
    {
        if (handle == 0)
        {
            throw std::runtime_error(
                "Android native activity handle is null"
            );
        }
        return *reinterpret_cast<NativeActivityHandle*>(handle);
    }

    template <typename T>
    [[nodiscard]] T& RequirePeer(jlong peer)
    {
        if (peer == 0)
        {
            throw std::runtime_error("Android native view peer is null");
        }
        return *reinterpret_cast<T*>(peer);
    }

    template <typename Action>
    void JniVoid(JNIEnv* env, Action&& action) noexcept
    {
        try
        {
            action();
        }
        catch (...)
        {
            ThrowJavaRuntime(env, std::current_exception());
        }
    }

    template <typename Action>
    jboolean JniBoolean(JNIEnv* env, Action&& action) noexcept
    {
        try
        {
            return action() ? JNI_TRUE : JNI_FALSE;
        }
        catch (...)
        {
            ThrowJavaRuntime(env, std::current_exception());
            return JNI_FALSE;
        }
    }

    template <typename Action>
    jobject JniObject(JNIEnv* env, Action&& action) noexcept
    {
        try
        {
            return action();
        }
        catch (...)
        {
            ThrowJavaRuntime(env, std::current_exception());
            return nullptr;
        }
    }
}

namespace MphRead::Droid
{
    MainActivityOwner& GetMainActivityOwner() noexcept
    {
        return HostOwner();
    }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeCreate(
    JNIEnv* env,
    jobject self,
    jobject savedInstanceState,
    jobject root,
    jobject launcher
)
{
    try
    {
        auto handle = std::make_unique<NativeActivityHandle>();
        handle->Activity =
            std::make_unique<MphRead::Droid::MainActivity>(env, self);
        HostOwner().Register(
            *handle->Activity,
            env,
            self,
            root,
            launcher
        );

        try
        {
            std::call_once(
                NativeApplicationOnce,
                [&]()
                {
                    (void)handle->Activity->CustomizeAppBuilder({});
                    NativeApplication.Initialize();
                    NativeApplication.OnFrameworkInitializationCompleted();
                }
            );
            handle->Activity->OnCreate(savedInstanceState);
        }
        catch (...)
        {
            HostOwner().Unregister(*handle->Activity);
            throw;
        }

        return reinterpret_cast<jlong>(handle.release());
    }
    catch (...)
    {
        ThrowJavaRuntime(env, std::current_exception());
        return 0;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeDestroy(
    JNIEnv* env,
    jobject,
    jlong value
)
{
    JniVoid(env, [&]()
    {
        std::unique_ptr<NativeActivityHandle> handle(
            reinterpret_cast<NativeActivityHandle*>(value)
        );
        if (!handle || !handle->Activity)
        {
            return;
        }
        std::exception_ptr pending;
        try
        {
            handle->Activity->OnDestroy();
        }
        catch (...)
        {
            pending = std::current_exception();
        }
        HostOwner().Unregister(*handle->Activity);
        if (pending)
        {
            std::rethrow_exception(pending);
        }
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeOnConfigurationChanged(
    JNIEnv* env,
    jobject,
    jlong handle,
    jobject configuration
)
{
    JniVoid(env, [&]()
    {
        RequireHandle(handle).Activity->OnConfigurationChanged(
            configuration
        );
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeOnPause(
    JNIEnv* env,
    jobject,
    jlong handle
)
{
    JniVoid(env, [&]()
    {
        RequireHandle(handle).Activity->OnPause();
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeOnResume(
    JNIEnv* env,
    jobject,
    jlong handle
)
{
    JniVoid(env, [&]()
    {
        RequireHandle(handle).Activity->OnResume();
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeOnWindowFocusChanged(
    JNIEnv* env,
    jobject,
    jlong handle,
    jboolean hasFocus
)
{
    JniVoid(env, [&]()
    {
        RequireHandle(handle).Activity->OnWindowFocusChanged(
            hasFocus == JNI_TRUE
        );
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeOnBackPressed(
    JNIEnv* env,
    jobject,
    jlong handle
)
{
    JniVoid(env, [&]()
    {
        RequireHandle(handle).Activity->OnBackPressed();
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeDispatchKeyEvent(
    JNIEnv* env,
    jobject,
    jlong handle,
    jobject event
)
{
    return JniBoolean(env, [&]()
    {
        return RequireHandle(handle).Activity->DispatchKeyEvent(event);
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeDispatchTouchEvent(
    JNIEnv* env,
    jobject,
    jlong handle,
    jobject event
)
{
    return JniBoolean(env, [&]()
    {
        return RequireHandle(handle).Activity->DispatchTouchEvent(event);
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeDispatchGenericMotionEvent(
    JNIEnv* env,
    jobject,
    jlong handle,
    jobject event
)
{
    return JniBoolean(env, [&]()
    {
        return RequireHandle(handle)
            .Activity->DispatchGenericMotionEvent(event);
    });
}

#define FRUITY_ACTIVITY_INT_CALLBACK(Name, MethodName) \
extern "C" JNIEXPORT void JNICALL \
Java_fr_livetek_fruityprime_MainActivity_##Name( \
    JNIEnv* env, jobject, jlong handle, jint value) \
{ \
    JniVoid(env, [&]() \
    { \
        RequireHandle(handle).Activity->MethodName( \
            static_cast<std::int32_t>(value)); \
    }); \
}

FRUITY_ACTIVITY_INT_CALLBACK(nativeOnDisplayAdded, OnDisplayAdded)
FRUITY_ACTIVITY_INT_CALLBACK(nativeOnDisplayRemoved, OnDisplayRemoved)
FRUITY_ACTIVITY_INT_CALLBACK(nativeOnDisplayChanged, OnDisplayChanged)
FRUITY_ACTIVITY_INT_CALLBACK(nativeOnInputDeviceAdded, OnInputDeviceAdded)
FRUITY_ACTIVITY_INT_CALLBACK(nativeOnInputDeviceChanged, OnInputDeviceChanged)
FRUITY_ACTIVITY_INT_CALLBACK(nativeOnInputDeviceRemoved, OnInputDeviceRemoved)

#undef FRUITY_ACTIVITY_INT_CALLBACK

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_MainActivity_nativeRunTask(
    JNIEnv* env,
    jclass,
    jlong token
)
{
    JniVoid(env, [&]()
    {
        std::function<void()> action =
            TakeTask(static_cast<std::int64_t>(token));
        if (action)
        {
            action();
        }
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_LauncherView_nativeRender(
    JNIEnv* env,
    jclass,
    jobject bitmap,
    jint width,
    jint height
)
{
    JniVoid(env, [&]()
    {
        const auto surface = MphRead::Droid::AndroidUiSurface::Current();
        if (surface == nullptr || bitmap == nullptr
            || width <= 0 || height <= 0)
        {
            return;
        }

        surface->Resize(width, height);
        surface->Tick();

        std::vector<std::uint8_t> frame;
        std::int32_t version = -1;
        std::int32_t frameWidth = 0;
        std::int32_t frameHeight = 0;
        if (!surface->TakeFrame(
                frame,
                version,
                frameWidth,
                frameHeight))
        {
            return;
        }

        AndroidBitmapInfo info{};
        if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS
            || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888)
        {
            throw std::runtime_error(
                "Android launcher bitmap must be RGBA_8888"
            );
        }

        void* pixels = nullptr;
        if (AndroidBitmap_lockPixels(env, bitmap, &pixels)
                != ANDROID_BITMAP_RESULT_SUCCESS
            || pixels == nullptr)
        {
            throw std::runtime_error(
                "could not lock the Android launcher bitmap"
            );
        }

        try
        {
            const std::uint32_t rows = std::min<std::uint32_t>(
                info.height,
                static_cast<std::uint32_t>(
                    std::max(frameHeight, 0)
                )
            );
            const std::size_t rowBytes =
                static_cast<std::size_t>(
                    std::min<std::uint32_t>(
                        info.width,
                        static_cast<std::uint32_t>(
                            std::max(frameWidth, 0)
                        )
                    )
                ) * 4U;
            auto* destination =
                static_cast<std::uint8_t*>(pixels);
            for (std::uint32_t y = 0; y < rows; ++y)
            {
                std::memcpy(
                    destination
                        + static_cast<std::size_t>(y) * info.stride,
                    frame.data()
                        + static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(frameWidth) * 4U,
                    rowBytes
                );
            }
        }
        catch (...)
        {
            AndroidBitmap_unlockPixels(env, bitmap);
            throw;
        }
        AndroidBitmap_unlockPixels(env, bitmap);
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_LauncherView_nativeTouch(
    JNIEnv* env,
    jclass,
    jint action,
    jfloat x,
    jfloat y
)
{
    JniVoid(env, [&]()
    {
        const auto surface = MphRead::Droid::AndroidUiSurface::Current();
        if (surface == nullptr)
        {
            return;
        }
        switch (action)
        {
        case AMOTION_EVENT_ACTION_DOWN:
            surface->TouchDown(x, y);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            surface->TouchMove(x, y);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            surface->TouchUp(x, y);
            break;
        default:
            break;
        }
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeOnCheckIsTextEditor(
    JNIEnv* env,
    jclass,
    jlong peer
)
{
    return JniBoolean(env, [&]()
    {
        return RequirePeer<MphRead::Droid::GameView>(peer)
            .OnCheckIsTextEditor();
    });
}

extern "C"
JNIEXPORT jobject JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeOnCreateInputConnection(
    JNIEnv* env,
    jclass,
    jlong peer,
    jobject outAttrs
)
{
    return JniObject(env, [&]()
    {
        return RequirePeer<MphRead::Droid::GameView>(peer)
            .OnCreateInputConnection(env, outAttrs);
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeOnKeyDown(
    JNIEnv* env,
    jclass,
    jlong peer,
    jint keyCode,
    jobject event
)
{
    return JniBoolean(env, [&]()
    {
        return RequirePeer<MphRead::Droid::GameView>(peer)
            .OnKeyDown(env, keyCode, event);
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeOnKeyUp(
    JNIEnv* env,
    jclass,
    jlong peer,
    jint keyCode,
    jobject event
)
{
    return JniBoolean(env, [&]()
    {
        return RequirePeer<MphRead::Droid::GameView>(peer)
            .OnKeyUp(env, keyCode, event);
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeOnGenericMotionEvent(
    JNIEnv* env,
    jclass,
    jlong peer,
    jobject event
)
{
    return JniBoolean(env, [&]()
    {
        return RequirePeer<MphRead::Droid::GameView>(peer)
            .OnGenericMotionEvent(env, event);
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeSurfaceCreated(
    JNIEnv* env,
    jclass,
    jlong peer,
    jobject holder
)
{
    JniVoid(env, [&]()
    {
        RequirePeer<MphRead::Droid::GameView>(peer)
            .SurfaceCreated(env, holder);
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeSurfaceChanged(
    JNIEnv* env,
    jclass,
    jlong peer,
    jobject holder,
    jint format,
    jint width,
    jint height
)
{
    JniVoid(env, [&]()
    {
        RequirePeer<MphRead::Droid::GameView>(peer)
            .SurfaceChanged(env, holder, format, width, height);
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_GameSurfaceView_nativeSurfaceDestroyed(
    JNIEnv* env,
    jclass,
    jlong peer,
    jobject holder
)
{
    JniVoid(env, [&]()
    {
        RequirePeer<MphRead::Droid::GameView>(peer)
            .SurfaceDestroyed(env, holder);
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_TouchOverlayView_nativeOnSizeChanged(
    JNIEnv* env,
    jclass,
    jlong peer,
    jint width,
    jint height,
    jint oldWidth,
    jint oldHeight
)
{
    JniVoid(env, [&]()
    {
        RequirePeer<MphRead::Droid::TouchOverlayView>(peer)
            .OnSizeChanged(
                env,
                width,
                height,
                oldWidth,
                oldHeight
            );
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_TouchOverlayView_nativeOnDraw(
    JNIEnv* env,
    jclass,
    jlong peer,
    jobject canvas
)
{
    JniVoid(env, [&]()
    {
        RequirePeer<MphRead::Droid::TouchOverlayView>(peer)
            .OnDraw(env, canvas);
    });
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_fr_livetek_fruityprime_TouchOverlayView_nativeOnTouchEvent(
    JNIEnv* env,
    jclass,
    jlong peer,
    jobject event
)
{
    return JniBoolean(env, [&]()
    {
        return RequirePeer<MphRead::Droid::TouchOverlayView>(peer)
            .OnTouchEvent(env, event);
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_InstallResultReceiver_nativeBind(
    JNIEnv* env,
    jclass type
)
{
    JniVoid(env, [&]()
    {
        MphRead::Droid::InstallResultReceiver::JavaClass(env, type);
    });
}

extern "C"
JNIEXPORT void JNICALL
Java_fr_livetek_fruityprime_InstallResultReceiver_nativeOnReceive(
    JNIEnv* env,
    jclass,
    jobject context,
    jobject intent
)
{
    JniVoid(env, [&]()
    {
        MphRead::Droid::InstallResultReceiver::OnReceive(
            env,
            context,
            intent
        );
    });
}

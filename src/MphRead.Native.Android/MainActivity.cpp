#include "MainActivity.hpp"

#include "AndroidConsole.hpp"
#include "AndroidLogShare.hpp"
#include "AndroidMaps.hpp"
#include "AndroidPng.hpp"
#include "AndroidUpdateInstaller.hpp"
#include "OffscreenGl.hpp"

#include "../MphRead.Native/Mods/Input/GamepadInput.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/GameFiles.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"
#include "../MphRead.Native/Mods/LogShare.hpp"
#include "../MphRead.Native/Mods/Network/DemoPlayback.hpp"
#include "../MphRead.Native/Mods/Network/NetHostSession.hpp"
#include "../MphRead.Native/Mods/Network/NetSession.hpp"
#include "../MphRead.Native/Mods/ScreenCapture.hpp"
#include "../MphRead.Native/Mods/ThumbnailGenerator.hpp"
#include "../MphRead.Native/Mods/ThumbnailHost.hpp"
#include "../MphRead.Native/Mods/Update/UpdateInstall.hpp"

#include <bit>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr std::int32_t KeyActionDown = 0;
    constexpr std::int32_t KeyActionUp = 1;

    [[nodiscard]] std::int64_t UncheckedSubtract(
        std::int64_t left,
        std::int64_t right
    ) noexcept
    {
        return std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(left)
            - static_cast<std::uint64_t>(right)
        );
    }

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
                reinterpret_cast<void**>(&_env),
                JNI_VERSION_1_6
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

    void DeleteGlobalRefNoThrow(JavaVM* javaVm, jobject value) noexcept
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
            if (javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK)
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

    std::shared_ptr<MphRead::Droid::AndroidThumbnailHost> g_thumbnailHost{};

    void WriteConsoleLine(const std::string& line)
    {
        std::cout << line << '\n';
    }

    class AndroidAppOwnerBridge final : public MphRead::Droid::AndroidAppOwner
    {
    public:
        void AddFluentTheme(
            MphRead::Droid::AndroidApp& application
        ) override
        {
            MphRead::Droid::GetMainActivityOwner().AddFluentTheme(application);
        }

        void SetRequestedThemeVariantDark(
            MphRead::Droid::AndroidApp& application
        ) override
        {
            MphRead::Droid::GetMainActivityOwner()
                .SetRequestedThemeVariantDark(application);
        }

        void BaseInitialize(
            MphRead::Droid::AndroidApp& application
        ) override
        {
            MphRead::Droid::GetMainActivityOwner().BaseInitialize(application);
        }

        [[nodiscard]] MphRead::Droid::AndroidSingleViewLifetime
            SingleViewApplicationLifetime(
                MphRead::Droid::AndroidApp& application
            ) override
        {
            return MphRead::Droid::GetMainActivityOwner()
                .SingleViewApplicationLifetime(application);
        }

        [[nodiscard]] std::shared_ptr<
            MphRead::Mods::Launcher::Gui::HomeViewAdapter>
            CreateHomeViewAdapter() override
        {
            return MphRead::Droid::GetMainActivityOwner()
                .CreateHomeViewAdapter();
        }

        void SetSingleViewMainView(
            const MphRead::Droid::AndroidSingleViewLifetime& lifetime,
            MphRead::Mods::Launcher::Gui::HomeView& home,
            MphRead::Mods::Launcher::Gui::HomeViewAdapter& adapter
        ) override
        {
            MphRead::Droid::GetMainActivityOwner().SetSingleViewMainView(
                lifetime,
                home,
                adapter
            );
        }

        void BaseOnFrameworkInitializationCompleted(
            MphRead::Droid::AndroidApp& application
        ) override
        {
            MphRead::Droid::GetMainActivityOwner()
                .BaseOnFrameworkInitializationCompleted(application);
        }

        void FinishMainActivityIfPresent() override
        {
            if (MphRead::Droid::MainActivity* activity =
                    MphRead::Droid::MainActivity::Instance();
                activity != nullptr)
            {
                activity->Finish();
            }
        }

        void StartMatchIfMainActivityPresent(
            const MphRead::Mods::Launcher::LaunchPlan& plan
        ) override
        {
            if (MphRead::Droid::MainActivity* activity =
                    MphRead::Droid::MainActivity::Instance();
                activity != nullptr)
            {
                activity->StartMatch(plan);
            }
        }
    };

    class AndroidThumbnailHostOwnerBridge final
        : public MphRead::Droid::AndroidThumbnailHostOwner
    {
    public:
        [[nodiscard]] MphRead::Mods::ThumbnailTaskIntRef RenderPreviews(
            JNIEnv* env,
            jobject activity,
            MphRead::Mods::ThumbnailRoomsRef rooms,
            MphRead::Mods::ThumbnailReportRef report
        ) override
        {
            MphRead::Droid::MainActivity* peer =
                MphRead::Droid::GetMainActivityOwner()
                    .ResolveMainActivityPeer(env, activity);
            if (peer == nullptr)
            {
                MphRead::Droid::GetMainActivityOwner().ThrowNullReference();
            }
            return peer->RenderPreviews(rooms, report);
        }
    };
}

namespace MphRead::Droid
{
    std::atomic<MainActivity*> MainActivity::_instance{nullptr};

    AndroidAppOwner& GetAndroidAppOwner() noexcept
    {
        static AndroidAppOwnerBridge owner;
        return owner;
    }

    AndroidThumbnailHostOwner& GetAndroidThumbnailHostOwner() noexcept
    {
        static AndroidThumbnailHostOwnerBridge owner;
        return owner;
    }

    MainActivity::MainActivity(JNIEnv* env, jobject activity)
    {
        if (env == nullptr)
        {
            throw std::invalid_argument(
                "Android JNI environment must not be null"
            );
        }
        if (activity == nullptr)
        {
            throw std::invalid_argument(
                "Android Activity must not be null"
            );
        }
        if (env->GetJavaVM(&_javaVm) != JNI_OK || _javaVm == nullptr)
        {
            throw std::runtime_error("Android Java VM is not available");
        }

        _activity = env->NewGlobalRef(activity);
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            _activity = nullptr;
            throw std::runtime_error(
                "could not retain the Android Activity"
            );
        }
        if (_activity == nullptr)
        {
            throw std::bad_alloc();
        }
    }

    MainActivity::~MainActivity()
    {
        DeleteGlobalRefNoThrow(_javaVm, _activity);
    }

    MainActivity* MainActivity::Instance() noexcept
    {
        return _instance.load(std::memory_order_relaxed);
    }

    bool MainActivity::InMatch() const noexcept
    {
        return _gameView.load(std::memory_order_relaxed) != nullptr;
    }

    MainActivityAppBuilderRef MainActivity::CustomizeAppBuilder(
        MainActivityAppBuilderRef builder
    )
    {
        MainActivityOwner& owner = GetMainActivityOwner();

        AndroidConsole::Install();

        const std::string root = ChooseRoot();
        if (!root.empty())
        {
            MphRead::Mods::Launcher::LauncherPrefs::Directory(root);
            MphRead::Mods::Launcher::GameFiles::Root(root);
            try
            {
                owner.SetCurrentDirectory(root);
            }
            catch (...)
            {
                const std::exception_ptr error = std::current_exception();
                WriteConsoleLine(
                    "[android] could not use " + root
                    + " as the working directory: "
                    + owner.ExceptionMessage(error)
                );
            }
        }

        JNIEnv* env = CurrentEnv();
        const std::u16string root16 = owner.ToUtf16(root);
        AndroidMaps::Install(env, owner.Assets(env, *this), root16);

        auto thumbnailHost = std::make_shared<AndroidThumbnailHost>(
            env, _activity);
        MphRead::Mods::ThumbnailHost::Current(thumbnailHost.get());
        g_thumbnailHost = std::move(thumbnailHost);

        MphRead::Mods::Update::UpdateInstall::Current(
            std::make_shared<AndroidUpdateInstaller>(env, _activity)
        );

        MphRead::Mods::LogShare::Current(
            std::make_shared<AndroidLogShare>(env, _activity)
        );

        MphRead::Mods::ScreenCapture::PngWriter(AndroidPng::Write);

        MainActivityAppBuilderRef result =
            owner.BaseCustomizeAppBuilder(*this, builder);
        return owner.WithInterFont(result);
    }

    std::string MainActivity::ChooseRoot()
    {
        MainActivityOwner& owner = GetMainActivityOwner();
        std::vector<std::string> candidates;

        const std::optional<std::string> external =
            owner.ExternalFilesPath(*this);
        if (external.has_value() && !external->empty())
        {
            candidates.push_back(*external);
        }

        const std::optional<std::string> internal =
            owner.InternalFilesPath(*this);
        if (internal.has_value() && !internal->empty())
        {
            candidates.push_back(*internal);
        }

        for (const std::string& candidate : candidates)
        {
            if (Writable(candidate)
                && owner.FileExists(
                    owner.CombinePath(candidate, "paths.txt")
                ))
            {
                return candidate;
            }
        }

        for (const std::string& candidate : candidates)
        {
            if (Writable(candidate))
            {
                if (candidate != candidates[0])
                {
                    WriteConsoleLine(
                        "[android] " + candidates[0]
                        + " cannot be written to; using " + candidate
                    );
                }
                return candidate;
            }
        }

        return {};
    }

    bool MainActivity::Writable(std::string_view directory)
    {
        MainActivityOwner& owner = GetMainActivityOwner();
        try
        {
            owner.CreateDirectory(directory);
            const std::string probe =
                owner.CombinePath(directory, ".write-probe");
            owner.WriteEmptyFile(probe);
            owner.DeleteFile(probe);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void MainActivity::OnCreate(jobject savedInstanceState)
    {
        _instance.store(this, std::memory_order_relaxed);

        MainActivityOwner& owner = GetMainActivityOwner();
        owner.BaseOnCreate(*this, savedInstanceState);

        owner.RunBackground([]()
        {
            AndroidMaps::EnsureBuilt();
        });

        _content = owner.ContentViewGroup(*this);
        _launcherView = _content
            ? owner.ChildAt(_content, 0)
            : MainActivityObjectRef{};
    }

    MphRead::Mods::ThumbnailTaskIntRef MainActivity::RenderPreviews(
        MphRead::Mods::ThumbnailRoomsRef rooms,
        MphRead::Mods::ThumbnailReportRef report
    )
    {
        MainActivityOwner& owner = GetMainActivityOwner();

        if (owner.ThumbnailRoomsCount(rooms) == 0
            || _renderingPreviews.load(std::memory_order_acquire))
        {
            return owner.CompletedIntTask(0);
        }

        _renderingPreviews.store(true, std::memory_order_release);

        auto reportOnUi = [this, report](const std::string& line)
        {
            GetMainActivityOwner().RunOnUiThread(
                [report, line]()
                {
                    GetMainActivityOwner().ThumbnailReport(report, line);
                }
            );
        };

        owner.RunOnUiThread([this]()
        {
            GetMainActivityOwner().AddKeepScreenOn(*this);
        });

        return owner.RunBackgroundInt(
            [this, rooms, reportOnUi]() -> std::int32_t
            {
                std::int32_t result = 0;
                std::exception_ptr escaped{};

                try
                {
                    try
                    {
                        MphRead::Mods::ThumbnailGenerator::
                            EnsureCacheDirectory();

                        MainActivityOwner& workerOwner =
                            GetMainActivityOwner();

                        const std::size_t initialCount =
                            workerOwner.ThumbnailRoomsCount(rooms);
                        std::vector<std::string> workerRooms;
                        workerRooms.reserve(initialCount);
                        for (std::size_t index = 0;
                            index < initialCount;
                            ++index)
                        {
                            workerRooms.push_back(
                                workerOwner.ThumbnailRoomAt(rooms, index)
                            );
                        }

                        ScopedJniEnv scopedEnv(_javaVm);
                        std::int32_t written = PreviewWorkers::Run(
                            scopedEnv.Get(),
                            _activity,
                            workerRooms,
                            PreviewWidth,
                            PreviewHeight,
                            reportOnUi
                        );

                        std::vector<std::string> left;
                        for (std::size_t index = 0;
                            index
                                < workerOwner.ThumbnailRoomsCount(rooms);
                            ++index)
                        {
                            const std::string room =
                                workerOwner.ThumbnailRoomAt(rooms, index);
                            if (!MphRead::Mods::ThumbnailGenerator::
                                    Exists(room))
                            {
                                left.push_back(room);
                            }
                        }

                        if (!left.empty())
                        {
                            written += RenderHere(left, reportOnUi);
                        }

                        result = written;
                    }
                    catch (...)
                    {
                        const std::exception_ptr error =
                            std::current_exception();
                        MainActivityOwner& workerOwner =
                            GetMainActivityOwner();
                        WriteConsoleLine(
                            "[thumbnails] the run failed: "
                            + workerOwner.ExceptionToString(error)
                        );
                        reportOnUi(
                            "[thumbnails] "
                            + workerOwner.ExceptionMessage(error)
                        );
                        result = 0;
                    }
                }
                catch (...)
                {
                    escaped = std::current_exception();
                }

                _renderingPreviews.store(
                    false,
                    std::memory_order_release
                );

                try
                {
                    GetMainActivityOwner().RunOnUiThread([this]()
                    {
                        if (!InMatch())
                        {
                            GetMainActivityOwner()
                                .ClearKeepScreenOn(*this);
                        }
                    });
                }
                catch (...)
                {
                    throw;
                }

                if (escaped)
                {
                    std::rethrow_exception(escaped);
                }

                return result;
            }
        );
    }

    std::int32_t MainActivity::RenderHere(
        const std::vector<std::string>& rooms,
        const std::function<void(const std::string&)>& report
    )
    {
        if (InMatch())
        {
            report("[thumbnails] not while a match is running");
            return 0;
        }

        _renderingHere.store(true, std::memory_order_release);

        std::int32_t result = 0;
        std::exception_ptr escaped{};

        try
        {
            try
            {
                std::shared_ptr<OffscreenGl> gl =
                    OffscreenGl::Create(PreviewWidth, PreviewHeight);

                std::int32_t rendered = 0;
                std::exception_ptr renderError{};
                try
                {
                    rendered = GetMainActivityOwner().PreviewRunRender(
                        rooms,
                        PreviewWidth,
                        PreviewHeight,
                        report
                    );
                }
                catch (...)
                {
                    renderError = std::current_exception();
                }

                if (gl)
                {
                    gl->Dispose();
                }

                if (renderError)
                {
                    std::rethrow_exception(renderError);
                }

                result = rendered;
            }
            catch (...)
            {
                const std::exception_ptr error =
                    std::current_exception();
                MainActivityOwner& owner = GetMainActivityOwner();
                WriteConsoleLine(
                    "[thumbnails] the offscreen context failed: "
                    + owner.ExceptionToString(error)
                );
                report(
                    "[thumbnails] "
                    + owner.ExceptionMessage(error)
                );
                result = 0;
            }
        }
        catch (...)
        {
            escaped = std::current_exception();
        }

        _renderingHere.store(false, std::memory_order_release);

        if (escaped)
        {
            std::rethrow_exception(escaped);
        }

        return result;
    }

    std::int32_t MainActivity::CurrentRotation()
    {
        try
        {
            MainActivityOwner& owner = GetMainActivityOwner();
            if (owner.IsAndroidVersionAtLeast30())
            {
                return owner.DisplayRotation(*this);
            }
            return owner.DefaultDisplayRotation(*this);
        }
        catch (...)
        {
            return -1;
        }
    }

    void MainActivity::OnDisplayAdded(std::int32_t displayId)
    {
        (void)displayId;
    }

    void MainActivity::OnDisplayRemoved(std::int32_t displayId)
    {
        (void)displayId;
    }

    void MainActivity::OnDisplayChanged(std::int32_t displayId)
    {
        (void)displayId;

        const std::int32_t rotation = CurrentRotation();
        if (rotation < 0 || rotation == _lastRotation)
        {
            return;
        }

        const std::int32_t before = _lastRotation;
        _lastRotation = rotation;

        if (before < 0)
        {
            return;
        }

        MainActivityOwner& owner = GetMainActivityOwner();
        const std::string beforeText =
            owner.FormatInt32Current(before);
        const std::string rotationText =
            owner.FormatInt32Current(rotation);
        const std::string widthText =
            owner.FormatInt32Current(ContentSize().Width);
        const std::string heightText =
            owner.FormatInt32Current(ContentSize().Height);
        WriteConsoleLine(
            "[android] display rotation "
            + beforeText
            + " -> "
            + rotationText
            + " at "
            + widthText
            + "x"
            + heightText
        );

        AfterRotation();

        if (_content)
        {
            owner.PostDelayed(
                _content,
                [this]()
                {
                    AfterRotation();
                },
                SettleMs
            );
        }
    }

    void MainActivity::AfterRotation()
    {
        _controls.ReleaseEverything();

        MainActivityOwner& owner = GetMainActivityOwner();
        if (_overlay)
        {
            owner.TouchOverlayRefresh(_overlay);
        }
        if (_content)
        {
            owner.RequestLayout(_content);
            owner.Invalidate(_content);
        }
        owner.RequestDecorLayout(*this);

        GoImmersive(true);
    }

    void MainActivity::OnConfigurationChanged(jobject newConfig)
    {
        GetMainActivityOwner().BaseOnConfigurationChanged(
            *this, newConfig);
        OnDisplayChanged(0);
    }

    void MainActivity::OnPause()
    {
        MainActivityOwner& owner = GetMainActivityOwner();

        if (_displays)
        {
            owner.UnregisterDisplayListener(_displays, *this);
            _displays = {};
        }

        if (MainActivityObjectRef gameView = LoadGameView();
            gameView)
        {
            owner.GameViewOnPause(gameView);
        }

        owner.BaseOnPause(*this);
    }

    void MainActivity::OnResume()
    {
        MainActivityOwner& owner = GetMainActivityOwner();

        owner.BaseOnResume(*this);

        GoImmersive(true);
        _lastRotation = CurrentRotation();

        if (!_displays)
        {
            MainActivityObjectRef manager =
                owner.DisplayManager(*this);
            if (manager)
            {
                _displays = manager;
                owner.RegisterDisplayListener(_displays, *this);
            }
        }

        if (MainActivityObjectRef gameView = LoadGameView();
            gameView)
        {
            owner.GameViewOnResume(gameView);
        }
    }

    bool MainActivity::DispatchKeyEvent(jobject event)
    {
        MainActivityOwner& owner = GetMainActivityOwner();

        const bool down = event != nullptr
            && owner.KeyEventAction(event) == KeyActionDown;

        if (event != nullptr
            && (down || owner.KeyEventAction(event) == KeyActionUp))
        {
            JNIEnv* env = CurrentEnv();
            if (GamepadBridge::HandleKey(
                    owner.KeyEventKeyCode(event),
                    event,
                    down,
                    env))
            {
                if (down)
                {
                    _controls.NotePadActivity();
                }
                return true;
            }
        }

        return owner.BaseDispatchKeyEvent(*this, event);
    }

    bool MainActivity::DispatchGenericMotionEvent(jobject event)
    {
        JNIEnv* env = event == nullptr ? nullptr : CurrentEnv();
        if (GamepadBridge::HandleMotion(event, env))
        {
            if (MphRead::Mods::Input::GamepadInput::InUse())
            {
                _controls.NotePadActivity();
            }
            return true;
        }

        return GetMainActivityOwner()
            .BaseDispatchGenericMotionEvent(*this, event);
    }

    void MainActivity::OnWindowFocusChanged(bool hasFocus)
    {
        GetMainActivityOwner().BaseOnWindowFocusChanged(
            *this, hasFocus);
        if (hasFocus)
        {
            GoImmersive(true);
        }
    }

    void MainActivity::OnDestroy()
    {
        if (_instance.load(std::memory_order_relaxed) == this)
        {
            _instance.store(nullptr, std::memory_order_relaxed);
        }

        if (MainActivityObjectRef gameView = LoadGameView();
            gameView)
        {
            GetMainActivityOwner().GameViewStop(gameView);
        }

        GetMainActivityOwner().BaseOnDestroy(*this);
    }

    void MainActivity::OnBackPressed()
    {
        if (InMatch())
        {
            TogglePauseMenu();
            return;
        }

        if (_pending)
        {
            CancelPending("the player went back");
            return;
        }

        if (std::shared_ptr<MphRead::Mods::Launcher::Gui::HomeView> home =
                AndroidApp::Home();
            home && home->GoBack())
        {
            return;
        }

        GetMainActivityOwner().BaseOnBackPressed(*this);
    }

    void MainActivity::StartMatch(
        const MphRead::Mods::Launcher::LaunchPlan& plan
    )
    {
        if (!_content || InMatch())
        {
            return;
        }

        if (_pending)
        {
            WriteConsoleLine(
                "[android] a match is already starting; ignoring"
            );
            return;
        }

        if (_renderingHere.load(std::memory_order_acquire))
        {
            GetMainActivityOwner().ToastLong(
                *this,
                "Still rendering map previews; try again in a moment."
            );
            if (std::shared_ptr<
                    MphRead::Mods::Launcher::Gui::HomeView> home =
                    AndroidApp::Home();
                home)
            {
                home->Reset();
            }
            return;
        }

        std::shared_ptr<AndroidInput> input =
            std::make_shared<AndroidInput>();

        _controls.ReleaseEverything();
        _controls.SetSpectator(false, false);

        MainActivityOwner& owner = GetMainActivityOwner();
        _orientationBefore = owner.RequestedOrientation(*this);
        owner.RequestedOrientation(
            *this,
            MainActivityOrientation::SensorLandscape
        );
        owner.AddKeepScreenOn(*this);
        GoImmersive(true);

        _pending = std::make_unique<PendingMatch>(
            PendingMatch{plan, input}
        );

        _waitingSince = owner.UptimeMillis();
        _lastSize = ContentSize();
        _sizeSettledAt = _waitingSince;

        const std::string& roomKey = RequireRoomKey(plan);
        ShowNotice(
            !roomKey.empty()
                ? "Loading " + roomKey + "..."
                : "Loading your game..."
        );

        const std::string startWidth =
            owner.FormatInt32Current(_lastSize.Width);
        const std::string startHeight =
            owner.FormatInt32Current(_lastSize.Height);
        WriteConsoleLine(
            "[android] starting "
            + roomKey
            + " from "
            + startWidth
            + "x"
            + startHeight
        );

        WaitForSteadyWindow();
    }

    MainActivitySize MainActivity::ContentSize() const
    {
        if (!_content)
        {
            return {};
        }
        return GetMainActivityOwner().ViewSize(_content);
    }

    MainActivityObjectRef MainActivity::LoadGameView() const noexcept
    {
        return MainActivityObjectRef{
            _gameView.load(std::memory_order_relaxed)
        };
    }

    void MainActivity::StoreGameView(
        MainActivityObjectRef value
    ) noexcept
    {
        _gameView.store(
            std::move(value.Native),
            std::memory_order_relaxed
        );
    }

    void MainActivity::WaitForSteadyWindow()
    {
        if (!_pending || !_content || InMatch())
        {
            return;
        }

        MainActivityOwner& owner = GetMainActivityOwner();
        const std::int64_t now = owner.UptimeMillis();
        const MainActivitySize size = ContentSize();

        if (!(size == _lastSize))
        {
            _lastSize = size;
            _sizeSettledAt = now;
        }

        const bool haveSize =
            size.Width > 0 && size.Height > 0;
        const bool landscape =
            size.Width > size.Height;
        const bool steady =
            haveSize
            && UncheckedSubtract(now, _sizeSettledAt) >= SettleMs;
        const std::int64_t waited =
            UncheckedSubtract(now, _waitingSince);

        if (steady && landscape)
        {
            StartPending(std::nullopt);
            return;
        }

        if (haveSize && waited >= RotateMs)
        {
            StartPending(
                landscape
                    ? std::optional<std::string>(
                        "the window was still moving after "
                        + owner.FormatInt64Current(waited)
                        + " ms")
                    : std::optional<std::string>(
                        "the display did not turn landscape in "
                        + owner.FormatInt64Current(waited)
                        + " ms")
            );
            return;
        }

        if (waited >= GiveUpMs)
        {
            CancelPending(
                "the window never took a size ("
                + owner.FormatInt32Current(size.Width)
                + "x"
                + owner.FormatInt32Current(size.Height)
                + ")"
            );
            return;
        }

        owner.PostDelayed(
            _content,
            [this]()
            {
                WaitForSteadyWindow();
            },
            50
        );
    }

    void MainActivity::CancelPending(std::string reason)
    {
        if (!_pending)
        {
            return;
        }

        WriteConsoleLine(
            "[android] the match was not started: " + reason
        );

        _pending.reset();
        HideNotice();
        _controls.ReleaseEverything();

        MainActivityOwner& owner = GetMainActivityOwner();
        if (_launcherView)
        {
            owner.SetViewVisible(_launcherView);
        }

        if (std::shared_ptr<
                MphRead::Mods::Launcher::Gui::HomeView> home =
                AndroidApp::Home();
            home)
        {
            home->Reset();
        }

        owner.ClearKeepScreenOn(*this);
        GoImmersive(true);
        owner.RequestedOrientation(*this, _orientationBefore);

        owner.ToastLong(
            *this,
            "Could not start the match: " + reason
        );
    }

    void MainActivity::StartPending(
        std::optional<std::string> note
    )
    {
        if (!_pending || InMatch())
        {
            return;
        }

        std::unique_ptr<PendingMatch> pending =
            std::move(_pending);

        MainActivityOwner& owner = GetMainActivityOwner();
        if (_launcherView)
        {
            owner.SetViewGone(_launcherView);
        }

        if (note.has_value())
        {
            WriteConsoleLine(
                "[android] starting the match anyway: " + *note
            );
        }

        const std::string widthText =
            owner.FormatInt32Current(ContentSize().Width);
        const std::string heightText =
            owner.FormatInt32Current(ContentSize().Height);
        WriteConsoleLine(
            "[android] building the match at "
            + widthText
            + "x"
            + heightText
        );

        owner.RequestedOrientation(
            *this,
            MainActivityOrientation::Locked
        );

        BeginMatch(pending->Plan, pending->Input);
    }

    void MainActivity::ShowSoftKeyboard(bool show)
    {
        MainActivityObjectRef gameView = LoadGameView();
        if (!gameView)
        {
            return;
        }

        MainActivityOwner& owner = GetMainActivityOwner();
        MainActivityObjectRef ime =
            owner.InputMethodManager(*this);
        if (!ime)
        {
            return;
        }

        if (show)
        {
            owner.GameViewRequestFocus(gameView);
            owner.ShowSoftInput(ime, gameView);
        }
        else
        {
            owner.HideSoftInput(ime, gameView);
        }
    }

    void MainActivity::BeginMatch(
        const MphRead::Mods::Launcher::LaunchPlan& plan,
        std::shared_ptr<AndroidInput> input
    )
    {
        if (!_content)
        {
            return;
        }

        MainActivityOwner& owner = GetMainActivityOwner();

        MainActivityObjectRef gameView =
            owner.CreateGameView(
                *this,
                _controls,
                std::move(input),
                plan,
                [this]()
                {
                    GetMainActivityOwner().RunOnUiThread(
                        [this]()
                        {
                            EndMatch();
                        }
                    );
                },
                [this]()
                {
                    GetMainActivityOwner().RunOnUiThread(
                        [this]()
                        {
                            EndMatch();
                        }
                    );
                },
                [this]()
                {
                    GetMainActivityOwner().RunOnUiThread(
                        [this]()
                        {
                            MatchLoaded();
                        }
                    );
                },
                [this](std::string error)
                {
                    GetMainActivityOwner().RunOnUiThread(
                        [this, error = std::move(error)]() mutable
                        {
                            FailMatch(std::move(error));
                        }
                    );
                },
                [this]()
                {
                    GetMainActivityOwner().RunOnUiThread(
                        [this]()
                        {
                            TogglePauseMenu();
                        }
                    );
                },
                [this](bool show)
                {
                    GetMainActivityOwner().RunOnUiThread(
                        [this, show]()
                        {
                            ShowSoftKeyboard(show);
                        }
                    );
                }
            );

        StoreGameView(gameView);

        owner.GameViewSetZOrderMediaOverlay(
            gameView, true);

        _overlay = owner.CreateTouchOverlayView(
            *this, _controls);

        owner.AddView(_content, gameView);
        owner.AddView(_content, _overlay);

        ShowNotice(
            "Loading " + RequireRoomKey(plan) + "..."
        );
        if (_notice)
        {
            owner.BringToFront(_notice);
        }
    }

    void MainActivity::ShowNotice(std::string text)
    {
        if (!_content)
        {
            return;
        }

        MainActivityOwner& owner = GetMainActivityOwner();
        if (_notice)
        {
            owner.SetNoticeText(_notice, std::move(text));
            return;
        }

        _notice = owner.CreateNoticeTextView(
            *this,
            std::move(text),
            0xE6E6EAF2u,
            0xFF0A0C10u
        );
        owner.AddView(_content, _notice);
    }

    void MainActivity::MatchLoaded()
    {
        HideNotice();
        GetMainActivityOwner().RequestedOrientation(
            *this,
            MainActivityOrientation::SensorLandscape
        );
    }

    void MainActivity::HideNotice()
    {
        if (_notice && _content)
        {
            GetMainActivityOwner().RemoveView(
                _content, _notice);
            _notice = {};
        }
    }

    void MainActivity::FailMatch(std::string message)
    {
        MainActivityOwner& owner = GetMainActivityOwner();

        if (_notice)
        {
            owner.SetNoticeText(_notice, message);
        }
        else
        {
            owner.ToastLong(*this, message);
        }

        if (_content)
        {
            owner.PostDelayed(
                _content,
                [this]()
                {
                    EndMatch();
                },
                4000
            );
        }
    }

    void MainActivity::TogglePauseMenu()
    {
        if (!InMatch())
        {
            return;
        }

        if (_pauseMenuOpen)
        {
            ClosePauseMenu();
            return;
        }

        _pauseMenuOpen = true;
        _controls.ReleaseEverything();

        MainActivityOwner& owner = GetMainActivityOwner();
        if (_overlay)
        {
            owner.SetViewGone(_overlay);
        }
        if (MainActivityObjectRef gameView = LoadGameView();
            gameView)
        {
            owner.SetViewGone(gameView);
        }
        if (_launcherView)
        {
            owner.SetViewVisible(_launcherView);
        }

        GoImmersive(true);

        if (std::shared_ptr<
                MphRead::Mods::Launcher::Gui::HomeView> home =
                AndroidApp::Home();
            home)
        {
            home->ShowPauseMenu(
                [this]()
                {
                    ClosePauseMenu();
                },
                [this]()
                {
                    EndMatch();
                },
                [this]()
                {
                    Finish();
                }
            );
        }
    }

    void MainActivity::ClosePauseMenu()
    {
        if (!_pauseMenuOpen)
        {
            return;
        }

        _pauseMenuOpen = false;
        MainActivityOwner& owner = GetMainActivityOwner();

        if (_launcherView)
        {
            owner.SetViewGone(_launcherView);
        }
        if (MainActivityObjectRef gameView = LoadGameView();
            gameView)
        {
            owner.SetViewVisible(gameView);
        }
        if (_overlay)
        {
            owner.SetViewVisible(_overlay);
        }

        _controls.ReleaseEverything();
        _controls.ReloadSettings();
        GoImmersive(true);
    }

    void MainActivity::EndMatch()
    {
        if (!_content)
        {
            return;
        }

        _pending.reset();
        _pauseMenuOpen = false;
        HideNotice();

        MainActivityOwner& owner = GetMainActivityOwner();

        if (_overlay)
        {
            owner.RemoveView(_content, _overlay);
            _overlay = {};
        }

        if (MainActivityObjectRef gameView = LoadGameView();
            gameView)
        {
            owner.GameViewStop(gameView);
            owner.RemoveView(_content, gameView);
            StoreGameView({});
        }

        _controls.ReleaseEverything();
        _controls.SetSpectator(false, false);

        if (_launcherView)
        {
            owner.SetViewVisible(_launcherView);
        }

        if (std::shared_ptr<
                MphRead::Mods::Launcher::Gui::HomeView> home =
                AndroidApp::Home();
            home)
        {
            home->Reset();
        }

        MphRead::Mods::Network::NetSession::Stop();
        MphRead::Mods::Network::NetHostSession::Stop();
        MphRead::Mods::Network::DemoPlayback::Stop();

        owner.ClearKeepScreenOn(*this);
        GoImmersive(true);
        owner.RequestedOrientation(*this, _orientationBefore);
    }

    void MainActivity::GoImmersive(bool immersive)
    {
        MainActivityOwner& owner = GetMainActivityOwner();
        MainActivityObjectRef window = owner.Window(*this);
        if (!window)
        {
            return;
        }

        if (owner.IsAndroidVersionAtLeast30())
        {
            MainActivityObjectRef controller =
                owner.WindowInsetsController(window);
            if (controller)
            {
                if (immersive)
                {
                    owner.SetTransientBarsBySwipe(controller);
                    owner.HideSystemBars(controller);
                }
                else
                {
                    owner.ShowSystemBars(controller);
                }
            }
            return;
        }

        owner.SetLegacySystemUiVisibility(window, immersive);
    }

    void MainActivity::Finish()
    {
        GetMainActivityOwner().Finish(*this);
    }

    const std::string& MainActivity::RequireRoomKey(
        const MphRead::Mods::Launcher::LaunchPlan& plan
    ) const
    {
        const std::optional<std::string>& roomKey =
            plan.RoomKey();
        if (!roomKey.has_value())
        {
            GetMainActivityOwner().ThrowNullReference();
        }
        return *roomKey;
    }

    JNIEnv* MainActivity::CurrentEnv() const
    {
        if (_javaVm == nullptr)
        {
            throw std::runtime_error(
                "Android Java VM is not available"
            );
        }

        JNIEnv* env = nullptr;
        const jint result = _javaVm->GetEnv(
            reinterpret_cast<void**>(&env),
            JNI_VERSION_1_6
        );
        if (result != JNI_OK || env == nullptr)
        {
            throw std::runtime_error(
                "the current thread is not attached to the Android Java VM"
            );
        }
        return env;
    }
}

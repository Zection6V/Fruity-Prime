#pragma once

#if !defined(__ANDROID__)
#error "MainActivity is only valid for the Android native target."
#endif

#include "AndroidApp.hpp"
#include "AndroidInput.hpp"
#include "AndroidThumbnails.hpp"
#include "TouchControls.hpp"

#include "../MphRead.Native/Mods/Launcher/Portable/LaunchPlan.hpp"
#include "../MphRead.Native/Mods/ThumbnailHost.hpp"

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <jni.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Droid
{
    class MainActivity;

    struct MainActivityAppBuilderRef final
    {
        void* Native = nullptr;
    };

    struct MainActivityObjectRef final
    {
        std::shared_ptr<void> Native{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return Native != nullptr;
        }

        [[nodiscard]] const void* Identity() const noexcept
        {
            return Native.get();
        }
    };

    struct MainActivitySize final
    {
        std::int32_t Width = 0;
        std::int32_t Height = 0;

        friend bool operator==(const MainActivitySize&, const MainActivitySize&) = default;
    };

    enum class MainActivityOrientation : std::int32_t
    {
        Unspecified = -1,
        SensorLandscape = 6,
        Locked = 14
    };

    // MainActivity.cs is an AvaloniaMainActivity<AndroidApp> and Android owns
    // that Activity object, its Window/View tree, lifecycle callbacks and the
    // UI looper. This interface is only the mechanical framework/runtime
    // surface those two native files cannot own. All choices, ordering and
    // state transitions from MainActivity.cs remain in MainActivity below.
    class MainActivityOwner
    {
    public:
        using Action = std::function<void()>;
        using ErrorAction = std::function<void(std::string)>;
        using BoolAction = std::function<void(bool)>;

        virtual ~MainActivityOwner() = default;

        // AndroidApp's unavoidable Avalonia owner. MainActivity.cpp supplies
        // AndroidAppOwner itself so Finish/StartMatch still use the exact
        // MainActivity.Instance lookup from AndroidApp.cs.
        virtual void AddFluentTheme(AndroidApp& application) = 0;
        virtual void SetRequestedThemeVariantDark(AndroidApp& application) = 0;
        virtual void BaseInitialize(AndroidApp& application) = 0;
        [[nodiscard]] virtual AndroidSingleViewLifetime
            SingleViewApplicationLifetime(AndroidApp& application) = 0;
        [[nodiscard]] virtual std::shared_ptr<
            MphRead::Mods::Launcher::Gui::HomeViewAdapter>
            CreateHomeViewAdapter() = 0;
        virtual void SetSingleViewMainView(
            const AndroidSingleViewLifetime& lifetime,
            MphRead::Mods::Launcher::Gui::HomeView& home,
            MphRead::Mods::Launcher::Gui::HomeViewAdapter& adapter) = 0;
        virtual void BaseOnFrameworkInitializationCompleted(
            AndroidApp& application) = 0;

        // AvaloniaMainActivity<AndroidApp>.CustomizeAppBuilder and Inter.
        [[nodiscard]] virtual MainActivityAppBuilderRef BaseCustomizeAppBuilder(
            MainActivity& activity,
            MainActivityAppBuilderRef builder) = 0;
        [[nodiscard]] virtual MainActivityAppBuilderRef WithInterFont(
            MainActivityAppBuilderRef builder) = 0;

        // Android Context/Application filesystem/resource calls. String/path
        // conversion and System.IO calls stay behind the runtime owner so this
        // pair does not substitute std::filesystem semantics for .NET.
        [[nodiscard]] virtual std::optional<std::string>
            ExternalFilesPath(MainActivity& activity) = 0;
        [[nodiscard]] virtual std::optional<std::string>
            InternalFilesPath(MainActivity& activity) = 0;
        virtual void CreateDirectory(std::string_view path) = 0;
        [[nodiscard]] virtual std::string CombinePath(
            std::string_view left, std::string_view right) = 0;
        virtual void WriteEmptyFile(std::string_view path) = 0;
        virtual void DeleteFile(std::string_view path) = 0;
        [[nodiscard]] virtual bool FileExists(std::string_view path) = 0;
        virtual void SetCurrentDirectory(std::string_view path) = 0;
        [[nodiscard]] virtual std::u16string ToUtf16(std::string_view value) = 0;
        [[nodiscard]] virtual jobject Assets(
            JNIEnv* env, MainActivity& activity) = 0;

        // CLR exception/culture surfaces used by interpolation and catch
        // blocks. The implementation must describe the current exception using
        // System.Exception.ToString()/Message equivalents.
        [[nodiscard]] virtual std::string ExceptionToString(
            std::exception_ptr error) = 0;
        [[nodiscard]] virtual std::string ExceptionMessage(
            std::exception_ptr error) = 0;
        [[nodiscard]] virtual std::string FormatInt32Current(
            std::int32_t value) = 0;
        [[nodiscard]] virtual std::string FormatInt64Current(
            std::int64_t value) = 0;
        [[noreturn]] virtual void ThrowNullReference() = 0;

        // Task/dispatcher mechanics. RunBackgroundInt must preserve Task.Run
        // result/exception completion and keep captured opaque references alive.
        virtual void RunBackground(Action action) = 0;
        virtual void RunOnUiThread(Action action) = 0;
        [[nodiscard]] virtual MphRead::Mods::ThumbnailTaskIntRef
            CompletedIntTask(std::int32_t result) = 0;
        [[nodiscard]] virtual MphRead::Mods::ThumbnailTaskIntRef
            RunBackgroundInt(std::function<std::int32_t()> action) = 0;
        [[nodiscard]] virtual std::size_t ThumbnailRoomsCount(
            MphRead::Mods::ThumbnailRoomsRef rooms) = 0;
        [[nodiscard]] virtual std::string ThumbnailRoomAt(
            MphRead::Mods::ThumbnailRoomsRef rooms,
            std::size_t index) = 0;
        virtual void ThumbnailReport(
            MphRead::Mods::ThumbnailReportRef report,
            std::string line) = 0;

        // PreviewRun is a separate migration owner. This is a call-through
        // only; it must invoke PreviewRun.Render and must not implement preview
        // policy here or in the Android host.
        [[nodiscard]] virtual std::int32_t PreviewRunRender(
            const std::vector<std::string>& rooms,
            std::int32_t width,
            std::int32_t height,
            const std::function<void(const std::string&)>& report) = 0;

        // Base Activity lifecycle/dispatch callbacks.
        virtual void BaseOnCreate(MainActivity& activity, jobject savedState) = 0;
        virtual void BaseOnConfigurationChanged(
            MainActivity& activity, jobject configuration) = 0;
        virtual void BaseOnPause(MainActivity& activity) = 0;
        virtual void BaseOnResume(MainActivity& activity) = 0;
        virtual void BaseOnWindowFocusChanged(
            MainActivity& activity, bool hasFocus) = 0;
        virtual void BaseOnDestroy(MainActivity& activity) = 0;
        virtual void BaseOnBackPressed(MainActivity& activity) = 0;
        [[nodiscard]] virtual bool BaseDispatchKeyEvent(
            MainActivity& activity, jobject event) = 0;
        [[nodiscard]] virtual bool BaseDispatchGenericMotionEvent(
            MainActivity& activity, jobject event) = 0;
        virtual void Finish(MainActivity& activity) = 0;

        // Content/View mechanics.
        [[nodiscard]] virtual MainActivityObjectRef ContentViewGroup(
            MainActivity& activity) = 0;
        [[nodiscard]] virtual MainActivityObjectRef ChildAt(
            const MainActivityObjectRef& parent, std::int32_t index) = 0;
        [[nodiscard]] virtual MainActivitySize ViewSize(
            const MainActivityObjectRef& view) = 0;
        virtual void SetViewVisible(const MainActivityObjectRef& view) = 0;
        virtual void SetViewGone(const MainActivityObjectRef& view) = 0;
        virtual void AddView(
            const MainActivityObjectRef& parent,
            const MainActivityObjectRef& child) = 0;
        virtual void RemoveView(
            const MainActivityObjectRef& parent,
            const MainActivityObjectRef& child) = 0;
        virtual void BringToFront(const MainActivityObjectRef& view) = 0;
        virtual void RequestLayout(const MainActivityObjectRef& view) = 0;
        virtual void Invalidate(const MainActivityObjectRef& view) = 0;
        virtual void RequestDecorLayout(MainActivity& activity) = 0;
        virtual void PostDelayed(
            const MainActivityObjectRef& view,
            Action action,
            std::int64_t milliseconds) = 0;

        // The concrete SurfaceView/View subclasses are framework owners.
        // CreateGameView is a mechanical constructor/wiring adapter for the
        // exact GameView constructor in MainActivity.cs; it must route its
        // build callback to AndroidMatch.Build rather than duplicate it.
        // onBuildClose and onEnd are deliberately distinct: the C# source
        // creates a close callback inside the build lambda and a separate
        // GameView onEnd callback even though both ultimately post EndMatch.
        [[nodiscard]] virtual MainActivityObjectRef CreateGameView(
            MainActivity& activity,
            TouchControls& controls,
            std::shared_ptr<AndroidInput> input,
            const MphRead::Mods::Launcher::LaunchPlan& plan,
            Action onBuildClose,
            Action onEnd,
            Action onLoaded,
            ErrorAction onError,
            Action onPauseMenu,
            BoolAction onSoftKeyboard) = 0;
        virtual void GameViewSetZOrderMediaOverlay(
            const MainActivityObjectRef& gameView, bool value) = 0;
        virtual void GameViewOnPause(
            const MainActivityObjectRef& gameView) = 0;
        virtual void GameViewOnResume(
            const MainActivityObjectRef& gameView) = 0;
        virtual void GameViewStop(
            const MainActivityObjectRef& gameView) = 0;
        virtual void GameViewRequestFocus(
            const MainActivityObjectRef& gameView) = 0;
        [[nodiscard]] virtual MainActivityObjectRef InputMethodManager(
            MainActivity& activity) = 0;
        virtual void ShowSoftInput(
            const MainActivityObjectRef& inputMethodManager,
            const MainActivityObjectRef& gameView) = 0;
        virtual void HideSoftInput(
            const MainActivityObjectRef& inputMethodManager,
            const MainActivityObjectRef& gameView) = 0;

        [[nodiscard]] virtual MainActivityObjectRef CreateTouchOverlayView(
            MainActivity& activity,
            TouchControls& controls) = 0;
        virtual void TouchOverlayRefresh(
            const MainActivityObjectRef& overlay) = 0;

        // Must construct the exact TextView shape from MainActivity.cs:
        // TextAlignment.Center and Gravity.Center, with the supplied text and
        // ARGB foreground/background values.
        [[nodiscard]] virtual MainActivityObjectRef CreateNoticeTextView(
            MainActivity& activity,
            std::string text,
            std::uint32_t textArgb,
            std::uint32_t backgroundArgb) = 0;
        virtual void SetNoticeText(
            const MainActivityObjectRef& notice,
            std::string text) = 0;
        virtual void ToastLong(MainActivity& activity, std::string text) = 0;

        // Window/orientation/display/input methods.
        [[nodiscard]] virtual MainActivityObjectRef Window(
            MainActivity& activity) = 0;
        virtual void AddKeepScreenOn(MainActivity& activity) = 0;
        virtual void ClearKeepScreenOn(MainActivity& activity) = 0;
        [[nodiscard]] virtual MainActivityOrientation RequestedOrientation(
            MainActivity& activity) = 0;
        virtual void RequestedOrientation(
            MainActivity& activity,
            MainActivityOrientation orientation) = 0;
        [[nodiscard]] virtual std::int64_t UptimeMillis() = 0;
        [[nodiscard]] virtual bool IsAndroidVersionAtLeast30() = 0;
        [[nodiscard]] virtual std::int32_t DisplayRotation(
            MainActivity& activity) = 0;
        [[nodiscard]] virtual std::int32_t DefaultDisplayRotation(
            MainActivity& activity) = 0;
        [[nodiscard]] virtual MainActivityObjectRef DisplayManager(
            MainActivity& activity) = 0;
        virtual void RegisterDisplayListener(
            const MainActivityObjectRef& manager,
            MainActivity& activity) = 0;
        virtual void UnregisterDisplayListener(
            const MainActivityObjectRef& manager,
            MainActivity& activity) = 0;

        [[nodiscard]] virtual std::int32_t KeyEventAction(
            jobject event) = 0;
        [[nodiscard]] virtual std::int32_t KeyEventKeyCode(
            jobject event) = 0;

        [[nodiscard]] virtual MainActivityObjectRef WindowInsetsController(
            const MainActivityObjectRef& window) = 0;
        virtual void SetTransientBarsBySwipe(
            const MainActivityObjectRef& controller) = 0;
        virtual void HideSystemBars(
            const MainActivityObjectRef& controller) = 0;
        virtual void ShowSystemBars(
            const MainActivityObjectRef& controller) = 0;
        virtual void SetLegacySystemUiVisibility(
            const MainActivityObjectRef& window, bool immersive) = 0;

        // Captured AndroidThumbnailHost instances must resolve the Activity
        // object they actually captured, not whichever static Instance happens
        // to be current after a recreation.
        [[nodiscard]] virtual MainActivity* ResolveMainActivityPeer(
            JNIEnv* env, jobject activity) = 0;
    };

    // Supplied by the real Avalonia/Android host. No Java/Kotlin Activity,
    // SurfaceView, Service, manifest, looper or lifecycle is invented here.
    [[nodiscard]] MainActivityOwner& GetMainActivityOwner() noexcept;

    class MainActivity final
    {
    public:
        MainActivity(JNIEnv* env, jobject activity);
        ~MainActivity();

        MainActivity(const MainActivity&) = delete;
        MainActivity& operator=(const MainActivity&) = delete;
        MainActivity(MainActivity&&) = delete;
        MainActivity& operator=(MainActivity&&) = delete;

        [[nodiscard]] static MainActivity* Instance() noexcept;
        [[nodiscard]] bool InMatch() const noexcept;

        [[nodiscard]] MainActivityAppBuilderRef CustomizeAppBuilder(
            MainActivityAppBuilderRef builder);

        void OnCreate(jobject savedInstanceState);
        void OnConfigurationChanged(jobject newConfig);
        void OnPause();
        void OnResume();
        void OnWindowFocusChanged(bool hasFocus);
        void OnDestroy();
        void OnBackPressed();

        [[nodiscard]] bool DispatchKeyEvent(jobject event);
        [[nodiscard]] bool DispatchGenericMotionEvent(jobject event);

        void OnDisplayAdded(std::int32_t displayId);
        void OnDisplayRemoved(std::int32_t displayId);
        void OnDisplayChanged(std::int32_t displayId);

        [[nodiscard]] MphRead::Mods::ThumbnailTaskIntRef RenderPreviews(
            MphRead::Mods::ThumbnailRoomsRef rooms,
            MphRead::Mods::ThumbnailReportRef report);

        void StartMatch(const MphRead::Mods::Launcher::LaunchPlan& plan);
        void TogglePauseMenu();
        void EndMatch();
        void Finish();

    private:
        struct PendingMatch final
        {
            MphRead::Mods::Launcher::LaunchPlan Plan;
            std::shared_ptr<AndroidInput> Input;
        };

        static constexpr std::int32_t SettleMs = 250;
        static constexpr std::int32_t RotateMs = 3000;
        static constexpr std::int32_t GiveUpMs = 8000;

        // PreviewRun.Width/Height are C# const values and therefore are
        // compile-time constants at this call site.
        static constexpr std::int32_t PreviewWidth = 640;
        static constexpr std::int32_t PreviewHeight = 360;

        [[nodiscard]] std::string ChooseRoot();
        [[nodiscard]] static bool Writable(std::string_view directory);

        [[nodiscard]] std::int32_t RenderHere(
            const std::vector<std::string>& rooms,
            const std::function<void(const std::string&)>& report);

        [[nodiscard]] std::int32_t CurrentRotation();
        void AfterRotation();

        [[nodiscard]] MainActivitySize ContentSize() const;
        [[nodiscard]] MainActivityObjectRef LoadGameView() const noexcept;
        void StoreGameView(MainActivityObjectRef value) noexcept;
        void WaitForSteadyWindow();
        void CancelPending(std::string reason);
        void StartPending(std::optional<std::string> note);
        void ShowSoftKeyboard(bool show);
        void BeginMatch(
            const MphRead::Mods::Launcher::LaunchPlan& plan,
            std::shared_ptr<AndroidInput> input);

        void ShowNotice(std::string text);
        void MatchLoaded();
        void HideNotice();
        void FailMatch(std::string message);
        void ClosePauseMenu();
        void GoImmersive(bool immersive);

        [[nodiscard]] const std::string& RequireRoomKey(
            const MphRead::Mods::Launcher::LaunchPlan& plan) const;

        [[nodiscard]] JNIEnv* CurrentEnv() const;

        static std::atomic<MainActivity*> _instance;

        JavaVM* _javaVm = nullptr;
        jobject _activity = nullptr;

        MainActivityObjectRef _content{};
        MainActivityObjectRef _launcherView{};
        std::atomic<std::shared_ptr<void>> _gameView{};
        MainActivityObjectRef _overlay{};
        MainActivityObjectRef _notice{};
        MainActivityObjectRef _displays{};

        std::atomic<bool> _renderingPreviews{false};
        std::atomic<bool> _renderingHere{false};

        TouchControls _controls{};
        MainActivityOrientation _orientationBefore =
            MainActivityOrientation::Unspecified;

        std::int32_t _lastRotation = -1;

        std::unique_ptr<PendingMatch> _pending{};
        MainActivitySize _lastSize{};
        std::int64_t _waitingSince = 0;
        std::int64_t _sizeSettledAt = 0;

        bool _pauseMenuOpen = false;
    };
}

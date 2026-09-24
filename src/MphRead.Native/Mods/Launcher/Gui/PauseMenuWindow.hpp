#pragma once

#include "GuiTheme.hpp"
#include "MapPickerView.hpp"
#include "PauseMenuView.hpp"
#include "SettingsWindow.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    class PauseMenuWindow;
    class PauseMenuWindowAdapter;

    class PauseMenuWindowNullReferenceException final : public std::runtime_error
    {
    public:
        PauseMenuWindowNullReferenceException();
    };

    enum class PauseMenuWindowSystemDecorations : std::uint8_t
    {
        None
    };

    enum class PauseMenuWindowTransparencyLevel : std::uint8_t
    {
        Transparent,
        None
    };

    enum class PauseMenuWindowThemeVariant : std::uint8_t
    {
        Dark
    };

    enum class PauseMenuWindowStartupLocation : std::uint8_t
    {
        CenterScreen,
        Manual
    };

    enum class PauseMenuWindowKey : std::uint8_t
    {
        Escape,
        Other
    };

    struct PauseMenuWindowPixelPoint final
    {
        std::int32_t X = 0;
        std::int32_t Y = 0;

        friend constexpr bool operator==(
            const PauseMenuWindowPixelPoint&,
            const PauseMenuWindowPixelPoint&) noexcept = default;
    };

    struct PauseMenuWindowEventArgs final
    {
        void* Native = nullptr;
    };

    struct PauseMenuWindowKeyEventArgs final
    {
        PauseMenuWindowKey Key = PauseMenuWindowKey::Other;
        bool Handled = false;
        void* Native = nullptr;
    };

    class PauseMenuWindowDialogCompletion final
    {
    public:
        using Callback = void (*)(void* context, std::exception_ptr error);

        PauseMenuWindowDialogCompletion() = default;
        PauseMenuWindowDialogCompletion(void* context, Callback function,
            std::shared_ptr<void> keepAlive = {});

        void Invoke(std::exception_ptr error = {}) const;

    private:
        void* _context = nullptr;
        Callback _function = nullptr;
        std::shared_ptr<void> _keepAlive;
    };

    class PauseMenuWindowCoverTarget
    {
    public:
        virtual ~PauseMenuWindowCoverTarget() = default;

        virtual void SetWindowStartupLocation(
            PauseMenuWindowStartupLocation location) = 0;
        [[nodiscard]] virtual PauseMenuWindowPixelPoint Position() const = 0;
        virtual void SetPosition(PauseMenuWindowPixelPoint position) = 0;
        [[nodiscard]] virtual double Width() const = 0;
        [[nodiscard]] virtual double Height() const = 0;
        virtual void SetWidth(double width) = 0;
        virtual void SetHeight(double height) = 0;

        // Equivalent to window.Screens?.ScreenFromPoint(point)?.Scaling.
        // nullopt means either Screens or the matching Screen was null.
        [[nodiscard]] virtual std::optional<double> ScreenScalingFromPoint(
            PauseMenuWindowPixelPoint point) const = 0;
        [[nodiscard]] virtual double RenderScaling() const = 0;
    };

    // One platform window adapter is sufficient for both child Window classes.
    // The overloads are the unavoidable native binding for the two already-
    // migrated C# window types; their logic remains in SettingsWindow and
    // MapPickerWindow rather than being reimplemented here.
    class PauseMenuWindowChildAdapter
        : public PauseMenuWindowCoverTarget,
          public SettingsWindowAdapter,
          public MapPickerWindowAdapter
    {
    public:
        ~PauseMenuWindowChildAdapter() override = default;

        // Unify identical Window members inherited through both child adapters
        // (and, for size, the cover target) into the one underlying window.
        virtual void Close() override = 0;
        virtual void SetIcon(const std::optional<GuiWindowIcon>& icon) override = 0;
        virtual void SetBackground(const GuiBrush& brush) override = 0;
        virtual void SetWidth(double width) override = 0;
        virtual void SetHeight(double height) override = 0;
        virtual void SetMinWidth(double minWidth) override = 0;
        virtual void SetMinHeight(double minHeight) override = 0;

        // SettingsWindowAdapter's exact PauseMenuWindow.CoverGameWindow(this)
        // boundary. Keeping it here makes #61 call the #62 policy directly.
        void CoverGameWindow() final override;

        virtual void ShowDialog(PauseMenuWindowAdapter& owner,
            const std::shared_ptr<SettingsWindow>& window,
            PauseMenuWindowDialogCompletion completion) = 0;
        virtual void ShowDialog(PauseMenuWindowAdapter& owner,
            const std::shared_ptr<MapPickerWindow>& window,
            PauseMenuWindowDialogCompletion completion) = 0;
    };

    class PauseMenuWindowAdapter
        : public PauseMenuWindowCoverTarget,
          public PauseMenuViewAdapter
    {
    public:
        ~PauseMenuWindowAdapter() override = default;

        // Binds the C++ Window override target before the platform window is shown.
        virtual void BindWindow(const std::shared_ptr<PauseMenuWindow>& window) = 0;
        virtual void Show() = 0;
        virtual void Activate() = 0;
        virtual void Close() = 0;

        virtual void SetTitle(std::string title) = 0;
        virtual void SetIcon(const std::optional<GuiWindowIcon>& icon) = 0;
        virtual void SetCanResize(bool canResize) = 0;
        virtual void SetSystemDecorations(
            PauseMenuWindowSystemDecorations decorations) = 0;
        virtual void SetTransparencyLevelHint(
            std::span<const PauseMenuWindowTransparencyLevel> levels) = 0;
        virtual void SetBackground(const GuiBrush& brush) = 0;
        virtual void SetRequestedThemeVariant(PauseMenuWindowThemeVariant variant) = 0;
        [[nodiscard]] virtual bool Topmost() const = 0;
        virtual void SetTopmost(bool topmost) = 0;
        virtual void SetShowInTaskbar(bool showInTaskbar) = 0;
        virtual void SetWindowContent(const std::shared_ptr<PauseMenuView>& view) = 0;

        [[nodiscard]] virtual std::shared_ptr<PauseMenuWindowChildAdapter>
            CreateChildWindowAdapter() = 0;
        [[nodiscard]] virtual std::shared_ptr<SettingsViewAdapter>
            CreateSettingsViewAdapter() = 0;
        [[nodiscard]] virtual std::shared_ptr<MapPickerViewAdapter>
            CreateMapPickerViewAdapter() = 0;

        void DispatchOpened(PauseMenuWindow& window, PauseMenuWindowEventArgs& e);
        void DispatchKeyDown(PauseMenuWindow& window, PauseMenuWindowKeyEventArgs& e);
        void DispatchClosed(PauseMenuWindow& window, PauseMenuWindowEventArgs& e);

        virtual void BaseOnOpened(PauseMenuWindowEventArgs& e) = 0;
        virtual void BaseOnKeyDown(PauseMenuWindowKeyEventArgs& e) = 0;
        virtual void BaseOnClosed(PauseMenuWindowEventArgs& e) = 0;

        // AsyncVoidMethodBuilder.SetException posts an unhandled async-void
        // exception back through the captured UI synchronization context rather
        // than throwing it to the event invoker.
        virtual void PostAsyncVoidException(std::exception_ptr error) noexcept = 0;
    };

    struct PauseMenuWindowEventTarget;
    struct PauseMenuWindowSettingsContinuation;
    struct PauseMenuWindowMapContinuation;

    class PauseMenuWindow final
    {
    public:
        PauseMenuWindow(const PauseMenuWindow&) = delete;
        PauseMenuWindow& operator=(const PauseMenuWindow&) = delete;
        PauseMenuWindow(PauseMenuWindow&&) = delete;
        PauseMenuWindow& operator=(PauseMenuWindow&&) = delete;

        static void FollowGameWindow();
        [[nodiscard]] static bool Open();
        static void CloseIfOpen();
        [[nodiscard]] static bool IsOpen() noexcept;

        static void CoverGameWindow(PauseMenuWindowCoverTarget& window);

    protected:
        void OnOpened(PauseMenuWindowEventArgs& e);
        void OnKeyDown(PauseMenuWindowKeyEventArgs& e);
        void OnClosed(PauseMenuWindowEventArgs& e);

    private:
        friend class PauseMenuWindowAdapter;

        explicit PauseMenuWindow(std::shared_ptr<PauseMenuWindowAdapter> adapter);
        void FinishConstruction(const std::shared_ptr<PauseMenuWindow>& self);

        static double ScalingAt(PauseMenuWindowCoverTarget& window,
            PauseMenuWindowPixelPoint point);
        [[nodiscard]] static std::string WindowLabel();

        void OpenSettings();
        void OpenMapVote();
        void CloseSelf();

        static std::shared_ptr<PauseMenuWindow> LockEventTarget(void* context);
        static void OnResumed(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnFullscreenRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnSettingsRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnVoteMapRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnSpectateRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnRejoinRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnRecordToggleRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnLeaveRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);
        static void OnQuitRequested(void* context, void* sender,
            const PauseMenuViewEventArgs& args);

        static void ContinueSettings(void* context, std::exception_ptr error);
        static void ContinueMapVote(void* context, std::exception_ptr error);
        static void OnMapPickerClosed(void* context, void* sender,
            const MapPickerEventArgs& args);

        void FinishSettings(bool wasTopmost);
        void FinishMapVote(bool wasTopmost);

        static std::shared_ptr<PauseMenuWindow> _open;
        static std::shared_ptr<PauseMenuWindowChildAdapter> _openSettings;
        static const std::array<PauseMenuWindowTransparencyLevel, 2> _scrimLevels;

        std::shared_ptr<PauseMenuWindowAdapter> _adapter;
        std::shared_ptr<PauseMenuView> _view;
        std::shared_ptr<PauseMenuWindowEventTarget> _eventTarget;
        bool _settingsOpen = false;
        bool _voteOpen = false;
    };

    namespace Detail
    {
        // Platform construction boundary for Avalonia's `new PauseMenuWindow()`.
        [[nodiscard]] std::shared_ptr<PauseMenuWindowAdapter>
            CreatePauseMenuWindowAdapter();
    }
}

namespace MphRead::Mods::Detail
{
    // PauseMenu's own calls into the window, which C# makes directly.
    void PauseMenuGuiFollowGameWindow();
    [[nodiscard]] bool PauseMenuGuiOpenWindow();
    void PauseMenuGuiCloseWindowIfOpen();
}

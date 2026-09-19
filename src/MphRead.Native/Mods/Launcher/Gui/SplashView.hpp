#pragma once

#include "GuiTheme.hpp"
#include "TrackedText.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    struct SplashBitmap final
    {
        std::shared_ptr<void> Native;
        double Width = 0.0;
        double Height = 0.0;
    };

    struct SplashGradientStop final
    {
        GuiColor Color;
        double Offset;

        friend constexpr bool operator==(
            const SplashGradientStop&, const SplashGradientStop&) noexcept = default;
    };

    enum class SplashGradientSpreadMethod : std::int32_t
    {
        Pad,
        Reflect,
        Repeat
    };

    struct SplashLinearGradientBrush final
    {
        GuiRelativePoint StartPoint;
        GuiRelativePoint EndPoint;
        std::array<SplashGradientStop, 2> GradientStops;
        SplashGradientSpreadMethod SpreadMethod = SplashGradientSpreadMethod::Pad;
        double Opacity = 1.0;
        std::shared_ptr<void> Transform;
        GuiRelativePoint TransformOrigin{{0.0, 0.0}, GuiRelativeUnit::Relative};
        std::shared_ptr<void> RelativeTransform;
    };

    class SplashViewControlAdapter
    {
    public:
        virtual ~SplashViewControlAdapter() = default;

        [[nodiscard]] virtual GuiRect Bounds() const = 0;
        virtual void InvalidateVisual() = 0;

        [[nodiscard]] virtual std::string AppContextBaseDirectory() const = 0;
        [[nodiscard]] virtual std::string PathCombine(
            std::string_view left, std::string_view right) const = 0;

        // These are the native boundary for Avalonia Bitmap construction.
        // Implementations must consume the supplied data synchronously and
        // return a distinct owning bitmap object, or throw on failure.
        [[nodiscard]] virtual std::shared_ptr<SplashBitmap> CreateBitmapFromMemory(
            std::span<const std::uint8_t> bytes) = 0;
        [[nodiscard]] virtual std::shared_ptr<SplashBitmap> CreateBitmapFromAsset(
            std::string_view uri) = 0;
        virtual void DisposeBitmap(SplashBitmap& bitmap) = 0;

        [[nodiscard]] virtual std::u16string ToUpperInvariant(
            std::u16string_view text) const = 0;
    };

    class SplashViewDrawingContext : public TrackedTextAdapter
    {
    public:
        SplashViewDrawingContext() noexcept;
        ~SplashViewDrawingContext() override = default;

        virtual void FillRectangle(const GuiBrush& brush, GuiRect rect) = 0;
        virtual void FillRectangle(
            const SplashLinearGradientBrush& brush, GuiRect rect) = 0;
        virtual void DrawImage(const SplashBitmap& image,
            GuiRect source, GuiRect destination) = 0;
    };

    class SplashView final
    {
    public:
        explicit SplashView(SplashViewControlAdapter& control);

        SplashView(const SplashView&) = delete;
        SplashView& operator=(const SplashView&) = delete;
        SplashView(SplashView&&) = delete;
        SplashView& operator=(SplashView&&) = delete;

        [[nodiscard]] double BottomInset() const noexcept;
        void BottomInset(double value);

        void ShowRoom(const std::optional<std::string>& roomKey);
        void Render(SplashViewDrawingContext& context);

    private:
        [[nodiscard]] std::shared_ptr<SplashBitmap> LoadCustom();
        [[nodiscard]] std::shared_ptr<SplashBitmap> Load(std::string_view path);

        void DrawBrand(SplashViewDrawingContext& context,
            GuiRect body, double bottomInset);
        static void DrawCover(SplashViewDrawingContext& context,
            const SplashBitmap& image, GuiRect body);
        void DrawTitleCard(SplashViewDrawingContext& context, GuiRect body);

        [[nodiscard]] std::shared_ptr<SplashBitmap> Brand();

        SplashViewControlAdapter& _control;
        std::shared_ptr<SplashBitmap> _image;
        double _bottomInset = 0.0;
    };
}

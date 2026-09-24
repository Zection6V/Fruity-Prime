#include "SplashView.hpp"

#include "../../Branding.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::FileReadAllBytes;
using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::MathMin;
using ::MphRead::NativeRuntime::PathFromUtf8;

namespace
{
    constexpr std::string_view BrandUri
        = "avares://FruityPrime/Assets/fruity-prime-logo.png";

    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view text)
    {
        std::u16string result;
        result.reserve(text.size());

        std::size_t index = 0;
        while (index < text.size())
        {
            const auto first = static_cast<unsigned char>(text[index]);
            char32_t value = 0;
            std::size_t consumed = 1;

            if (first <= 0x7FU)
            {
                value = first;
            }
            else if (first >= 0xC2U && first <= 0xDFU
                && index + 1 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                if ((second & 0xC0U) == 0x80U)
                {
                    value = static_cast<char32_t>(((first & 0x1FU) << 6)
                        | (second & 0x3FU));
                    consumed = 2;
                }
                else
                {
                    value = 0xFFFDU;
                }
            }
            else if (first >= 0xE0U && first <= 0xEFU
                && index + 2 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                const auto third = static_cast<unsigned char>(text[index + 2]);
                const bool secondOk = (second & 0xC0U) == 0x80U
                    && !(first == 0xE0U && second < 0xA0U)
                    && !(first == 0xEDU && second >= 0xA0U);
                if (secondOk && (third & 0xC0U) == 0x80U)
                {
                    value = static_cast<char32_t>(((first & 0x0FU) << 12)
                        | ((second & 0x3FU) << 6) | (third & 0x3FU));
                    consumed = 3;
                }
                else
                {
                    value = 0xFFFDU;
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U
                && index + 3 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                const auto third = static_cast<unsigned char>(text[index + 2]);
                const auto fourth = static_cast<unsigned char>(text[index + 3]);
                const bool secondOk = (second & 0xC0U) == 0x80U
                    && !(first == 0xF0U && second < 0x90U)
                    && !(first == 0xF4U && second > 0x8FU);
                if (secondOk && (third & 0xC0U) == 0x80U
                    && (fourth & 0xC0U) == 0x80U)
                {
                    value = static_cast<char32_t>(((first & 0x07U) << 18)
                        | ((second & 0x3FU) << 12)
                        | ((third & 0x3FU) << 6) | (fourth & 0x3FU));
                    consumed = 4;
                }
                else
                {
                    value = 0xFFFDU;
                }
            }
            else
            {
                value = 0xFFFDU;
            }

            if (value <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(value));
            }
            else
            {
                value -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (value >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (value & 0x3FFU)));
            }
            index += consumed;
        }
        return result;
    }

    class BrandLazy final
    {
    public:
        [[nodiscard]] std::shared_ptr<MphRead::Mods::Launcher::Gui::SplashBitmap> Value(
            MphRead::Mods::Launcher::Gui::SplashViewControlAdapter& control)
        {
            {
                std::unique_lock lock(_mutex);
                for (;;)
                {
                    if (_state == State::Completed)
                    {
                        return _value;
                    }
                    if (_state == State::Running)
                    {
                        if (_owner == std::this_thread::get_id())
                        {
                            throw std::logic_error(
                                "ValueFactory attempted to access the Value property of this instance.");
                        }
                        _condition.wait(lock, [this]
                        {
                            return _state != State::Running;
                        });
                        continue;
                    }

                    _state = State::Running;
                    _owner = std::this_thread::get_id();
                    break;
                }
            }

            std::shared_ptr<MphRead::Mods::Launcher::Gui::SplashBitmap> value;
            try
            {
                value = control.CreateBitmapFromAsset(BrandUri);
            }
            catch (...)
            {
                value.reset();
            }

            {
                std::lock_guard lock(_mutex);
                _value = std::move(value);
                _owner = std::thread::id{};
                _state = State::Completed;
            }
            _condition.notify_all();
            return _value;
        }

    private:
        enum class State : std::uint8_t
        {
            NotStarted,
            Running,
            Completed
        };

        std::mutex _mutex;
        std::condition_variable _condition;
        State _state = State::NotStarted;
        std::thread::id _owner;
        std::shared_ptr<MphRead::Mods::Launcher::Gui::SplashBitmap> _value;
    };

    [[nodiscard]] BrandLazy& BrandState()
    {
        static BrandLazy lazy;
        return lazy;
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    SplashViewDrawingContext::SplashViewDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    SplashView::SplashView(SplashViewControlAdapter& control)
        : _control(control)
    {
        _image = LoadCustom();
    }

    double SplashView::BottomInset() const noexcept
    {
        return _bottomInset;
    }

    void SplashView::BottomInset(double value)
    {
        if (std::abs(_bottomInset - value) > 0.5)
        {
            _bottomInset = value;
            _control.InvalidateVisual();
        }
    }

    void SplashView::ShowRoom(const std::optional<std::string>& roomKey)
    {
        std::shared_ptr<SplashBitmap> next;
        if (roomKey.has_value() && !roomKey->empty())
        {
            next = Load(::MphRead::Mods::ThumbnailGenerator::PathFor(*roomKey));
        }
        if (!next)
        {
            next = LoadCustom();
        }
        if (next.get() != _image.get())
        {
            if (_image)
            {
                _control.DisposeBitmap(*_image);
            }
            _image = std::move(next);
        }
        _control.InvalidateVisual();
    }

    std::shared_ptr<SplashBitmap> SplashView::LoadCustom()
    {
        static constexpr std::array<std::string_view, 3> Names{
            "splash.png", "splash.jpg", "splash.jpeg"
        };

        for (const std::string_view name : Names)
        {
            const std::string customPath = _control.PathCombine(
                _control.AppContextBaseDirectory(), name);
            std::shared_ptr<SplashBitmap> custom = Load(customPath);
            if (custom)
            {
                return custom;
            }
        }
        return {};
    }

    std::shared_ptr<SplashBitmap> SplashView::Load(std::string_view path)
    {
        try
        {
            if (!FileExists(path))
            {
                return {};
            }
            const std::vector<std::uint8_t> bytes = FileReadAllBytes(std::string(path));
            return _control.CreateBitmapFromMemory(bytes);
        }
        catch (...)
        {
            return {};
        }
    }

    void SplashView::Render(SplashViewDrawingContext& context)
    {
        const GuiRect body{
            0.0,
            0.0,
            _control.Bounds().Width,
            _control.Bounds().Height
        };
        context.FillRectangle(GuiTheme::InkBrush, body);
        if (_image)
        {
            DrawCover(context, *_image, body);
        }
        else
        {
            DrawTitleCard(context, body);
        }

        const SplashLinearGradientBrush wash{
            GuiRelativePoint{{0.0, 1.0}, GuiRelativeUnit::Relative},
            GuiRelativePoint{{0.0, 0.0}, GuiRelativeUnit::Relative},
            std::array<SplashGradientStop, 2>{
                SplashGradientStop{GuiColor::FromArgb(220, 10, 12, 16), 0.0},
                SplashGradientStop{GuiColor::FromArgb(0, 10, 12, 16), 1.0}
            },
            SplashGradientSpreadMethod::Pad,
            1.0,
            {},
            GuiRelativePoint{{0.0, 0.0}, GuiRelativeUnit::Relative},
            {}
        };
        context.FillRectangle(wash, GuiRect{
            0.0,
            body.Height - 90.0 - _bottomInset,
            body.Width,
            90.0 + _bottomInset
        });

        if (_image)
        {
            DrawBrand(context, body, _bottomInset);
        }
    }

    void SplashView::DrawBrand(SplashViewDrawingContext& context,
        GuiRect body, double bottomInset)
    {
        const std::shared_ptr<SplashBitmap> brand = Brand();
        if (!brand || body.Width < 80.0)
        {
            return;
        }

        const double width = MathMin(MathMin(body.Width - 48.0, 320.0), brand->Width);
        const double height = width * brand->Height / brand->Width;
        context.DrawImage(*brand,
            GuiRect{0.0, 0.0, brand->Width, brand->Height},
            GuiRect{24.0, body.Height - 26.0 - height - bottomInset, width, height});
    }

    void SplashView::DrawCover(SplashViewDrawingContext& context,
        const SplashBitmap& image, GuiRect body)
    {
        const double scale = MathMax(body.Width / image.Width,
            body.Height / image.Height);
        const double width = image.Width * scale;
        const double height = image.Height * scale;
        context.DrawImage(image,
            GuiRect{0.0, 0.0, image.Width, image.Height},
            GuiRect{(body.Width - width) / 2.0,
                (body.Height - height) / 2.0, width, height});
    }

    void SplashView::DrawTitleCard(SplashViewDrawingContext& context, GuiRect body)
    {
        const double cx = body.Width / 2.0;
        const double cy = body.Height / 2.0 - 20.0;
        const std::shared_ptr<SplashBitmap> brand = Brand();
        if (brand)
        {
            const double maxWidth = MathMin(body.Width * 0.72, brand->Width);
            const double scale = maxWidth / brand->Width;
            const double width = brand->Width * scale;
            const double height = brand->Height * scale;
            const GuiRect destination{
                cx - width / 2.0,
                cy - height / 2.0,
                width,
                height
            };
            context.DrawImage(*brand,
                GuiRect{0.0, 0.0, brand->Width, brand->Height}, destination);
            return;
        }

        const std::u16string name = Utf8ToUtf16(::MphRead::Mods::Branding::Name);
        const std::u16string upper = _control.ToUpperInvariant(name);
        const TrackedTextFormattedText title = context.CreateFormattedText(
            upper,
            TrackedTextCulture::Invariant,
            TrackedTextFlowDirection::LeftToRight,
            TrackedTextFace::FaceTrue,
            30.0,
            TrackedTextBrush{&GuiTheme::TextBrush});
        context.DrawText(title, TrackedTextPoint{cx - title.Width / 2.0, cy});
    }

    std::shared_ptr<SplashBitmap> SplashView::Brand()
    {
        return BrandState().Value(_control);
    }
}

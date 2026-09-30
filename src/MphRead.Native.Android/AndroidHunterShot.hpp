#pragma once

#if !defined(__ANDROID__)
#error "AndroidHunterShot is only valid for the Android native target."
#endif

#include "../MphRead.Native/Mods/Render/HunterShot.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Droid
{
    class AndroidHunterShot final : public Mods::Render::IHunterShot
    {
    public:
        AndroidHunterShot(const AndroidHunterShot&) = delete;
        AndroidHunterShot& operator=(const AndroidHunterShot&) = delete;
        AndroidHunterShot(AndroidHunterShot&&) = delete;
        AndroidHunterShot& operator=(AndroidHunterShot&&) = delete;
        ~AndroidHunterShot() override;

        // MainApplication installs one instance before the launcher is built.
        // The same instance is exposed to the activity for Retire(), and to
        // HunterStand through HunterShot::Current.
        [[nodiscard]] static std::shared_ptr<AndroidHunterShot> Install();
        [[nodiscard]] static std::shared_ptr<AndroidHunterShot> Current();
        static void RetireCurrent();
        static void ResumeCurrent();

        [[nodiscard]] std::shared_future<
            std::optional<std::vector<std::uint8_t>>> RenderAsync(
                Hunter hunter,
                std::int32_t suit,
                std::int32_t width,
                std::int32_t height) override;

        void Retire();

    private:
        struct State;
        struct Job;
        struct Worker;

        AndroidHunterShot();

        static void Loop(
            const std::shared_ptr<State>& state,
            const std::shared_ptr<Worker>& worker);
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> Draw(
            Scene& scene,
            const Job& job,
            std::int32_t width,
            std::int32_t height);
        [[nodiscard]] static bool InMatch() noexcept;

        std::shared_ptr<State> _state;
    };
}

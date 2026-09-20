#pragma once

#if !defined(__ANDROID__)
#error "PreviewService is only valid for the Android native target."
#endif

#include <array>
#include <cstdint>
#include <jni.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Droid
{
    struct PreviewWorkerType final
    {
        std::string_view Name;
        std::string_view Process;
        bool Exported;
    };

    class PreviewService
    {
    public:
        inline static constexpr std::string_view RoomsExtra = "rooms";
        inline static constexpr std::string_view MarkerExtra = "marker";
        inline static constexpr std::string_view WidthExtra = "width";
        inline static constexpr std::string_view HeightExtra = "height";

        // Native peer for an Android Service subclass. A Java/Kotlin service
        // owner must forward the framework callbacks to this peer; the native
        // pair deliberately does not invent a substitute Android lifecycle.
        PreviewService(JNIEnv* env, jobject service);
        virtual ~PreviewService() = 0;

        PreviewService(const PreviewService&) = delete;
        PreviewService& operator=(const PreviewService&) = delete;
        PreviewService(PreviewService&&) = delete;
        PreviewService& operator=(PreviewService&&) = delete;

        [[nodiscard]] virtual jobject OnBind(JNIEnv* env, jobject intent) const noexcept;

        [[nodiscard]] virtual std::int32_t OnStartCommand(
            JNIEnv* env,
            jobject intent,
            std::int32_t flags,
            std::int32_t startId
        );

    private:
        class ServiceTarget;

        static constexpr std::int32_t StartNotSticky = 2;

        static void Run(
            const std::shared_ptr<ServiceTarget>& service,
            const std::vector<std::string>& rooms,
            std::int32_t width,
            std::int32_t height
        );

        static void Finish(const std::optional<std::string>& marker);

        std::shared_ptr<ServiceTarget> _service;
    };

    class PreviewWorker0 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker1 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker2 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker3 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker4 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker5 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker6 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker7 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker8 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorker9 final : public PreviewService
    {
    public:
        using PreviewService::PreviewService;
    };

    class PreviewWorkerTypes final
    {
    public:
        static const std::array<PreviewWorkerType, 10> All;

    private:
        PreviewWorkerTypes() = delete;
        ~PreviewWorkerTypes() = delete;
        PreviewWorkerTypes(const PreviewWorkerTypes&) = delete;
        PreviewWorkerTypes& operator=(const PreviewWorkerTypes&) = delete;
        PreviewWorkerTypes(PreviewWorkerTypes&&) = delete;
        PreviewWorkerTypes& operator=(PreviewWorkerTypes&&) = delete;
    };
}

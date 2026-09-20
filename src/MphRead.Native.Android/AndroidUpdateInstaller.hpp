#pragma once

#if !defined(__ANDROID__)
#error "AndroidUpdateInstaller is only valid for the Android native target."
#endif

#include <jni.h>

#include "../MphRead.Native/Mods/Update/UpdateInstall.hpp"

#include <functional>
#include <string>

namespace MphRead::Droid
{
    class AndroidUpdateInstaller final : public MphRead::Mods::Update::IUpdateInstaller
    {
    public:
        AndroidUpdateInstaller(JNIEnv* env, jobject activity);
        ~AndroidUpdateInstaller() override;

        AndroidUpdateInstaller(const AndroidUpdateInstaller&) = delete;
        AndroidUpdateInstaller& operator=(const AndroidUpdateInstaller&) = delete;
        AndroidUpdateInstaller(AndroidUpdateInstaller&&) = delete;
        AndroidUpdateInstaller& operator=(AndroidUpdateInstaller&&) = delete;

        [[nodiscard]] bool Allowed() override;
        [[nodiscard]] bool RequestPermission() override;
        [[nodiscard]] bool Prepare(
            MphRead::Mods::Update::UpdateInfo update,
            std::function<void(float)> progress,
            std::string& error
        ) override;
        [[nodiscard]] bool Install(std::string& error) override;
        [[nodiscard]] bool ExitAfterInstall() override;
        [[nodiscard]] std::function<void(bool, std::string)> Finished() override;
        void Finished(std::function<void(bool, std::string)> value) override;

    private:
        JavaVM* _javaVm = nullptr;
        jobject _activity = nullptr;
        std::string _staged{};
    };
}

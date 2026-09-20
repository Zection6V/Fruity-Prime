#pragma once

#if !defined(__ANDROID__)
#error "AndroidLogShare is only valid for the Android native target."
#endif

#include <jni.h>

#include "../MphRead.Native/Mods/LogShare.hpp"

#include <string>
#include <string_view>

namespace MphRead::Droid
{
    class AndroidLogShare final : public MphRead::Mods::ILogShare
    {
    public:
        AndroidLogShare(JNIEnv* env, jobject context);
        ~AndroidLogShare() override;

        AndroidLogShare(const AndroidLogShare&) = delete;
        AndroidLogShare& operator=(const AndroidLogShare&) = delete;
        AndroidLogShare(AndroidLogShare&&) = delete;
        AndroidLogShare& operator=(AndroidLogShare&&) = delete;

        [[nodiscard]] std::u16string StagingPath(std::u16string_view fileName) override;
        bool Share(
            std::u16string_view path,
            std::u16string_view subject,
            std::u16string& error
        ) override;

    private:
        static constexpr std::u16string_view _folder = u"logs-share";

        JavaVM* _javaVm = nullptr;
        jobject _context = nullptr;
    };
}

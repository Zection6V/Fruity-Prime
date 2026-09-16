#pragma once

#if !defined(__ANDROID__)
#error "AndroidLogShare is only valid for the Android native target."
#endif

#include <jni.h>

#include <string>
#include <string_view>

namespace MphRead::Droid
{
    class AndroidLogShare final
    {
    public:
        AndroidLogShare(JNIEnv* env, jobject context);
        ~AndroidLogShare();

        AndroidLogShare(const AndroidLogShare&) = delete;
        AndroidLogShare& operator=(const AndroidLogShare&) = delete;
        AndroidLogShare(AndroidLogShare&&) = delete;
        AndroidLogShare& operator=(AndroidLogShare&&) = delete;

        [[nodiscard]] std::u16string StagingPath(std::u16string_view fileName);
        bool Share(
            std::u16string_view path,
            std::u16string_view subject,
            std::u16string& error
        );

    private:
        static constexpr std::u16string_view _folder = u"logs-share";

        JavaVM* _javaVm = nullptr;
        jobject _context = nullptr;
    };
}

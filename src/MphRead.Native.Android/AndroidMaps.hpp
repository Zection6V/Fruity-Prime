#pragma once

#if !defined(__ANDROID__)
#error "AndroidMaps is only valid for the Android native target."
#endif

#include <jni.h>

#include <string_view>

namespace MphRead::Droid
{
    class AndroidMaps final
    {
    public:
        static void Install(JNIEnv* env, jobject assets, std::u16string_view root);
        static void EnsureBuilt();

    private:
        static constexpr std::u16string_view AssetFolder = u"maps";

        AndroidMaps() = delete;
        ~AndroidMaps() = delete;
        AndroidMaps(const AndroidMaps&) = delete;
        AndroidMaps& operator=(const AndroidMaps&) = delete;
        AndroidMaps(AndroidMaps&&) = delete;
        AndroidMaps& operator=(AndroidMaps&&) = delete;
    };
}

#pragma once

#if !defined(__ANDROID__)
#error "ApkInstaller is only valid for the Android native target."
#endif

#include <jni.h>

#include <functional>
#include <optional>
#include <string>

namespace MphRead::Droid
{
    class InstallResultReceiver;

    class ApkInstaller final
    {
    public:
        using FinishedHandler = std::function<void(bool, std::string)>;

        [[nodiscard]] static FinishedHandler Finished();
        static void Finished(FinishedHandler value);

        [[nodiscard]] static std::string StagingPath(JNIEnv* env, jobject context);
        [[nodiscard]] static bool Allowed(JNIEnv* env, jobject context);
        [[nodiscard]] static bool RequestPermission(JNIEnv* env, jobject activity);
        [[nodiscard]] static bool SameSigner(
            JNIEnv* env,
            jobject context,
            const std::string& apkPath,
            std::optional<std::string>& mismatch
        );
        [[nodiscard]] static bool Commit(
            JNIEnv* env,
            jobject context,
            const std::string& apkPath,
            std::string& error
        );

        ApkInstaller() = delete;
        ~ApkInstaller() = delete;
        ApkInstaller(const ApkInstaller&) = delete;
        ApkInstaller& operator=(const ApkInstaller&) = delete;
        ApkInstaller(ApkInstaller&&) = delete;
        ApkInstaller& operator=(ApkInstaller&&) = delete;

    private:
        static FinishedHandler _finished;
    };

    class InstallResultReceiver final
    {
    public:
        // Mechanical JNI owner seam for the Java BroadcastReceiver class that
        // corresponds to the C# [BroadcastReceiver] type.
        static void JavaClass(JNIEnv* env, jclass receiverClass);

        // Called by that Java BroadcastReceiver from its onReceive method.
        static void OnReceive(JNIEnv* env, jobject context, jobject intent);

        InstallResultReceiver() = delete;
        ~InstallResultReceiver() = delete;
        InstallResultReceiver(const InstallResultReceiver&) = delete;
        InstallResultReceiver& operator=(const InstallResultReceiver&) = delete;
        InstallResultReceiver(InstallResultReceiver&&) = delete;
        InstallResultReceiver& operator=(InstallResultReceiver&&) = delete;

    private:
        friend class ApkInstaller;

        [[nodiscard]] static jclass JavaClass();
        [[nodiscard]] static std::string Explain(jint status, const std::string& message);
        static void Report(bool ok, const std::string& message);

        static jclass _javaClass;
    };
}

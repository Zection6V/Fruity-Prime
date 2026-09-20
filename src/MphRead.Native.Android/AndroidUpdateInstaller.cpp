#include "AndroidUpdateInstaller.hpp"

#include "ApkInstaller.hpp"
#include "../MphRead.Native/Mods/Update/UpdateDownload.hpp"

#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    class ScopedJniEnv final
    {
    public:
        explicit ScopedJniEnv(JavaVM* javaVm)
            : _javaVm(javaVm)
        {
            if (_javaVm == nullptr)
            {
                throw std::runtime_error("Android Java VM is not available");
            }

            const jint result = _javaVm->GetEnv(
                reinterpret_cast<void**>(&_env),
                JNI_VERSION_1_6
            );
            if (result == JNI_EDETACHED)
            {
                if (_javaVm->AttachCurrentThread(&_env, nullptr) != JNI_OK)
                {
                    throw std::runtime_error(
                        "could not attach the current thread to the Android Java VM"
                    );
                }
                _attached = true;
            }
            else if (result != JNI_OK || _env == nullptr)
            {
                throw std::runtime_error(
                    "could not obtain the Android JNI environment"
                );
            }
        }

        ~ScopedJniEnv()
        {
            if (_attached)
            {
                _javaVm->DetachCurrentThread();
            }
        }

        ScopedJniEnv(const ScopedJniEnv&) = delete;
        ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

        [[nodiscard]] JNIEnv* Get() const noexcept
        {
            return _env;
        }

    private:
        JavaVM* _javaVm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    void DeleteGlobalRefNoThrow(JavaVM* javaVm, jobject value) noexcept
    {
        if (javaVm == nullptr || value == nullptr)
        {
            return;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        const jint result = javaVm->GetEnv(
            reinterpret_cast<void**>(&env),
            JNI_VERSION_1_6
        );
        if (result == JNI_EDETACHED)
        {
            if (javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            {
                return;
            }
            attached = true;
        }
        else if (result != JNI_OK || env == nullptr)
        {
            return;
        }

        env->DeleteGlobalRef(value);
        if (attached)
        {
            javaVm->DetachCurrentThread();
        }
    }
}

namespace MphRead::Droid
{
    AndroidUpdateInstaller::AndroidUpdateInstaller(JNIEnv* env, jobject activity)
    {
        if (env == nullptr)
        {
            throw std::invalid_argument(
                "Android JNI environment must not be null"
            );
        }
        if (env->GetJavaVM(&_javaVm) != JNI_OK || _javaVm == nullptr)
        {
            throw std::runtime_error("Android Java VM is not available");
        }

        _activity = env->NewGlobalRef(activity);
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            throw std::runtime_error(
                "Android Java exception while retaining the Activity"
            );
        }
        if (activity != nullptr && _activity == nullptr)
        {
            throw std::bad_alloc();
        }
    }

    AndroidUpdateInstaller::~AndroidUpdateInstaller()
    {
        DeleteGlobalRefNoThrow(_javaVm, _activity);
    }

    bool AndroidUpdateInstaller::Allowed()
    {
        ScopedJniEnv scopedEnv(_javaVm);
        return ApkInstaller::Allowed(scopedEnv.Get(), _activity);
    }

    bool AndroidUpdateInstaller::RequestPermission()
    {
        ScopedJniEnv scopedEnv(_javaVm);
        return ApkInstaller::RequestPermission(scopedEnv.Get(), _activity);
    }

    bool AndroidUpdateInstaller::ExitAfterInstall()
    {
        return false;
    }

    std::function<void(bool, std::string)> AndroidUpdateInstaller::Finished()
    {
        return ApkInstaller::Finished();
    }

    void AndroidUpdateInstaller::Finished(
        std::function<void(bool, std::string)> value
    )
    {
        ApkInstaller::Finished(std::move(value));
    }

    bool AndroidUpdateInstaller::Prepare(
        MphRead::Mods::Update::UpdateInfo update,
        std::function<void(float)> progress,
        std::string& error
    )
    {
        {
            ScopedJniEnv scopedEnv(_javaVm);
            _staged = ApkInstaller::StagingPath(scopedEnv.Get(), _activity);
        }

        const std::optional<std::string>& assetUrl = update.AssetUrl.Get();
        const std::string url = assetUrl.has_value()
            ? *assetUrl
            : std::string{};

        if (!MphRead::Mods::Update::UpdateDownload::Fetch(
                url,
                _staged,
                update.AssetSize.Get(),
                progress
            ))
        {
            const std::optional<std::string> lastError
                = MphRead::Mods::Update::UpdateDownload::LastError();
            error = lastError.has_value()
                ? *lastError
                : "the download failed";
            return false;
        }

        std::optional<std::string> mismatch;
        {
            ScopedJniEnv scopedEnv(_javaVm);
            if (!ApkInstaller::SameSigner(
                    scopedEnv.Get(),
                    _activity,
                    _staged,
                    mismatch
                ))
            {
                error = mismatch.has_value()
                    ? *mismatch
                    : "that package cannot be installed over this one";
                return false;
            }
        }

        error.clear();
        return true;
    }

    bool AndroidUpdateInstaller::Install(std::string& error)
    {
        ScopedJniEnv scopedEnv(_javaVm);
        return ApkInstaller::Commit(
            scopedEnv.Get(),
            _activity,
            _staged,
            error
        );
    }
}

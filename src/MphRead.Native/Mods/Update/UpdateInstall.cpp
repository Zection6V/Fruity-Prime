#include "UpdateInstall.hpp"

#include "DesktopUpdate.hpp"

#include <memory>
#include <utility>

namespace MphRead::Mods::Update
{
    NullReferenceException::NullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    std::shared_ptr<IUpdateInstaller> UpdateInstall::_current{};

    std::shared_ptr<IUpdateInstaller> UpdateInstall::Current()
    {
        return _current;
    }

    void UpdateInstall::Current(std::shared_ptr<IUpdateInstaller> value)
    {
        _current = std::move(value);
    }

    bool UpdateInstall::CanInstall(UpdateInfo update)
    {
        if (_current == nullptr)
        {
            return false;
        }

        const std::optional<std::string>& assetUrl = update.AssetUrl.Get();
        if (!assetUrl.has_value())
        {
            throw NullReferenceException();
        }
        return !assetUrl->empty();
    }

    void UpdateInstall::UseDesktopIfPossible()
    {
        if (_current == nullptr && DesktopUpdate::Supported())
        {
            _current = std::make_shared<DesktopUpdateInstaller>();
        }
    }

    bool DesktopUpdateInstaller::Allowed() const
    {
        return true;
    }

    bool DesktopUpdateInstaller::RequestPermission()
    {
        return true;
    }

    bool DesktopUpdateInstaller::ExitAfterInstall() const
    {
        return true;
    }

    std::function<void(bool, std::string)> DesktopUpdateInstaller::Finished() const
    {
        return _finished;
    }

    void DesktopUpdateInstaller::Finished(std::function<void(bool, std::string)> value)
    {
        _finished = std::move(value);
    }

    bool DesktopUpdateInstaller::Prepare(
        UpdateInfo update,
        std::function<void(float)> progress,
        std::string& error)
    {
        const bool ok = DesktopUpdate::Stage(std::move(update), std::move(progress));
        if (ok)
        {
            error.clear();
        }
        else
        {
            std::optional<std::string> lastError = DesktopUpdate::LastError();
            error = lastError.has_value() ? *lastError : "the download failed";
        }
        return ok;
    }

    bool DesktopUpdateInstaller::Install(std::string& error)
    {
        const bool ok = DesktopUpdate::Launch();
        if (ok)
        {
            error.clear();
        }
        else
        {
            std::optional<std::string> lastError = DesktopUpdate::LastError();
            error = lastError.has_value() ? *lastError : "the update could not be started";
        }
        return ok;
    }
}

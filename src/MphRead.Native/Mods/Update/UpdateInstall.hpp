#pragma once

#include "UpdateCheck.hpp"

#include <functional>
#include <memory>
#include <string>

namespace MphRead::Mods::Update
{
    class IUpdateInstaller
    {
    public:
        virtual ~IUpdateInstaller() = default;

        [[nodiscard]] virtual bool Allowed() = 0;
        [[nodiscard]] virtual bool RequestPermission() = 0;
        [[nodiscard]] virtual bool Prepare(
            UpdateInfo update,
            std::function<void(float)> progress,
            std::string& error) = 0;
        [[nodiscard]] virtual bool Install(std::string& error) = 0;
        [[nodiscard]] virtual bool ExitAfterInstall() = 0;
        [[nodiscard]] virtual std::function<void(bool, std::string)> Finished() = 0;
        virtual void Finished(std::function<void(bool, std::string)> value) = 0;
    };

    class UpdateInstall final
    {
    public:
        [[nodiscard]] static std::shared_ptr<IUpdateInstaller> Current();
        static void Current(std::shared_ptr<IUpdateInstaller> value);

        [[nodiscard]] static bool CanInstall(UpdateInfo update);
        static void UseDesktopIfPossible();

        UpdateInstall() = delete;
        UpdateInstall(const UpdateInstall&) = delete;
        UpdateInstall& operator=(const UpdateInstall&) = delete;

    private:
        static std::shared_ptr<IUpdateInstaller> _current;
    };

    class DesktopUpdateInstaller final : public IUpdateInstaller
    {
    public:
        DesktopUpdateInstaller() = default;
        DesktopUpdateInstaller(const DesktopUpdateInstaller&) = delete;
        DesktopUpdateInstaller& operator=(const DesktopUpdateInstaller&) = delete;
        DesktopUpdateInstaller(DesktopUpdateInstaller&&) = delete;
        DesktopUpdateInstaller& operator=(DesktopUpdateInstaller&&) = delete;
        ~DesktopUpdateInstaller() override = default;

        [[nodiscard]] bool Allowed() override;
        [[nodiscard]] bool RequestPermission() override;
        [[nodiscard]] bool Prepare(
            UpdateInfo update,
            std::function<void(float)> progress,
            std::string& error) override;
        [[nodiscard]] bool Install(std::string& error) override;
        [[nodiscard]] bool ExitAfterInstall() override;
        [[nodiscard]] std::function<void(bool, std::string)> Finished() override;
        void Finished(std::function<void(bool, std::string)> value) override;

    private:
        std::function<void(bool, std::string)> _finished{};
    };
}

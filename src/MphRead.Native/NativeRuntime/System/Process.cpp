#include "Process.hpp"

#include "Encoding.hpp"
#include "Exceptions.hpp"
#include "Runtime.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        // Win32Exception / the errno message .NET wraps a failed start in.
        [[noreturn]] void ThrowStartFailure(const std::string& fileName, const std::string& reason)
        {
            throw std::runtime_error("An error occurred trying to start process '" + fileName + "'. " + reason);
        }

#if defined(_WIN32)
        [[nodiscard]] std::string LastErrorMessage()
        {
            const DWORD error = ::GetLastError();
            wchar_t* buffer = nullptr;
            const DWORD length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
            std::string message = length == 0 ? "error " + std::to_string(error) : WideToUtf8(std::wstring(buffer, length));
            if (buffer != nullptr)
            {
                ::LocalFree(buffer);
            }
            while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' '))
            {
                message.pop_back();
            }
            return message;
        }

        // Process.Kill(true) on Windows: every descendant, found by parent id
        // and started no earlier than its parent, then the process itself.
        void KillTree(DWORD id)
        {
            const HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot != INVALID_HANDLE_VALUE)
            {
                PROCESSENTRY32W entry{};
                entry.dwSize = sizeof(entry);
                std::vector<DWORD> children;
                for (BOOL more = ::Process32FirstW(snapshot, &entry); more != FALSE; more = ::Process32NextW(snapshot, &entry))
                {
                    if (entry.th32ParentProcessID == id && entry.th32ProcessID != id)
                    {
                        children.push_back(entry.th32ProcessID);
                    }
                }
                ::CloseHandle(snapshot);
                for (const DWORD child : children)
                {
                    KillTree(child);
                }
            }
            const HANDLE handle = ::OpenProcess(PROCESS_TERMINATE, FALSE, id);
            if (handle != nullptr)
            {
                ::TerminateProcess(handle, static_cast<UINT>(-1));
                ::CloseHandle(handle);
            }
        }
#else
        // Process.Kill(true) on Unix: children found through /proc.
        void KillTree(pid_t pid)
        {
#if defined(__linux__)
            if (DIR* proc = ::opendir("/proc"))
            {
                std::vector<pid_t> children;
                while (const dirent* item = ::readdir(proc))
                {
                    const pid_t candidate = static_cast<pid_t>(std::atoi(item->d_name));
                    if (candidate <= 0 || candidate == pid)
                    {
                        continue;
                    }
                    std::ifstream stat("/proc/" + std::string(item->d_name) + "/stat");
                    std::string text;
                    std::getline(stat, text);
                    const std::size_t close = text.rfind(')');
                    if (close == std::string::npos)
                    {
                        continue;
                    }
                    std::istringstream fields(text.substr(close + 1));
                    std::string state;
                    pid_t parent = 0;
                    fields >> state >> parent;
                    if (parent == pid)
                    {
                        children.push_back(candidate);
                    }
                }
                ::closedir(proc);
                for (const pid_t child : children)
                {
                    KillTree(child);
                }
            }
#endif
            ::kill(pid, SIGKILL);
        }
#endif
    }

    std::shared_ptr<Process> Process::Start(const ProcessStartInfo& startInfo)
    {
        std::shared_ptr<Process> process(new Process());
#if defined(_WIN32)
        std::wstring arguments;
        for (const std::string& argument : startInfo.ArgumentList)
        {
            if (!arguments.empty())
            {
                arguments += L' ';
            }
            arguments += PasteArgument(Wtf8ToWide(argument));
        }
        const std::wstring file = Wtf8ToWide(startInfo.FileName);
        const std::wstring directory = Wtf8ToWide(startInfo.WorkingDirectory);
        if (startInfo.UseShellExecute)
        {
            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
            info.lpFile = file.c_str();
            info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
            info.lpDirectory = directory.empty() ? nullptr : directory.c_str();
            info.nShow = SW_SHOWNORMAL;
            if (::ShellExecuteExW(&info) == FALSE)
            {
                ThrowStartFailure(startInfo.FileName, LastErrorMessage());
            }
            if (info.hProcess == nullptr)
            {
                return nullptr;
            }
            process->_handle = info.hProcess;
            process->_id = ::GetProcessId(info.hProcess);
            return process;
        }
        std::wstring commandLine = PasteArgument(file);
        if (!arguments.empty())
        {
            commandLine += L' ' + arguments;
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION information{};
        const DWORD flags = CREATE_UNICODE_ENVIRONMENT | (startInfo.CreateNoWindow ? CREATE_NO_WINDOW : 0);
        if (::CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, flags, nullptr,
            directory.empty() ? nullptr : directory.c_str(), &startup, &information) == FALSE)
        {
            ThrowStartFailure(startInfo.FileName, LastErrorMessage());
        }
        ::CloseHandle(information.hThread);
        process->_handle = information.hProcess;
        process->_id = information.dwProcessId;
        return process;
#else
        std::vector<std::string> strings;
        strings.push_back(startInfo.FileName);
        strings.insert(strings.end(), startInfo.ArgumentList.begin(), startInfo.ArgumentList.end());
        std::vector<char*> argv;
        for (std::string& value : strings)
        {
            argv.push_back(value.data());
        }
        argv.push_back(nullptr);
        int pipes[2];
        if (::pipe(pipes) != 0)
        {
            ThrowStartFailure(startInfo.FileName, std::strerror(errno));
        }
        const pid_t pid = ::fork();
        if (pid < 0)
        {
            const int error = errno;
            ::close(pipes[0]);
            ::close(pipes[1]);
            ThrowStartFailure(startInfo.FileName, std::strerror(error));
        }
        if (pid == 0)
        {
            ::close(pipes[0]);
            ::fcntl(pipes[1], F_SETFD, FD_CLOEXEC);
            if (!startInfo.WorkingDirectory.empty() && ::chdir(startInfo.WorkingDirectory.c_str()) != 0)
            {
                const int error = errno;
                static_cast<void>(::write(pipes[1], &error, sizeof(error)));
                ::_exit(127);
            }
            ::execvp(argv[0], argv.data());
            const int error = errno;
            static_cast<void>(::write(pipes[1], &error, sizeof(error)));
            ::_exit(127);
        }
        ::close(pipes[1]);
        int childError = 0;
        const ssize_t read = ::read(pipes[0], &childError, sizeof(childError));
        ::close(pipes[0]);
        if (read == sizeof(childError))
        {
            ::waitpid(pid, nullptr, 0);
            ThrowStartFailure(startInfo.FileName, std::strerror(childError));
        }
        process->_pid = pid;
        return process;
#endif
    }

    Process::~Process()
    {
        Dispose();
    }

    bool Process::HasExited()
    {
        if (_exited)
        {
            return true;
        }
#if defined(_WIN32)
        if (_handle == nullptr)
        {
            throw System::InvalidOperationException("No process is associated with this object.");
        }
        if (::WaitForSingleObject(_handle, 0) != WAIT_OBJECT_0)
        {
            return false;
        }
        DWORD code = 0;
        ::GetExitCodeProcess(_handle, &code);
        _exitCode = static_cast<std::int32_t>(code);
#else
        if (_pid < 0)
        {
            throw System::InvalidOperationException("No process is associated with this object.");
        }
        int status = 0;
        const pid_t result = ::waitpid(_pid, &status, WNOHANG);
        if (result == 0)
        {
            return false;
        }
        _reaped = true;
        _exitCode = result < 0 ? -1 : WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
        _exited = true;
        return true;
    }

    std::int32_t Process::ExitCode()
    {
        if (!HasExited())
        {
            throw System::InvalidOperationException("Process must exit before requested information can be determined.");
        }
        return _exitCode;
    }

    void Process::Kill(bool entireProcessTree)
    {
        if (HasExited())
        {
            return;
        }
#if defined(_WIN32)
        if (entireProcessTree)
        {
            KillTree(_id);
        }
        else
        {
            ::TerminateProcess(_handle, static_cast<UINT>(-1));
        }
#else
        if (entireProcessTree)
        {
            KillTree(_pid);
        }
        else
        {
            ::kill(_pid, SIGKILL);
        }
#endif
    }

    void Process::Dispose()
    {
#if defined(_WIN32)
        if (_handle != nullptr)
        {
            ::CloseHandle(_handle);
            _handle = nullptr;
        }
#else
        // A child nobody waits for any more is left to init, as .NET's
        // SIGCHLD handling does for a disposed Process.
        _pid = _exited ? -1 : _pid;
#endif
    }

    std::vector<std::int32_t> ActiveUdpListenerPorts()
    {
        std::vector<std::int32_t> ports;
#if defined(_WIN32)
        for (const ULONG family : {static_cast<ULONG>(AF_INET), static_cast<ULONG>(AF_INET6)})
        {
            DWORD size = 0;
            ::GetExtendedUdpTable(nullptr, &size, FALSE, family, UDP_TABLE_BASIC, 0);
            std::vector<unsigned char> buffer(size);
            if (::GetExtendedUdpTable(buffer.data(), &size, FALSE, family, UDP_TABLE_BASIC, 0) != NO_ERROR)
            {
                throw std::runtime_error("GetExtendedUdpTable failed");
            }
            if (family == AF_INET)
            {
                const auto* table = reinterpret_cast<const MIB_UDPTABLE*>(buffer.data());
                for (DWORD i = 0; i < table->dwNumEntries; i++)
                {
                    ports.push_back(::ntohs(static_cast<u_short>(table->table[i].dwLocalPort)));
                }
            }
            else
            {
                const auto* table = reinterpret_cast<const MIB_UDP6TABLE*>(buffer.data());
                for (DWORD i = 0; i < table->dwNumEntries; i++)
                {
                    ports.push_back(::ntohs(static_cast<u_short>(table->table[i].dwLocalPort)));
                }
            }
        }
#elif defined(__linux__)
        // /proc/net/udp and udp6: "sl local_address:port ...", hex.
        for (const char* path : {"/proc/net/udp", "/proc/net/udp6"})
        {
            std::ifstream file(path);
            std::string line;
            std::getline(file, line);
            while (std::getline(file, line))
            {
                std::istringstream fields(line);
                std::string slot;
                std::string local;
                fields >> slot >> local;
                const std::size_t colon = local.rfind(':');
                if (colon != std::string::npos)
                {
                    ports.push_back(static_cast<std::int32_t>(std::stoul(local.substr(colon + 1), nullptr, 16)));
                }
            }
        }
#else
        throw std::runtime_error("Operation is not supported on this platform.");
#endif
        return ports;
    }

    std::int32_t ProcessRunCaptureOutput(
        const std::string& fileName, const std::vector<std::string>& arguments, std::string& output)
    {
        output.clear();
#if defined(_WIN32)
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;
        HANDLE readOut = nullptr;
        HANDLE writeOut = nullptr;
        if (::CreatePipe(&readOut, &writeOut, &security, 0) == FALSE)
        {
            ThrowStartFailure(fileName, LastErrorMessage());
        }
        ::SetHandleInformation(readOut, HANDLE_FLAG_INHERIT, 0);
        const HANDLE nul = ::CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
            OPEN_EXISTING, 0, nullptr);
        std::wstring commandLine = PasteArgument(Wtf8ToWide(fileName));
        for (const std::string& argument : arguments)
        {
            commandLine += L' ' + PasteArgument(Wtf8ToWide(argument));
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = writeOut;
        startup.hStdError = nul;
        PROCESS_INFORMATION information{};
        const BOOL started = ::CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
            CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &information);
        ::CloseHandle(writeOut);
        if (nul != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(nul);
        }
        if (started == FALSE)
        {
            const std::string reason = LastErrorMessage();
            ::CloseHandle(readOut);
            ThrowStartFailure(fileName, reason);
        }
        ::CloseHandle(information.hThread);
        char buffer[4096];
        DWORD read = 0;
        while (::ReadFile(readOut, buffer, sizeof(buffer), &read, nullptr) != FALSE && read > 0)
        {
            output.append(buffer, read);
        }
        ::CloseHandle(readOut);
        ::WaitForSingleObject(information.hProcess, INFINITE);
        DWORD exitCode = 0;
        ::GetExitCodeProcess(information.hProcess, &exitCode);
        ::CloseHandle(information.hProcess);
        return static_cast<std::int32_t>(exitCode);
#else
        std::vector<std::string> strings;
        strings.push_back(fileName);
        strings.insert(strings.end(), arguments.begin(), arguments.end());
        std::vector<char*> argv;
        for (std::string& value : strings)
        {
            argv.push_back(value.data());
        }
        argv.push_back(nullptr);
        int out[2];
        int status[2];
        if (::pipe(out) != 0)
        {
            ThrowStartFailure(fileName, std::strerror(errno));
        }
        if (::pipe(status) != 0)
        {
            const int error = errno;
            ::close(out[0]);
            ::close(out[1]);
            ThrowStartFailure(fileName, std::strerror(error));
        }
        const pid_t pid = ::fork();
        if (pid < 0)
        {
            const int error = errno;
            ::close(out[0]);
            ::close(out[1]);
            ::close(status[0]);
            ::close(status[1]);
            ThrowStartFailure(fileName, std::strerror(error));
        }
        if (pid == 0)
        {
            ::close(out[0]);
            ::close(status[0]);
            ::fcntl(status[1], F_SETFD, FD_CLOEXEC);
            ::dup2(out[1], STDOUT_FILENO);
            const int devNull = ::open("/dev/null", O_WRONLY);
            if (devNull >= 0)
            {
                ::dup2(devNull, STDERR_FILENO);
            }
            ::execvp(argv[0], argv.data());
            const int error = errno;
            static_cast<void>(::write(status[1], &error, sizeof(error)));
            ::_exit(127);
        }
        ::close(out[1]);
        ::close(status[1]);
        int childError = 0;
        const ssize_t failed = ::read(status[0], &childError, sizeof(childError));
        ::close(status[0]);
        if (failed == sizeof(childError))
        {
            ::close(out[0]);
            ::waitpid(pid, nullptr, 0);
            ThrowStartFailure(fileName, std::strerror(childError));
        }
        char buffer[4096];
        ssize_t read = 0;
        while ((read = ::read(out[0], buffer, sizeof(buffer))) > 0)
        {
            output.append(buffer, static_cast<std::size_t>(read));
        }
        ::close(out[0]);
        int waitStatus = 0;
        ::waitpid(pid, &waitStatus, 0);
        return WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : 128 + WTERMSIG(waitStatus);
#endif
    }
}

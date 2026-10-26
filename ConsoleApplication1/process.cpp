#include "process.h"
#include <tlhelp32.h>
#include <vector>

namespace proc
{
    bool Launch(const std::wstring& commandLine, bool wait, DWORD* exitCode)
    {
        // CreateProcessW may modify the buffer, so it must be writable.
        std::vector<wchar_t> buf(commandLine.begin(), commandLine.end());
        buf.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
            return false;

        if (wait)
        {
            WaitForSingleObject(pi.hProcess, INFINITE);
            if (exitCode)
                GetExitCodeProcess(pi.hProcess, exitCode);
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    bool IsRunning(const wchar_t* exeName)
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        bool found = false;
        if (Process32FirstW(snap, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, exeName) == 0)
                {
                    found = true;
                    break;
                }
            } while (Process32NextW(snap, &entry));
        }

        CloseHandle(snap);
        return found;
    }
}
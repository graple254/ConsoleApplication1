#pragma once
#include "platform.h"
#include <string>

namespace proc
{
    // Start a new process from a command line, e.g. L"notepad.exe" or L"\"C:\\My Tool\\tool.exe\" --arg".
    // If wait is true, blocks until it exits and (optionally) returns its exit code.
    bool Launch(const std::wstring& commandLine, bool wait = false, DWORD* exitCode = nullptr);

    // True if a process with this exe name (e.g. L"notepad.exe") is currently running.
    bool IsRunning(const wchar_t* exeName);
}
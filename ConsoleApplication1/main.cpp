// main.cpp - program entry point. Keep this file small: it only wires modules together.
//
//   game.*     -> game logic + drawing
//   window.*   -> Win32 window, message loop, input forwarding
//   process.*  -> helpers to launch / check other processes
//
// Extension points:
//   * Startup checks (is another program running? launch a helper?) -> before window.Run()
//   * New systems (audio, settings, save files)                     -> new .cpp/.h, call them here
//   * In-game actions (see Game::OnKey in game.cpp, F1 launches Notepad as an example)

#include "platform.h"
#include "game.h"
#include "window.h"
#include "process.h"

// Build as a GUI (no console window) app regardless of project template settings.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Example startup hook:
    // if (proc::IsRunning(L"SomeTool.exe")) { /* react */ }
    // proc::Launch(L"helper.exe --flag");

    Game game;
    Window window(hInstance, game);

    if (!window.Create(L"Space Shooter", kWidth, kHeight, nCmdShow))
    {
        MessageBoxW(nullptr, L"Could not create the game window.", L"Error", MB_ICONERROR);
        return 1;
    }

    return window.Run();
}
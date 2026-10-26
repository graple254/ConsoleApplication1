#pragma once
#include "platform.h"

class Game;

class Window
{
public:
    Window(HINSTANCE hInstance, Game& game);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(const wchar_t* title, int clientWidth, int clientHeight, int nCmdShow);
    int Run();   // message loop + fixed game loop; returns exit code

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
    void Paint();

    HINSTANCE hInst_;
    HWND hwnd_ = nullptr;
    Game& game_;
    int width_ = 0, height_ = 0;

    // Back buffer to avoid flicker
    HDC memDC_ = nullptr;
    HBITMAP bmp_ = nullptr;
    HGDIOBJ oldBmp_ = nullptr;
};
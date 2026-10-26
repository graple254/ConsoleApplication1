// ===== CHANGED (v2) =====================================================
// Small edits only: crosshair cursor + forwarding mouse move / left-click
// to the game (WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP).
// ========================================================================
#include "window.h"
#include "game.h"

Window::Window(HINSTANCE hInstance, Game& game) : hInst_(hInstance), game_(game) {}

Window::~Window()
{
    if (memDC_)
    {
        SelectObject(memDC_, oldBmp_);
        DeleteDC(memDC_);
    }
    if (bmp_)
        DeleteObject(bmp_);
}

bool Window::Create(const wchar_t* title, int clientWidth, int clientHeight, int nCmdShow)
{
    width_ = clientWidth;
    height_ = clientHeight;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst_;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = L"SpaceShooterWindow";
    if (!RegisterClassExW(&wc))
        return false;

    // Fixed-size window: caption, close, minimize (no resize/maximize)
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r{ 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&r, style, FALSE);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hInst_, this);
    if (!hwnd_)
        return false;

    HDC dc = GetDC(hwnd_);
    memDC_ = CreateCompatibleDC(dc);
    bmp_ = CreateCompatibleBitmap(dc, clientWidth, clientHeight);
    oldBmp_ = SelectObject(memDC_, bmp_);
    ReleaseDC(hwnd_, dc);

    ShowWindow(hwnd_, nCmdShow);
    return true;
}

int Window::Run()
{
    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    MSG msg{};
    for (;;)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        last = now;
        if (dt > 0.05f) dt = 0.05f;   // avoid huge jumps after dragging the window

        game_.Update(dt);

        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);          // triggers WM_PAINT -> Paint()

        Sleep(1);                     // be nice to the CPU
    }
}

void Window::Paint()
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd_, &ps);
    if (memDC_)
    {
        game_.Render(memDC_, width_, height_);
        BitBlt(dc, 0, 0, width_, height_, memDC_, 0, 0, SRCCOPY);
    }
    EndPaint(hwnd_, &ps);
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Window* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    }
    else
    {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;   // we draw everything ourselves

    case WM_PAINT:
        Paint();
        return 0;

    case WM_KEYDOWN:
    {
        if (wp == VK_ESCAPE)
        {
            DestroyWindow(hwnd_);
            return 0;
        }
        const bool repeat = (lp & (1 << 30)) != 0;
        game_.OnKey(wp, true, repeat);
        return 0;
    }

    case WM_KEYUP:
        game_.OnKey(wp, false, false);
        return 0;

    case WM_MOUSEMOVE:
        game_.OnMouseMove((short)LOWORD(lp), (short)HIWORD(lp));
        return 0;

    case WM_LBUTTONDOWN:
        SetCapture(hwnd_);   // keep receiving the release even outside the window
        game_.OnMouseButton(true, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;

    case WM_LBUTTONUP:
        ReleaseCapture();
        game_.OnMouseButton(false, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;

    case WM_KILLFOCUS:
        game_.ClearInput();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}
// ===== CHANGED (v2) =====================================================
// New: start menu with Easy/Normal/Hard speed choice, mouse aiming + click
// to fire, power-ups (T triple shot, F fast fire, + extra life), explosion
// particles, best score per difficulty (saved to highscore.dat next to the
// exe), auto-pause when the window loses focus, heart-style lives display.
// ========================================================================
#include "game.h"
#include "process.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr float kPlayerSpeed = 380.f;
    constexpr float kBulletSpeed = 640.f;
    constexpr float kFireDelay = 0.20f;
    constexpr float kRapidDelay = 0.09f;
    constexpr float kInvulnTime = 1.5f;
    constexpr float kPowerTime = 8.f;
    constexpr int kStarCount = 80;

    struct Difficulty { const wchar_t* name; float speed; float spawn; int lives; };
    const Difficulty kDiff[3] = {
        { L"Easy",   0.7f, 1.4f, 5 },
        { L"Normal", 1.0f, 1.0f, 3 },
        { L"Hard",   1.4f, 0.7f, 2 },
    };

    RECT ButtonRect(int i)
    {
        const int w = 260, h = 46, gap = 14;
        const int x = (kWidth - w) / 2, y = 250 + i * (h + gap);
        return RECT{ x, y, x + w, y + h };
    }

    bool InRect(const RECT& r, int x, int y)
    {
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }

    void Box(HDC dc, float x, float y, float w, float h, COLORREF c)
    {
        SetDCBrushColor(dc, c);
        RECT r{ (LONG)x, (LONG)y, (LONG)(x + w), (LONG)(y + h) };
        FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
    }

    void Ship(HDC dc, float x, float y, float w, float h, COLORREF c)
    {
        POINT t[3] = { { (LONG)(x + w / 2), (LONG)y },{ (LONG)x, (LONG)(y + h) },{ (LONG)(x + w), (LONG)(y + h) } };
        SetDCBrushColor(dc, c);
        Polygon(dc, t, 3);
    }

    void Draw(HDC dc, RECT r, const wchar_t* s, COLORREF c, UINT flags)
    {
        SetTextColor(dc, c);
        DrawTextW(dc, s, -1, &r, flags | DT_VCENTER | DT_SINGLELINE);
    }

    bool Overlap(const Entity& a, const Entity& b)
    {
        return a.x < b.x + b.w && a.x + a.w > b.x &&
            a.y < b.y + b.h && a.y + a.h > b.y;
    }

    template <class T>
    void RemoveDead(std::vector<T>& v)
    {
        v.erase(std::remove_if(v.begin(), v.end(), [](const T& e) { return e.dead; }), v.end());
    }

    std::wstring ScorePath()
    {
        wchar_t p[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH);
        std::wstring s(p, n);
        size_t i = s.find_last_of(L"\\/");
        return s.substr(0, i == std::wstring::npos ? 0 : i + 1) + L"highscore.dat";
    }
}

// ---------------------------------------------------------------- setup

Game::Game() : rng_(std::random_device{}())
{
    bigFont_ = CreateFontW(44, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    for (int i = 0; i < kStarCount; ++i)
        stars_.push_back({ Rand(0, (float)kWidth), Rand(0, (float)kHeight), Rand(20.f, 130.f) });

    LoadScores();
    Reset();
    state_ = State::Menu;   // start on the menu
}

Game::~Game()
{
    if (bigFont_)
        DeleteObject(bigFont_);
}

float Game::Rand(float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(rng_);
}

void Game::LoadScores()
{
    HANDLE h = CreateFileW(ScorePath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    int tmp[3];
    DWORD n = 0;
    if (ReadFile(h, tmp, sizeof(tmp), &n, nullptr) && n == sizeof(tmp))
        for (int i = 0; i < 3; ++i)
            best_[i] = std::max(0, tmp[i]);
    CloseHandle(h);
}

void Game::SaveScores()
{
    HANDLE h = CreateFileW(ScorePath().c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD n = 0;
    WriteFile(h, best_, sizeof(best_), &n, nullptr);
    CloseHandle(h);
}

void Game::Reset()
{
    player_ = Entity();
    player_.w = 40;
    player_.h = 40;
    player_.x = kWidth / 2.f - player_.w / 2.f;
    player_.y = kHeight - 70.f;

    bullets_.clear();
    enemies_.clear();
    powerups_.clear();
    particles_.clear();
    score_ = 0;
    lives_ = kDiff[diff_].lives;
    fireCooldown_ = 0;
    spawnTimer_ = 0.5f;
    invuln_ = elapsed_ = 0;
    tripleTimer_ = rapidTimer_ = overTimer_ = 0;
    newBest_ = false;
    state_ = State::Playing;
}

void Game::StartGame(int difficulty)
{
    diff_ = difficulty;
    Reset();
}

void Game::GoToMenu()
{
    state_ = State::Menu;
    mouseDown_ = false;
}

void Game::EndGame()
{
    state_ = State::GameOver;
    overTimer_ = 0.8f;   // brief delay so a click/keypress doesn't instantly restart
    mouseDown_ = false;
    if (score_ > best_[diff_])
    {
        best_[diff_] = score_;
        newBest_ = true;
        SaveScores();
    }
}

// ---------------------------------------------------------------- input

void Game::ClearInput()
{
    left_ = right_ = fire_ = mouseDown_ = false;
    if (state_ == State::Playing)
        state_ = State::Paused;   // alt-tab safety
}

void Game::OnKey(WPARAM vk, bool down, bool repeat)
{
    switch (vk)
    {
    case VK_LEFT:  case 'A': left_ = down;  if (down) mouseActive_ = false; break;
    case VK_RIGHT: case 'D': right_ = down; if (down) mouseActive_ = false; break;
    case VK_SPACE:           fire_ = down;  break;
    }

    if (!down || repeat)
        return;

    if (vk == VK_F1)   // demo: spawn another process
    {
        proc::Launch(L"notepad.exe");
        return;
    }

    switch (state_)
    {
    case State::Menu:
        if (vk >= '1' && vk <= '3')            StartGame((int)(vk - '1'));
        else if (vk == VK_UP)                  diff_ = (diff_ + 2) % 3;
        else if (vk == VK_DOWN)                diff_ = (diff_ + 1) % 3;
        else if (vk == VK_RETURN || vk == VK_SPACE) StartGame(diff_);
        break;

    case State::Playing:
        if (vk == 'P') state_ = State::Paused;
        else if (vk == 'R') Reset();
        else if (vk == 'M') GoToMenu();
        break;

    case State::Paused:
        if (vk == 'P') state_ = State::Playing;
        else if (vk == 'R') Reset();
        else if (vk == 'M') GoToMenu();
        break;

    case State::GameOver:
        if (overTimer_ <= 0 && (vk == 'R' || vk == VK_RETURN)) Reset();
        else if (vk == 'M') GoToMenu();
        break;
    }
}

void Game::OnMouseMove(int x, int y)
{
    mouseX_ = x;
    mouseY_ = y;
    mouseActive_ = true;
}

void Game::OnMouseButton(bool down, int x, int y)
{
    mouseX_ = x;
    mouseY_ = y;

    if (!down)
    {
        mouseDown_ = false;
        return;
    }

    switch (state_)
    {
    case State::Menu:
        for (int i = 0; i < 3; ++i)
            if (InRect(ButtonRect(i), x, y))
                StartGame(i);
        break;
    case State::Paused:
        state_ = State::Playing;
        break;
    case State::GameOver:
        if (overTimer_ <= 0) Reset();
        break;
    case State::Playing:
        mouseDown_ = true;
        mouseActive_ = true;
        break;
    }
}

// ---------------------------------------------------------------- gameplay

void Game::SpawnEnemy()
{
    Entity e;
    e.w = 34;
    e.h = 30;
    e.x = Rand(0.f, kWidth - e.w);
    e.y = -e.h;
    e.vx = Rand(-60.f, 60.f) * kDiff[diff_].speed;
    e.vy = (Rand(90.f, 170.f) + elapsed_ * 2.f) * kDiff[diff_].speed;
    enemies_.push_back(e);
}

void Game::Fire()
{
    auto add = [&](float vx)
        {
            Entity b;
            b.w = 4;
            b.h = 14;
            b.x = player_.x + player_.w / 2.f - b.w / 2.f;
            b.y = player_.y - b.h;
            b.vx = vx;
            b.vy = -kBulletSpeed;
            bullets_.push_back(b);
        };

    if (tripleTimer_ > 0) { add(-120.f); add(0.f); add(120.f); }
    else { add(0.f); }
}

void Game::Burst(float x, float y, int count, COLORREF color)
{
    for (int i = 0; i < count; ++i)
    {
        float a = Rand(0.f, 6.2832f), s = Rand(40.f, 240.f);
        particles_.push_back({ x, y, std::cos(a) * s, std::sin(a) * s, 1.f, color });
    }
}

void Game::Update(float dt)
{
    // Background and particles always animate
    for (Star& s : stars_)
    {
        s.y += s.speed * dt;
        if (s.y > kHeight)
        {
            s.y = 0;
            s.x = Rand(0, (float)kWidth);
        }
    }
    for (Particle& p : particles_)
    {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt * 1.8f;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const Particle& p) { return p.life <= 0; }), particles_.end());

    if (overTimer_ > 0) overTimer_ -= dt;
    if (state_ != State::Playing)
        return;

    elapsed_ += dt;
    if (invuln_ > 0) invuln_ -= dt;
    if (tripleTimer_ > 0) tripleTimer_ -= dt;
    if (rapidTimer_ > 0) rapidTimer_ -= dt;

    // Player: keyboard wins when pressed, otherwise follow the mouse
    float dir = (right_ ? 1.f : 0.f) - (left_ ? 1.f : 0.f);
    if (dir != 0.f)
    {
        player_.x += dir * kPlayerSpeed * dt;
    }
    else if (mouseActive_)
    {
        float diff = (mouseX_ - player_.w / 2.f) - player_.x;
        float step = kPlayerSpeed * 1.6f * dt;
        player_.x += (std::fabs(diff) <= step) ? diff : (diff > 0 ? step : -step);
    }
    player_.x = std::max(0.f, std::min(player_.x, kWidth - player_.w));

    fireCooldown_ -= dt;
    if ((fire_ || mouseDown_) && fireCooldown_ <= 0)
    {
        Fire();
        fireCooldown_ = rapidTimer_ > 0 ? kRapidDelay : kFireDelay;
    }

    // Spawning
    spawnTimer_ -= dt;
    if (spawnTimer_ <= 0)
    {
        SpawnEnemy();
        spawnTimer_ = std::max(0.2f, (1.0f - elapsed_ * 0.01f) * kDiff[diff_].spawn);
    }

    // Movement
    for (Entity& b : bullets_)
    {
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        if (b.y + b.h < 0 || b.x < -10 || b.x > kWidth + 10) b.dead = true;
    }
    for (Entity& e : enemies_)
    {
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        if (e.x < 0) { e.x = 0;            e.vx = -e.vx; }
        if (e.x + e.w > kWidth) { e.x = kWidth - e.w; e.vx = -e.vx; }
        if (e.y > kHeight)      e.dead = true;
    }
    for (Entity& p : powerups_)
    {
        p.y += p.vy * dt;
        if (p.y > kHeight) p.dead = true;
    }

    // Bullet vs enemy
    for (Entity& b : bullets_)
    {
        for (Entity& e : enemies_)
        {
            if (b.dead || e.dead || !Overlap(b, e))
                continue;

            b.dead = e.dead = true;
            score_ += 10;
            Burst(e.x + e.w / 2, e.y + e.h / 2, 12, RGB(255, 150, 60));

            if (Rand(0.f, 1.f) < 0.12f)   // chance to drop a power-up
            {
                Entity p;
                p.w = p.h = 22;
                p.x = e.x + e.w / 2 - 11;
                p.y = e.y;
                p.vy = 110;
                p.kind = Rand(0.f, 1.f) < 0.2f ? 2 : (Rand(0.f, 1.f) < 0.5f ? 0 : 1);
                powerups_.push_back(p);
            }
        }
    }

    // Enemy vs player
    if (invuln_ <= 0)
    {
        for (Entity& e : enemies_)
        {
            if (!e.dead && Overlap(e, player_))
            {
                e.dead = true;
                invuln_ = kInvulnTime;
                Burst(player_.x + player_.w / 2, player_.y + player_.h / 2, 20, RGB(255, 80, 80));
                if (--lives_ <= 0)
                    EndGame();
                break;
            }
        }
    }

    // Power-up pickup
    for (Entity& p : powerups_)
    {
        if (p.dead || !Overlap(p, player_))
            continue;
        p.dead = true;
        if (p.kind == 0)      tripleTimer_ = kPowerTime;
        else if (p.kind == 1) rapidTimer_ = kPowerTime;
        else                  lives_ = std::min(lives_ + 1, 9);
        Burst(p.x + 11, p.y + 11, 8, RGB(120, 255, 160));
    }

    RemoveDead(bullets_);
    RemoveDead(enemies_);
    RemoveDead(powerups_);
}

// ---------------------------------------------------------------- drawing

void Game::RenderMenu(HDC dc, int width, int height)
{
    SelectObject(dc, bigFont_);
    Draw(dc, RECT{ 0, 110, width, 190 }, L"SPACE SHOOTER", RGB(80, 200, 255), DT_CENTER);

    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    Draw(dc, RECT{ 0, 195, width, 235 }, L"Choose your speed to start", RGB(200, 200, 220), DT_CENTER);

    for (int i = 0; i < 3; ++i)
    {
        RECT r = ButtonRect(i);
        bool hot = InRect(r, mouseX_, mouseY_) || i == diff_;
        SetDCBrushColor(dc, hot ? RGB(60, 100, 190) : RGB(30, 36, 70));
        FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));

        wchar_t label[48], best[32];
        wsprintfW(label, L"%d   %s", i + 1, kDiff[i].name);
        wsprintfW(best, L"Best: %d", best_[i]);
        RECT inner{ r.left + 16, r.top, r.right - 16, r.bottom };
        Draw(dc, inner, label, RGB(255, 255, 255), DT_LEFT);
        Draw(dc, inner, best, RGB(170, 180, 210), DT_RIGHT);
    }

    const wchar_t* lines[] = {
        L"Mouse: move your ship, hold left click to fire",
        L"Keyboard: arrows or A / D to move, Space to fire",
        L"Power-ups: T triple shot   F fast fire   + extra life",
        L"P pause    M menu    R restart    F1 Notepad    Esc quit",
    };
    for (int i = 0; i < 4; ++i)
        Draw(dc, RECT{ 0, 440 + i * 24, width, 464 + i * 24 }, lines[i], RGB(150, 150, 175), DT_CENTER);
    (void)height;
}

void Game::Render(HDC dc, int width, int height)
{
    RECT full{ 0, 0, width, height };
    SetDCBrushColor(dc, RGB(8, 8, 22));
    FillRect(dc, &full, (HBRUSH)GetStockObject(DC_BRUSH));

    SelectObject(dc, GetStockObject(DC_BRUSH));
    SelectObject(dc, GetStockObject(NULL_PEN));
    SetBkMode(dc, TRANSPARENT);

    for (const Star& s : stars_)
    {
        int v = std::min(255, 70 + (int)s.speed);
        Box(dc, s.x, s.y, 2, 2, RGB(v, v, v));
    }

    if (state_ == State::Menu)
    {
        RenderMenu(dc, width, height);
        return;
    }

    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));

    for (const Entity& b : bullets_)
        Box(dc, b.x, b.y, b.w, b.h, RGB(255, 230, 80));

    for (const Entity& e : enemies_)
    {
        Box(dc, e.x, e.y, e.w, e.h, RGB(220, 60, 60));
        Box(dc, e.x + 6, e.y + 8, 7, 7, RGB(20, 20, 20));
        Box(dc, e.x + e.w - 13, e.y + 8, 7, 7, RGB(20, 20, 20));
    }

    for (const Entity& p : powerups_)
    {
        static const COLORREF col[3] = { RGB(80, 220, 255), RGB(255, 220, 60), RGB(90, 230, 120) };
        static const wchar_t* sym[3] = { L"T", L"F", L"+" };
        Box(dc, p.x, p.y, p.w, p.h, col[p.kind]);
        Draw(dc, RECT{ (LONG)p.x, (LONG)p.y, (LONG)(p.x + p.w), (LONG)(p.y + p.h) }, sym[p.kind], RGB(10, 10, 30), DT_CENTER);
    }

    for (const Particle& p : particles_)
    {
        float s = 1.f + 3.f * p.life;
        Box(dc, p.x, p.y, s, s, p.color);
    }

    bool visible = invuln_ <= 0 || std::fmod(invuln_ * 10.f, 2.f) < 1.f;
    if (visible && state_ != State::GameOver)
    {
        Ship(dc, player_.x, player_.y, player_.w, player_.h, RGB(80, 200, 255));
        Box(dc, player_.x + player_.w / 2 - 4, player_.y + player_.h / 2, 8, 12, RGB(255, 255, 255));
    }

    // HUD
    wchar_t buf[128];
    wsprintfW(buf, L"Score: %d    Best: %d    [%s]", score_, best_[diff_], kDiff[diff_].name);
    Draw(dc, RECT{ 10, 6, 400, 26 }, buf, RGB(255, 255, 255), DT_LEFT);

    for (int i = 0; i < lives_; ++i)
        Ship(dc, (float)(width - 34 - i * 26), 10.f, 20.f, 18.f, RGB(255, 90, 110));

    buf[0] = L'\0';
    if (tripleTimer_ > 0) wsprintfW(buf, L"TRIPLE SHOT %d", (int)std::ceil(tripleTimer_));
    if (rapidTimer_ > 0)
    {
        wchar_t t[48];
        wsprintfW(t, L"%sFAST FIRE %d", buf[0] ? L"   " : L"", (int)std::ceil(rapidTimer_));
        lstrcatW(buf, t);
    }
    if (buf[0])
        Draw(dc, RECT{ 0, 30, width, 50 }, buf, RGB(255, 230, 120), DT_CENTER);

    Draw(dc, RECT{ 10, height - 24, width, height - 4 },
        L"Mouse/Arrows: move   Click/Space: fire   P: pause   M: menu   R: restart   F1: Notepad   Esc: quit",
        RGB(150, 150, 170), DT_LEFT);

    // Overlays
    const wchar_t* title = nullptr;
    wchar_t sub[96] = L"";
    if (state_ == State::GameOver)
    {
        title = L"GAME OVER";
        wsprintfW(sub, L"Score %d%s     R / click: retry     M: menu", score_, newBest_ ? L"  -  NEW BEST!" : L"");
    }
    else if (state_ == State::Paused)
    {
        title = L"PAUSED";
        lstrcpyW(sub, L"Click or press P to resume");
    }

    if (title)
    {
        SelectObject(dc, bigFont_);
        Draw(dc, RECT{ 0, height / 2 - 60, width, height / 2 }, title, RGB(255, 255, 255), DT_CENTER);
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        Draw(dc, RECT{ 0, height / 2 + 5, width, height / 2 + 35 }, sub, RGB(200, 200, 220), DT_CENTER);
    }
}
// ===== CHANGED (v2) =====================================================
// Added: game states (menu/playing/paused/game over), difficulty choice,
// mouse input, power-ups, particles, saved best scores.
// ========================================================================
#pragma once
#include "platform.h"
#include <random>
#include <vector>

constexpr int kWidth = 800;   // client area size
constexpr int kHeight = 600;

struct Entity
{
    float x = 0, y = 0, w = 0, h = 0;
    float vx = 0, vy = 0;
    int kind = 0;             // power-up type: 0 triple, 1 fast fire, 2 extra life
    bool dead = false;
};

struct Particle
{
    float x, y, vx, vy, life;   // life goes 1 -> 0
    COLORREF color;
};

enum class State { Menu, Playing, Paused, GameOver };

class Game
{
public:
    Game();
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void Reset();                               // restart with current difficulty
    void Update(float dt);                      // dt in seconds
    void Render(HDC dc, int width, int height);
    void OnKey(WPARAM vk, bool down, bool repeat);
    void OnMouseMove(int x, int y);
    void OnMouseButton(bool down, int x, int y);
    void ClearInput();                          // on focus loss (also auto-pauses)

private:
    struct Star { float x, y, speed; };

    float Rand(float lo, float hi);
    void SpawnEnemy();
    void Fire();
    void Burst(float x, float y, int count, COLORREF color);
    void StartGame(int difficulty);
    void GoToMenu();
    void EndGame();
    void LoadScores();
    void SaveScores();
    void RenderMenu(HDC dc, int width, int height);

    Entity player_;
    std::vector<Entity> bullets_, enemies_, powerups_;
    std::vector<Particle> particles_;
    std::vector<Star> stars_;

    State state_ = State::Menu;
    int diff_ = 1;                              // 0 easy, 1 normal, 2 hard
    bool left_ = false, right_ = false, fire_ = false;
    bool mouseDown_ = false, mouseActive_ = false;
    int mouseX_ = kWidth / 2, mouseY_ = 0;

    int score_ = 0, lives_ = 0;
    int best_[3] = { 0, 0, 0 };
    bool newBest_ = false;
    float fireCooldown_ = 0, spawnTimer_ = 0, invuln_ = 0, elapsed_ = 0;
    float tripleTimer_ = 0, rapidTimer_ = 0, overTimer_ = 0;

    std::mt19937 rng_;
    HFONT bigFont_ = nullptr;
};
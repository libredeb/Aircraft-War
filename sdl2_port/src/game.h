#pragma once

#include "entity.h"
#include "resources.h"
#include <vector>
#include <string>

enum class GameState { MainMenu, Playing, Paused, GameOver, Settings, About };

struct SpawnTimer {
    float interval;
    float elapsed;
};

class Game {
public:
    bool init(const std::string& dataPath, int screenW, int screenH, bool fullscreen);
    void run();
    void shutdown();

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    Resources m_res;
    std::string m_dataPath;
    int m_screenW = 720, m_screenH = 720;
    float m_scale = 1.5f;

    GameState m_state = GameState::MainMenu;
    bool m_running = true;

    // Keyboard held state
    bool m_kbUp = false, m_kbDown = false, m_kbLeft = false, m_kbRight = false;
    bool m_kbConfirm = false, m_kbBomb = false, m_kbPause = false, m_kbBack = false;

    // Combined held state (keyboard OR pad)
    bool m_keyUp = false, m_keyDown = false, m_keyLeft = false, m_keyRight = false;
    bool m_keyConfirm = false, m_keyBomb = false, m_keyPause = false, m_keyBack = false;

    // Edge-triggered presses (one frame)
    bool m_keyConfirmPressed = false, m_keyBombPressed = false;
    bool m_keyPausePressed = false, m_keyBackPressed = false;
    bool m_keyUpPressed = false, m_keyDownPressed = false;
    bool m_keyLeftPressed = false, m_keyRightPressed = false;

    // Pad edge detection previous frame
    bool m_padUp = false, m_padDown = false, m_padLeft = false, m_padRight = false;
    bool m_padConfirm = false, m_padBomb = false, m_padPause = false, m_padBack = false;

    // Analog axes (updated every frame for low-latency movement)
    float m_axisX = 0.0f;
    float m_axisY = 0.0f;

    // Controllers: prefer GameController API, fallback to raw Joystick
    SDL_GameController* m_controller = nullptr;
    SDL_Joystick* m_joystick = nullptr;
    SDL_JoystickID m_joystickId = -1;
    static constexpr float AXIS_DEADZONE = 0.12f;
    // Matte charcoal for UI text on light backgrounds (softer than pure black)
    static constexpr SDL_Color UI_MATTE{58, 58, 58, 255};
    static constexpr SDL_Color UI_MATTE_SELECTED{40, 40, 40, 255};

    // Game entities
    Player m_player;
    std::vector<Bullet> m_bullets;
    std::vector<Enemy> m_enemies;
    std::vector<Prop> m_props;

    // Game state
    int m_score = 0;
    int m_bombs = 0;
    int m_grade = 1;
    int m_bulletType = 1;
    float m_bulletTimer = 0;
    float m_doubleBulletTimeLeft = -1;
    float m_enemyFreeTimeLeft = -1;
    bool m_propsActive = false;
    float m_propTimer = 0;
    float m_bgScrollY1 = 0, m_bgScrollY2 = 0;

    // Spawning
    SpawnTimer m_enemyTimers[9];
    bool m_grade2Timers = false;

    // Menu
    int m_menuSelection = 0;
    int m_menuItemCount = 0;

    // Sound/music settings
    bool m_soundOn = true;
    bool m_musicOn = true;

    // Core loop
    void processInput();
    void update(float dt);
    void render();

    // Input helpers
    void loadControllerMappings();
    void openPreferredController();
    void closeController();
    void pollPadState();
    void pollAxesOnly();
    void applyPlayerMovement(float dt);
    void applyEdge(bool now, bool& held, bool& pressed);
    bool isPreferredController(int deviceIndex) const;
    static float axisNorm(Sint16 value);
    void movementVector(float& outX, float& outY) const;

    // State updates
    void updatePlaying(float dt);
    void updateMenu(float dt);
    void updatePaused(float dt);
    void updateGameOver(float dt);
    void updateSettings(float dt);

    // Gameplay
    void startGame();
    void pauseGame();
    void resumeGame();
    void endGame();
    void upgradeGrade();
    void spawnEnemy(int type, int speedVariant);
    void spawnBullet();
    void spawnProp();
    void useBomb();
    void checkCollisions();
    bool frectOverlap(const SDL_FRect& a, const SDL_FRect& b) const;

    // Rendering
    void renderBackground();
    void renderEntities();
    void renderHUD();
    void renderMainMenu();
    void renderPauseMenu();
    void renderGameOverScreen();
    void renderSettingsMenu();
    void renderAboutScreen();
    void drawTextCentered(const std::string& text, int fontSize, SDL_Color color, int y);
    void drawTextureCentered(const std::string& name, int y, float scale);
    void drawMenuItems(const std::vector<std::string>& items, int selectedIdx, int startY);
    void drawImageButton(const std::string& label, int y, bool selected, float btnScale = 1.0f);

    // Helpers
    void initPlayerSize();
    std::string texNameForEnemy(int type, int frame) const;
    std::string texNameForPlayer(int frame) const;
    float enemyBaseSpeed(int type, int variant) const;
    float enemySpawnInterval(int type, int variant) const;
    void setSpawnIntervals();
    void resetEdgeInput();
    void removeDeadEntities();
    void loadSettings();
    void saveSettings();
};

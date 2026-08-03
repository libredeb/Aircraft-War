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
    int m_screenW = 720, m_screenH = 720;
    float m_scale = 1.5f;

    GameState m_state = GameState::MainMenu;
    bool m_running = true;

    // Input state
    bool m_keyUp = false, m_keyDown = false, m_keyLeft = false, m_keyRight = false;
    bool m_keyConfirm = false, m_keyBomb = false, m_keyPause = false, m_keyBack = false;
    bool m_keyConfirmPressed = false, m_keyBombPressed = false;
    bool m_keyPausePressed = false, m_keyBackPressed = false;
    bool m_keyUpPressed = false, m_keyDownPressed = false;
    SDL_GameController* m_controller = nullptr;

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

    // Helpers
    void initPlayerSize();
    std::string texNameForEnemy(int type, int frame) const;
    std::string texNameForPlayer(int frame) const;
    float enemyBaseSpeed(int type, int variant) const;
    float enemySpawnInterval(int type, int variant) const;
    void setSpawnIntervals();
    void resetInput();
    void removeDeadEntities();
    void loadSettings();
    void saveSettings();
};

#include "game.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>

static const float BG_SCROLL_SPEED = 36.0f;  // px/sec (original ~720/20s)
static const float BULLET_INTERVAL = 0.23f;
static const float PROP_INTERVAL = 30.0f;
static const float DOUBLE_BULLET_DURATION = 15.0f;
static const float BOMB_FREE_DURATION = 2.0f;
static const int   MAX_BOMBS = 3;

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

bool Game::init(const std::string& dataPath, int screenW, int screenH, bool fullscreen) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        return false;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
        return false;
    }
    Mix_AllocateChannels(24);
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }

    m_dataPath = dataPath;
    m_screenW = screenW;
    m_screenH = screenH;
    m_scale = m_screenW / 480.0f;

    Uint32 flags = SDL_WINDOW_SHOWN;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;

    m_window = SDL_CreateWindow("Aircraft War", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                m_screenW, m_screenH, flags);
    if (!m_window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        return false;
    }
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_RenderSetVSync(m_renderer, 0);
#endif

    SDL_ShowCursor(SDL_DISABLE);

    loadControllerMappings();
    openPreferredController();

    m_res.init(m_renderer, dataPath);

    srand(static_cast<unsigned>(time(nullptr)));

    loadSettings();
    m_res.setSoundEnabled(m_soundOn);
    m_res.setMusicEnabled(m_musicOn);

    m_bgScrollY1 = 0;
    m_bgScrollY2 = static_cast<float>(-m_screenH);

    return true;
}

void Game::shutdown() {
    saveSettings();
    closeController();
    m_res.shutdown();
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    TTF_Quit();
    Mix_CloseAudio();
    IMG_Quit();
    SDL_Quit();
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void Game::run() {
    Uint32 lastTick = SDL_GetTicks();
    while (m_running) {
        Uint32 frameStart = SDL_GetTicks();
        float dt = (frameStart - lastTick) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.0f) dt = 0.0f;
        lastTick = frameStart;

        processInput();
        update(dt);
        render();

        // Cap ~60fps without VSync blocking (lower input latency)
        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void Game::resetEdgeInput() {
    m_keyConfirmPressed = false;
    m_keyBombPressed = false;
    m_keyPausePressed = false;
    m_keyBackPressed = false;
    m_keyUpPressed = false;
    m_keyDownPressed = false;
    m_keyLeftPressed = false;
    m_keyRightPressed = false;
}

void Game::applyEdge(bool now, bool& held, bool& pressed) {
    if (now && !held) pressed = true;
    held = now;
}

float Game::axisNorm(Sint16 value) {
    return value / 32767.0f;
}

void Game::loadControllerMappings() {
    const std::string paths[] = {
        m_dataPath + "/gamecontrollerdb.txt",
        m_dataPath + "/data/gamecontrollerdb.txt",
        "data/gamecontrollerdb.txt",
        "../data/gamecontrollerdb.txt",
        "sdl2_port/data/gamecontrollerdb.txt",
    };
    for (const auto& path : paths) {
        int n = SDL_GameControllerAddMappingsFromFile(path.c_str());
        if (n >= 0) {
            fprintf(stderr, "Loaded %d gamecontroller mappings from %s\n", n, path.c_str());
            return;
        }
    }
    fprintf(stderr, "Warning: gamecontrollerdb.txt not found (using SDL built-in mappings)\n");
}

bool Game::isPreferredController(int deviceIndex) const {
    const char* name = SDL_JoystickNameForIndex(deviceIndex);
    if (!name) return false;
    std::string n(name);
    // Priority target: Arduino Leonardo (GamerCard)
    if (n.find("Arduino Leonardo") != std::string::npos) return true;
    if (n.find("Leonardo") != std::string::npos) return true;
    // GUID match for Arduino Leonardo mapping
    char guidStr[64];
    SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(deviceIndex);
    SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
    return std::string(guidStr).find("03000000412300003680000001010000") == 0;
}

void Game::closeController() {
    if (m_controller) {
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
    }
    if (m_joystick) {
        SDL_JoystickClose(m_joystick);
        m_joystick = nullptr;
    }
    m_joystickId = -1;
    m_padUp = m_padDown = m_padLeft = m_padRight = false;
    m_padConfirm = m_padBomb = m_padPause = m_padBack = false;
}

void Game::openPreferredController() {
    closeController();

    int preferred = -1;
    int anyController = -1;
    int anyJoystick = -1;

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (isPreferredController(i)) {
            preferred = i;
            break;
        }
        if (anyController < 0 && SDL_IsGameController(i)) anyController = i;
        if (anyJoystick < 0) anyJoystick = i;
    }

    int idx = (preferred >= 0) ? preferred
            : (anyController >= 0) ? anyController
            : anyJoystick;

    if (idx < 0) {
        fprintf(stderr, "No joystick/gamepad connected\n");
        return;
    }

    // Arduino Leonardo: open as raw joystick for lowest D-pad latency
    // (axis D-pad without GameController translation layer)
    if (preferred >= 0) {
        m_joystick = SDL_JoystickOpen(preferred);
        if (m_joystick) {
            m_joystickId = SDL_JoystickInstanceID(m_joystick);
            SDL_JoystickEventState(SDL_ENABLE);
            fprintf(stderr, "Opened preferred joystick (low-latency): %s (axes=%d buttons=%d)\n",
                    SDL_JoystickName(m_joystick),
                    SDL_JoystickNumAxes(m_joystick),
                    SDL_JoystickNumButtons(m_joystick));
            return;
        }
    }

    if (SDL_IsGameController(idx)) {
        m_controller = SDL_GameControllerOpen(idx);
        if (m_controller) {
            m_joystickId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller));
            fprintf(stderr, "Opened game controller: %s\n", SDL_GameControllerName(m_controller));
            return;
        }
    }

    // Raw joystick fallback
    m_joystick = SDL_JoystickOpen(idx);
    if (m_joystick) {
        m_joystickId = SDL_JoystickInstanceID(m_joystick);
        fprintf(stderr, "Opened raw joystick: %s (axes=%d buttons=%d)\n",
                SDL_JoystickName(m_joystick),
                SDL_JoystickNumAxes(m_joystick),
                SDL_JoystickNumButtons(m_joystick));
    }
}

void Game::pollPadState() {
    bool up = false, down = false, left = false, right = false;
    bool confirm = false, bomb = false, pause = false, back = false;
    float ax = 0.0f, ay = 0.0f;

    if (m_controller) {
        // Prefer raw axes first (Arduino Leonardo D-pad is axis-mapped) — lowest latency
        ax = axisNorm(SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX));
        ay = axisNorm(SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY));

        if (ay < -AXIS_DEADZONE) up = true;
        if (ay >  AXIS_DEADZONE) down = true;
        if (ax < -AXIS_DEADZONE) left = true;
        if (ax >  AXIS_DEADZONE) right = true;

        // Hat / digital D-pad buttons (other controllers)
        if (SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    up = true;
        if (SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  down = true;
        if (SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  left = true;
        if (SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) right = true;

        confirm = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_A);
        bomb = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_B)
            || SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_X)
            || SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_Y)
            || SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
            || SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        pause = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_START);
        back  = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_BACK);
    } else if (m_joystick) {
        if (SDL_JoystickNumAxes(m_joystick) >= 2) {
            ax = axisNorm(SDL_JoystickGetAxis(m_joystick, 0));
            ay = axisNorm(SDL_JoystickGetAxis(m_joystick, 1));
            if (ay < -AXIS_DEADZONE) up = true;
            if (ay >  AXIS_DEADZONE) down = true;
            if (ax < -AXIS_DEADZONE) left = true;
            if (ax >  AXIS_DEADZONE) right = true;
        }
        auto btn = [&](int b) {
            return b < SDL_JoystickNumButtons(m_joystick) && SDL_JoystickGetButton(m_joystick, b);
        };
        confirm = btn(0);
        bomb = btn(1) || btn(3) || btn(4) || btn(5) || btn(6);
        back = btn(10);
        pause = btn(11);
    }

    // Snap near-digital axes to ±1 for snappier D-pad feel (Leonardo)
    if (std::fabs(ax) > 0.7f) ax = (ax > 0.0f) ? 1.0f : -1.0f;
    else if (std::fabs(ax) < AXIS_DEADZONE) ax = 0.0f;
    if (std::fabs(ay) > 0.7f) ay = (ay > 0.0f) ? 1.0f : -1.0f;
    else if (std::fabs(ay) < AXIS_DEADZONE) ay = 0.0f;

    m_axisX = ax;
    m_axisY = ay;

    applyEdge(up, m_padUp, m_keyUpPressed);
    applyEdge(down, m_padDown, m_keyDownPressed);
    applyEdge(left, m_padLeft, m_keyLeftPressed);
    applyEdge(right, m_padRight, m_keyRightPressed);
    applyEdge(confirm, m_padConfirm, m_keyConfirmPressed);
    applyEdge(bomb, m_padBomb, m_keyBombPressed);
    applyEdge(pause, m_padPause, m_keyPausePressed);
    applyEdge(back, m_padBack, m_keyBackPressed);

    m_keyUp = m_kbUp || up;
    m_keyDown = m_kbDown || down;
    m_keyLeft = m_kbLeft || left;
    m_keyRight = m_kbRight || right;
    m_keyConfirm = m_kbConfirm || confirm;
    m_keyBomb = m_kbBomb || bomb;
    m_keyPause = m_kbPause || pause;
    m_keyBack = m_kbBack || back;
}

void Game::movementVector(float& outX, float& outY) const {
    float dx = 0.0f, dy = 0.0f;

    // Analog axes take priority when active (lowest latency path for Leonardo)
    if (std::fabs(m_axisX) > AXIS_DEADZONE || std::fabs(m_axisY) > AXIS_DEADZONE) {
        dx = m_axisX;
        dy = m_axisY;
    } else {
        if (m_keyLeft)  dx -= 1.0f;
        if (m_keyRight) dx += 1.0f;
        if (m_keyUp)    dy -= 1.0f;
        if (m_keyDown)  dy += 1.0f;
    }

    // Keyboard always merges so both can be used together
    if (m_kbLeft)  dx = -1.0f;
    if (m_kbRight) dx =  1.0f;
    if (m_kbUp)    dy = -1.0f;
    if (m_kbDown)  dy =  1.0f;

    float mag = std::sqrt(dx * dx + dy * dy);
    if (mag > 1.0f) {
        dx /= mag;
        dy /= mag;
    }
    outX = dx;
    outY = dy;
}

void Game::processInput() {
    resetEdgeInput();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { m_running = false; return; }

        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            bool down = (e.type == SDL_KEYDOWN);
            bool firstPress = down && !e.key.repeat;
            switch (e.key.keysym.sym) {
            case SDLK_UP:    case SDLK_w:
            case SDLK_KP_8:
                m_kbUp = down; if (firstPress) m_keyUpPressed = true; break;
            case SDLK_DOWN:  case SDLK_s:
            case SDLK_KP_2:
                m_kbDown = down; if (firstPress) m_keyDownPressed = true; break;
            case SDLK_LEFT:  case SDLK_a:
            case SDLK_KP_4:
                m_kbLeft = down; if (firstPress) m_keyLeftPressed = true; break;
            case SDLK_RIGHT: case SDLK_d:
            case SDLK_KP_6:
                m_kbRight = down; if (firstPress) m_keyRightPressed = true; break;
            case SDLK_RETURN: case SDLK_KP_ENTER:
            case SDLK_z: case SDLK_SPACE: case SDLK_j:
                m_kbConfirm = down; if (firstPress) m_keyConfirmPressed = true; break;
            case SDLK_x: case SDLK_LSHIFT: case SDLK_RSHIFT: case SDLK_k:
                m_kbBomb = down; if (firstPress) m_keyBombPressed = true; break;
            case SDLK_ESCAPE: case SDLK_p:
                m_kbPause = down; if (firstPress) m_keyPausePressed = true; break;
            case SDLK_BACKSPACE: case SDLK_TAB:
                m_kbBack = down; if (firstPress) m_keyBackPressed = true; break;
            default: break;
            }
        }

        // Apply axis events immediately (don't wait for end-of-frame poll)
        if (e.type == SDL_CONTROLLERAXISMOTION && m_controller) {
            float v = axisNorm(e.caxis.value);
            if (std::fabs(v) < AXIS_DEADZONE) v = 0.0f;
            else if (std::fabs(v) > 0.7f) v = (v > 0.0f) ? 1.0f : -1.0f;
            if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) m_axisX = v;
            if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) m_axisY = v;
        }
        if (e.type == SDL_JOYAXISMOTION && m_joystick && e.jaxis.which == m_joystickId) {
            float v = axisNorm(e.jaxis.value);
            if (std::fabs(v) < AXIS_DEADZONE) v = 0.0f;
            else if (std::fabs(v) > 0.7f) v = (v > 0.0f) ? 1.0f : -1.0f;
            if (e.jaxis.axis == 0) m_axisX = v;
            if (e.jaxis.axis == 1) m_axisY = v;
        }

        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            fprintf(stderr, "Controller added (index %d)\n", e.cdevice.which);
            if (!m_controller && !m_joystick) openPreferredController();
            else if (!m_controller && isPreferredController(e.cdevice.which)) openPreferredController();
        }
        if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (e.cdevice.which == m_joystickId) {
                fprintf(stderr, "Controller removed\n");
                closeController();
                openPreferredController();
            }
        }
        if (e.type == SDL_JOYDEVICEADDED) {
            if (!m_controller && !m_joystick) openPreferredController();
        }
        if (e.type == SDL_JOYDEVICEREMOVED) {
            if (e.jdevice.which == m_joystickId) {
                fprintf(stderr, "Joystick removed\n");
                closeController();
                openPreferredController();
            }
        }
    }

    pollPadState();
}

// ---------------------------------------------------------------------------
// Update dispatch
// ---------------------------------------------------------------------------

void Game::update(float dt) {
    m_bgScrollY1 += BG_SCROLL_SPEED * dt;
    m_bgScrollY2 += BG_SCROLL_SPEED * dt;
    if (m_bgScrollY1 >= m_screenH) m_bgScrollY1 = m_bgScrollY2 - m_screenH;
    if (m_bgScrollY2 >= m_screenH) m_bgScrollY2 = m_bgScrollY1 - m_screenH;

    switch (m_state) {
    case GameState::MainMenu:   updateMenu(dt); break;
    case GameState::Playing:    updatePlaying(dt); break;
    case GameState::Paused:     updatePaused(dt); break;
    case GameState::GameOver:   updateGameOver(dt); break;
    case GameState::Settings:   updateSettings(dt); break;
    case GameState::About:
        if (m_keyBackPressed || m_keyConfirmPressed || m_keyPausePressed) {
            m_state = GameState::MainMenu;
            m_menuSelection = 0;
            m_res.playSound("button");
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Menu logic
// ---------------------------------------------------------------------------

void Game::updateMenu(float /*dt*/) {
    m_menuItemCount = 3;
    if (m_keyUpPressed)   { m_menuSelection = (m_menuSelection - 1 + m_menuItemCount) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyDownPressed) { m_menuSelection = (m_menuSelection + 1) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyConfirmPressed) {
        m_res.playSound("button");
        switch (m_menuSelection) {
        case 0: startGame(); break;
        case 1: m_state = GameState::Settings; m_menuSelection = 0; break;
        case 2: m_running = false; break;
        }
    }
}

void Game::updatePaused(float /*dt*/) {
    m_menuItemCount = 3;
    if (m_keyUpPressed)   { m_menuSelection = (m_menuSelection - 1 + m_menuItemCount) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyDownPressed) { m_menuSelection = (m_menuSelection + 1) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyPausePressed) { resumeGame(); return; }
    if (m_keyConfirmPressed) {
        m_res.playSound("button");
        switch (m_menuSelection) {
        case 0: resumeGame(); break;
        case 1: endGame(); startGame(); break;
        case 2: endGame(); m_state = GameState::MainMenu; m_menuSelection = 0; break;
        }
    }
}

void Game::updateGameOver(float /*dt*/) {
    m_menuItemCount = 2;
    if (m_keyUpPressed)   { m_menuSelection = (m_menuSelection - 1 + m_menuItemCount) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyDownPressed) { m_menuSelection = (m_menuSelection + 1) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyConfirmPressed) {
        m_res.playSound("button");
        switch (m_menuSelection) {
        case 0: endGame(); startGame(); break;
        case 1: endGame(); m_state = GameState::MainMenu; m_menuSelection = 0; break;
        }
    }
}

void Game::updateSettings(float /*dt*/) {
    m_menuItemCount = 3;
    if (m_keyUpPressed)   { m_menuSelection = (m_menuSelection - 1 + m_menuItemCount) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyDownPressed) { m_menuSelection = (m_menuSelection + 1) % m_menuItemCount; m_res.playSound("button"); }
    if (m_keyBackPressed || m_keyPausePressed) {
        m_state = GameState::MainMenu; m_menuSelection = 0;
        m_res.playSound("button");
        saveSettings();
        return;
    }
    if (m_keyConfirmPressed) {
        m_res.playSound("button");
        switch (m_menuSelection) {
        case 0:
            m_soundOn = !m_soundOn;
            m_res.setSoundEnabled(m_soundOn);
            break;
        case 1:
            m_musicOn = !m_musicOn;
            m_res.setMusicEnabled(m_musicOn);
            break;
        case 2:
            m_state = GameState::MainMenu; m_menuSelection = 0;
            saveSettings();
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Gameplay update
// ---------------------------------------------------------------------------

void Game::updatePlaying(float dt) {
    if (m_keyPausePressed) { pauseGame(); return; }

    // Player movement (analog axes + keyboard, low-latency)
    if (m_player.alive && !m_player.exploding) {
        float dx = 0, dy = 0;
        movementVector(dx, dy);
        m_player.x += dx * m_player.speed * dt;
        m_player.y += dy * m_player.speed * dt;

        if (m_keyBombPressed) useBomb();
    }

    m_player.update(dt, m_screenW, m_screenH);

    if (!m_player.alive) {
        m_state = GameState::GameOver;
        m_menuSelection = 0;
        m_res.stopSound("game_music");
        return;
    }

    // Bullet spawning
    if (!m_player.exploding) {
        m_bulletTimer += dt;
        if (m_bulletTimer >= BULLET_INTERVAL) {
            m_bulletTimer -= BULLET_INTERVAL;
            spawnBullet();
        }
    }

    // Double bullet countdown
    if (m_doubleBulletTimeLeft > 0) {
        m_doubleBulletTimeLeft -= dt;
        if (m_doubleBulletTimeLeft <= 0) {
            m_bulletType = 1;
            m_doubleBulletTimeLeft = -1;
        }
    }

    // Enemy-free zone countdown (after bomb)
    if (m_enemyFreeTimeLeft > 0) {
        m_enemyFreeTimeLeft -= dt;
        if (m_enemyFreeTimeLeft <= 0) {
            m_enemyFreeTimeLeft = -1;
        }
    }

    // Enemy spawning
    if (m_enemyFreeTimeLeft <= 0) {
        int timerCount = m_grade2Timers ? 9 : 6;
        int idx = 0;
        for (int type = 1; type <= 3; type++) {
            int variants = m_grade2Timers ? 3 : 2;
            for (int v = 0; v < variants; v++) {
                if (idx >= timerCount) break;
                m_enemyTimers[idx].elapsed += dt;
                if (m_enemyTimers[idx].elapsed >= m_enemyTimers[idx].interval) {
                    m_enemyTimers[idx].elapsed -= m_enemyTimers[idx].interval;
                    spawnEnemy(type, v);
                }
                idx++;
            }
        }
    }

    // Prop spawning
    if (m_propsActive) {
        m_propTimer += dt;
        if (m_propTimer >= PROP_INTERVAL) {
            m_propTimer -= PROP_INTERVAL;
            spawnProp();
        }
    }

    // Update entities
    for (auto& b : m_bullets) b.update(dt);
    for (auto& e : m_enemies) e.update(dt, m_screenH);
    for (auto& p : m_props)   p.update(dt, m_screenH);

    if (!m_player.exploding) checkCollisions();
    removeDeadEntities();
}

// ---------------------------------------------------------------------------
// Game lifecycle
// ---------------------------------------------------------------------------

void Game::startGame() {
    m_state = GameState::Playing;
    m_score = 0;
    m_bombs = 0;
    m_grade = 1;
    m_bulletType = 1;
    m_bulletTimer = 0;
    m_doubleBulletTimeLeft = -1;
    m_enemyFreeTimeLeft = -1;
    m_propsActive = false;
    m_propTimer = 0;
    m_grade2Timers = false;

    m_bullets.clear();
    m_enemies.clear();
    m_props.clear();

    initPlayerSize();
    m_player.alive = true;
    m_player.exploding = false;
    m_player.visible = true;
    m_player.animFrame = 0;
    m_player.animTimer = 0;
    m_player.explodeFrame = 0;
    m_player.explodeTimer = 0;
    m_player.x = (m_screenW - m_player.w) / 2.0f;
    m_player.y = m_screenH - m_player.h - 20.0f;
    m_player.speed = m_screenW * 1.05f;  // snappier response on D-pad

    upgradeGrade();
    m_res.playSound("game_music", -1);
}

void Game::pauseGame() {
    m_state = GameState::Paused;
    m_menuSelection = 0;
    m_res.stopSound("game_music");
    m_res.playSound("button");
}

void Game::resumeGame() {
    m_state = GameState::Playing;
    m_res.playSound("game_music", -1);
}

void Game::endGame() {
    m_res.stopSound("game_music");
    m_bullets.clear();
    m_enemies.clear();
    m_props.clear();
}

// ---------------------------------------------------------------------------
// Difficulty
// ---------------------------------------------------------------------------

void Game::upgradeGrade() {
    setSpawnIntervals();
    for (auto& t : m_enemyTimers) t.elapsed = 0;
}

void Game::setSpawnIntervals() {
    // Intervals in seconds: [type1_v0, type1_v1, type1_v2, type2_v0, type2_v1, type2_v2, type3_v0, type3_v1, type3_v2]
    float intervals[5][9] = {
        { 1.0f, 2.0f, 1.8f,   5.0f, 15.0f, 18.0f,   24.0f, 35.0f, 47.0f },  // grade 1
        { 0.9f, 1.5f, 1.8f,   4.0f,  6.0f, 14.0f,   20.0f, 36.0f, 46.0f },  // grade 2
        { 0.7f, 1.0f, 1.1f,   2.0f,  2.6f,  7.0f,   10.0f, 23.0f, 35.0f },  // grade 3
        { 0.8f, 0.8f, 1.0f,   1.5f,  1.6f,  2.0f,    5.0f, 12.0f, 16.0f },  // grade 4
        { 0.5f, 0.6f, 0.8f,   1.0f,  1.2f,  1.5f,    4.0f,  8.0f, 12.0f },  // grade 5
    };
    int g = std::clamp(m_grade, 1, 5) - 1;
    int idx = 0;
    for (int type = 1; type <= 3; type++) {
        int variants = m_grade2Timers ? 3 : 2;
        for (int v = 0; v < variants; v++) {
            int col = (type - 1) * 3 + v;
            m_enemyTimers[idx].interval = intervals[g][col];
            m_enemyTimers[idx].elapsed = 0;
            idx++;
        }
    }
    for (; idx < 9; idx++) m_enemyTimers[idx] = {99999.0f, 0};
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void Game::initPlayerSize() {
    int w, h;
    m_res.texSizeScaled("F1_01", m_scale, &w, &h);
    m_player.w = w;
    m_player.h = h;
}

float Game::enemyBaseSpeed(int type, int variant) const {
    // Speed = screen height / original duration, scaled to current screen
    static const float durations[3][3] = {
        { 3.1f, 2.1f, 1.3f },   // type 1
        { 3.8f, 2.5f, 1.5f },   // type 2
        { 7.0f, 4.9f, 3.8f },   // type 3
    };
    int t = std::clamp(type, 1, 3) - 1;
    int v = std::clamp(variant, 0, 2);
    return static_cast<float>(m_screenH) / durations[t][v];
}

void Game::spawnEnemy(int type, int speedVariant) {
    Enemy e;
    e.type = type;
    e.speed = enemyBaseSpeed(type, speedVariant);

    std::string texName = texNameForEnemy(type, 0);
    int tw, th;
    m_res.texSizeScaled(texName, m_scale, &tw, &th);
    e.w = tw;
    e.h = th;

    e.y = static_cast<float>(-e.h);
    e.x = static_cast<float>(rand() % std::max(1, m_screenW - e.w));

    switch (type) {
    case 1: e.hp = 1;  e.score = 1000;  break;
    case 2: e.hp = 5;  e.score = 6000;  break;
    case 3: e.hp = 15; e.score = 30000; break;
    }
    e.alive = true;
    e.exploding = false;
    e.explodeFrame = 0;
    e.explodeTimer = 0;
    e.hitFlash = false;
    e.hitFlashTimer = 0;
    e.flashTimer = 0;
    e.flashState = false;

    m_enemies.push_back(e);

    if (type == 3) m_res.playSound("flying");
}

void Game::spawnBullet() {
    Bullet b;
    b.type = m_bulletType;
    std::string texName = (b.type == 1) ? "Bullets_01" : "Bullets_02";
    int tw, th;
    m_res.texSizeScaled(texName, m_scale, &tw, &th);
    b.w = tw;
    b.h = th;
    b.x = m_player.x + (m_player.w - b.w) / 2.0f;
    b.y = m_player.y - b.h;
    b.speed = m_screenH / 0.45f;
    b.alive = true;

    if (b.y > 0) {
        m_bullets.push_back(b);
        m_res.playSound("bullet");
    }
}

void Game::spawnProp() {
    Prop p;
    p.type = rand() % 2;
    std::string texName = (p.type == 0) ? "Bullets_Dual" : "Bullets_Bomb";
    int tw, th;
    m_res.texSizeScaled(texName, m_scale, &tw, &th);
    p.w = tw;
    p.h = th;
    p.y = static_cast<float>(-p.h);
    p.x = static_cast<float>(rand() % std::max(1, m_screenW - p.w));
    p.speed = m_screenH / 5.0f;
    p.alive = true;

    m_props.push_back(p);
    m_res.playSound("out_porp");
}

void Game::useBomb() {
    if (m_bombs <= 0) return;
    m_bombs--;
    m_res.playSound("use_bomb");

    for (auto& e : m_enemies) {
        if (!e.exploding && e.alive) {
            e.exploding = true;
            e.explodeFrame = 0;
            e.explodeTimer = 0;
            m_score += e.score;
        }
    }

    m_enemyFreeTimeLeft = BOMB_FREE_DURATION;
}

// ---------------------------------------------------------------------------
// Collisions
// ---------------------------------------------------------------------------

bool Game::frectOverlap(const SDL_FRect& a, const SDL_FRect& b) const {
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

void Game::checkCollisions() {
    SDL_FRect playerHB = m_player.hitbox();

    // Bullets vs enemies
    for (auto& b : m_bullets) {
        if (!b.alive) continue;
        SDL_FRect bHB = b.hitbox();
        for (auto& e : m_enemies) {
            if (!e.alive || e.exploding) continue;
            if (frectOverlap(bHB, e.hitbox())) {
                b.alive = false;
                e.hp -= b.type;
                if (e.hp <= 0) {
                    e.exploding = true;
                    e.explodeFrame = 0;
                    e.explodeTimer = 0;
                    m_score += e.score;
                    const char* snd = (e.type == 1) ? "enemy1_down" : (e.type == 2) ? "enemy2_down" : "enemy3_down";
                    m_res.playSound(snd);
                } else {
                    e.hitFlash = true;
                    e.hitFlashTimer = 0;
                }

                // Difficulty upgrade check
                if (m_score > 60000 && !m_propsActive) {
                    m_propsActive = true;
                    m_propTimer = 0;
                }
                if (m_score > 400000 && m_grade == 1)  { m_grade = 2; m_grade2Timers = true; upgradeGrade(); }
                if (m_score > 500000 && m_grade == 2)  { m_grade = 3; upgradeGrade(); }
                if (m_score > 700000 && m_grade == 3)  { m_grade = 4; upgradeGrade(); }
                if (m_score > 1000000 && m_grade == 4) { m_grade = 5; upgradeGrade(); }

                break;
            }
        }
    }

    // Player vs props
    for (auto& p : m_props) {
        if (!p.alive) continue;
        if (frectOverlap(playerHB, p.hitbox())) {
            p.alive = false;
            if (p.type == 0) {
                m_bulletType = 2;
                m_doubleBulletTimeLeft = DOUBLE_BULLET_DURATION;
                m_res.playSound("double_laser");
            } else {
                if (m_bombs < MAX_BOMBS) m_bombs++;
                m_res.playSound("bomb");
            }
        }
    }

    // Player vs enemies
    for (auto& e : m_enemies) {
        if (!e.alive || e.exploding) continue;
        if (frectOverlap(playerHB, e.hitbox())) {
            m_player.exploding = true;
            m_player.explodeFrame = 0;
            m_player.explodeTimer = 0;
            m_res.playSound("game_over");
            m_res.stopSound("game_music");
            break;
        }
    }
}

void Game::removeDeadEntities() {
    auto removeIf = [](auto& vec) {
        vec.erase(std::remove_if(vec.begin(), vec.end(), [](const auto& e) { return !e.alive; }), vec.end());
    };
    removeIf(m_bullets);
    removeIf(m_enemies);
    removeIf(m_props);
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void Game::render() {
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    renderBackground();

    switch (m_state) {
    case GameState::MainMenu:   renderMainMenu(); break;
    case GameState::Playing:    renderEntities(); renderHUD(); break;
    case GameState::Paused:     renderEntities(); renderHUD(); renderPauseMenu(); break;
    case GameState::GameOver:   renderEntities(); renderHUD(); renderGameOverScreen(); break;
    case GameState::Settings:   renderSettingsMenu(); break;
    case GameState::About:      renderAboutScreen(); break;
    }

    SDL_RenderPresent(m_renderer);
}

void Game::renderBackground() {
    SDL_Texture* bg = m_res.tex("Blackground_MeeGo");
    if (!bg) return;

    SDL_Rect dst1 = { 0, static_cast<int>(m_bgScrollY1), m_screenW, m_screenH };
    SDL_Rect dst2 = { 0, static_cast<int>(m_bgScrollY2), m_screenW, m_screenH };
    SDL_RenderCopy(m_renderer, bg, nullptr, &dst1);
    SDL_RenderCopy(m_renderer, bg, nullptr, &dst2);
}

void Game::renderEntities() {
    // Props
    for (auto& p : m_props) {
        if (!p.alive) continue;
        std::string texName = (p.type == 0) ? "Bullets_Dual" : "Bullets_Bomb";
        SDL_Texture* t = m_res.tex(texName);
        if (t) {
            SDL_Rect dst = p.rect();
            SDL_RenderCopy(m_renderer, t, nullptr, &dst);
        }
    }

    // Enemies
    for (auto& e : m_enemies) {
        if (!e.alive) continue;
        std::string texName;
        if (e.exploding) {
            int frame;
            if (e.type == 1) frame = e.explodeFrame + 1;
            else if (e.type == 2) frame = e.explodeFrame + 2;
            else frame = e.explodeFrame + 3;
            texName = texNameForEnemy(e.type, frame);
        } else if (e.hitFlash) {
            texName = texNameForEnemy(e.type, (e.type == 3) ? 2 : 1);
        } else if (e.type == 3 && e.flashState) {
            texName = texNameForEnemy(3, 1);
        } else {
            texName = texNameForEnemy(e.type, 0);
        }
        SDL_Texture* t = m_res.tex(texName);
        if (t) {
            SDL_Rect dst = e.rect();
            SDL_RenderCopy(m_renderer, t, nullptr, &dst);
        }
    }

    // Bullets
    for (auto& b : m_bullets) {
        if (!b.alive) continue;
        std::string texName = (b.type == 1) ? "Bullets_01" : "Bullets_02";
        SDL_Texture* t = m_res.tex(texName);
        if (t) {
            SDL_Rect dst = b.rect();
            SDL_RenderCopy(m_renderer, t, nullptr, &dst);
        }
    }

    // Player
    if (m_player.visible) {
        std::string texName;
        if (m_player.exploding) {
            texName = texNameForPlayer(m_player.explodeFrame + 2);
        } else {
            texName = texNameForPlayer(m_player.animFrame);
        }
        SDL_Texture* t = m_res.tex(texName);
        if (t) {
            SDL_Rect dst = m_player.rect();
            SDL_RenderCopy(m_renderer, t, nullptr, &dst);
        }
    }
}

void Game::renderHUD() {
    int margin = static_cast<int>(18 * m_scale / 1.5f);
    int fontSize = static_cast<int>(30 * m_scale / 1.5f);

    // Pause icon (top-left) + score to its right — matches original layout / reference
    const char* pauseTex = (m_state == GameState::Paused) ? "Pause_02" : "Pause_01";
    int pw = 0, ph = 0;
    m_res.texSizeScaled(pauseTex, m_scale * 0.85f, &pw, &ph);
    if (pw > 0) {
        SDL_Rect pauseDst = { margin, margin, pw, ph };
        SDL_Texture* pt = m_res.tex(pauseTex);
        if (pt) SDL_RenderCopy(m_renderer, pt, nullptr, &pauseDst);

        char scoreBuf[32];
        snprintf(scoreBuf, sizeof(scoreBuf), "%d", m_score);
        int tw = 0, th = 0;
        SDL_Texture* scoreTex = m_res.renderText(scoreBuf, fontSize, UI_MATTE, &tw, &th);
        if (scoreTex) {
            SDL_Rect scoreDst = { margin + pw + static_cast<int>(8 * m_scale / 1.5f),
                                  margin + (ph - th) / 2, tw, th };
            SDL_RenderCopy(m_renderer, scoreTex, nullptr, &scoreDst);
            SDL_DestroyTexture(scoreTex);
        }
    }

    // Bomb icon + count (bottom-left), always visible while playing
    int bw = 0, bh = 0;
    m_res.texSizeScaled("Bomb", m_scale * 0.55f, &bw, &bh);
    if (bw > 0) {
        int bx = margin;
        int by = m_screenH - bh - margin;
        SDL_Texture* bombTex = m_res.tex("Bomb");
        if (bombTex) {
            SDL_Rect dst = { bx, by, bw, bh };
            if (m_bombs <= 0) SDL_SetTextureAlphaMod(bombTex, 90);
            else SDL_SetTextureAlphaMod(bombTex, 255);
            SDL_RenderCopy(m_renderer, bombTex, nullptr, &dst);
            SDL_SetTextureAlphaMod(bombTex, 255);
        }

        char bombBuf[8];
        snprintf(bombBuf, sizeof(bombBuf), "x%d", m_bombs);
        int tw = 0, th = 0;
        SDL_Texture* countTex = m_res.renderText(bombBuf, fontSize, UI_MATTE, &tw, &th);
        if (countTex) {
            SDL_Rect textDst = { bx + bw + static_cast<int>(6 * m_scale / 1.5f),
                                 by + (bh - th) / 2, tw, th };
            SDL_RenderCopy(m_renderer, countTex, nullptr, &textDst);
            SDL_DestroyTexture(countTex);
        }
    }
}

// ---------------------------------------------------------------------------
// Menu rendering
// ---------------------------------------------------------------------------

void Game::drawTextCentered(const std::string& text, int fontSize, SDL_Color color, int y) {
    int tw, th;
    SDL_Texture* t = m_res.renderText(text, fontSize, color, &tw, &th);
    if (t) {
        SDL_Rect dst = { (m_screenW - tw) / 2, y, tw, th };
        SDL_RenderCopy(m_renderer, t, nullptr, &dst);
        SDL_DestroyTexture(t);
    }
}

void Game::drawTextureCentered(const std::string& name, int y, float texScale) {
    SDL_Texture* t = m_res.tex(name);
    if (!t) return;
    int tw, th;
    m_res.texSizeScaled(name, texScale, &tw, &th);
    SDL_Rect dst = { (m_screenW - tw) / 2, y, tw, th };
    SDL_RenderCopy(m_renderer, t, nullptr, &dst);
}

void Game::drawImageButton(const std::string& label, int y, bool selected, float btnScale) {
    const char* texName = selected ? "button_2_2" : "button_2_1";
    int bw = 0, bh = 0;
    m_res.texSizeScaled(texName, btnScale, &bw, &bh);
    if (bw <= 0) {
        // Fallback if textures missing
        drawTextCentered(label, static_cast<int>(28 * m_scale / 1.5f),
                         selected ? UI_MATTE_SELECTED : UI_MATTE, y);
        return;
    }

    SDL_Rect dst = { (m_screenW - bw) / 2, y, bw, bh };
    SDL_Texture* btn = m_res.tex(texName);
    if (btn) SDL_RenderCopy(m_renderer, btn, nullptr, &dst);

    int fontSize = static_cast<int>(28 * m_scale / 1.5f);
    int tw = 0, th = 0;
    SDL_Color color = selected ? UI_MATTE_SELECTED : UI_MATTE;
    SDL_Texture* text = m_res.renderText(label, fontSize, color, &tw, &th);
    if (text) {
        SDL_Rect td = { dst.x + (bw - tw) / 2, dst.y + (bh - th) / 2, tw, th };
        SDL_RenderCopy(m_renderer, text, nullptr, &td);
        SDL_DestroyTexture(text);
    }
}

void Game::drawMenuItems(const std::vector<std::string>& items, int selectedIdx, int startY) {
    float btnScale = m_scale * 0.95f;
    int bw = 0, bh = 0;
    m_res.texSizeScaled("button_2_1", btnScale, &bw, &bh);
    int spacing = bh > 0 ? bh + static_cast<int>(18 * m_scale / 1.5f)
                         : static_cast<int>(60 * m_scale / 1.5f);

    for (int i = 0; i < static_cast<int>(items.size()); i++) {
        drawImageButton(items[i], startY + i * spacing, i == selectedIdx, btnScale);
    }
}

void Game::renderMainMenu() {
    drawTextureCentered("LOGO", m_screenH / 6, m_scale);

    std::vector<std::string> items = { "Start Game", "Settings", "Exit" };
    drawMenuItems(items, m_menuSelection, m_screenH * 52 / 100);
}

void Game::renderPauseMenu() {
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 140);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect overlay = { 0, 0, m_screenW, m_screenH };
    SDL_RenderFillRect(m_renderer, &overlay);

    int titleSize = static_cast<int>(40 * m_scale / 1.5f);
    drawTextCentered("PAUSED", titleSize, {210, 210, 210, 255}, m_screenH / 5);

    std::vector<std::string> items = { "Continue", "Restart", "Quit" };
    drawMenuItems(items, m_menuSelection, m_screenH * 38 / 100);
}

void Game::renderGameOverScreen() {
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 160);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect overlay = { 0, 0, m_screenW, m_screenH };
    SDL_RenderFillRect(m_renderer, &overlay);

    int titleSize = static_cast<int>(40 * m_scale / 1.5f);
    drawTextCentered("Game Over", titleSize, {210, 210, 210, 255}, m_screenH / 6);

    char scoreBuf[64];
    snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", m_score);
    int scoreSize = static_cast<int>(32 * m_scale / 1.5f);
    drawTextCentered(scoreBuf, scoreSize, {210, 210, 210, 255}, m_screenH / 6 + titleSize + 16);

    std::vector<std::string> items = { "Play Again", "Quit" };
    drawMenuItems(items, m_menuSelection, m_screenH * 48 / 100);
}

void Game::renderSettingsMenu() {
    int titleSize = static_cast<int>(40 * m_scale / 1.5f);
    drawTextCentered("Settings", titleSize, UI_MATTE, m_screenH / 5);

    std::string sndLabel = std::string("Sound: ") + (m_soundOn ? "ON" : "OFF");
    std::string musLabel = std::string("Music: ") + (m_musicOn ? "ON" : "OFF");
    std::vector<std::string> items = { sndLabel, musLabel, "Back" };
    drawMenuItems(items, m_menuSelection, m_screenH * 40 / 100);
}

void Game::renderAboutScreen() {
    int titleSize = static_cast<int>(36 * m_scale / 1.5f);
    int textSize = static_cast<int>(22 * m_scale / 1.5f);
    int y = m_screenH / 5;
    int lineH = static_cast<int>(35 * m_scale / 1.5f);

    drawTextCentered("AIRCRAFT WAR", titleSize, {255, 255, 255, 255}, y);
    y += titleSize + lineH;
    drawTextCentered("v2.2.0 - SDL2 Native Port", textSize, {200, 200, 200, 255}, y);
    y += lineH;
    drawTextCentered("D-Pad / Stick / Arrows: Move", textSize, {180, 180, 180, 255}, y);
    y += lineH;
    drawTextCentered("A / Z / Space: Confirm", textSize, {180, 180, 180, 255}, y);
    y += lineH;
    drawTextCentered("B / X / Shoulder: Bomb", textSize, {180, 180, 180, 255}, y);
    y += lineH;
    drawTextCentered("Start / P / Esc: Pause", textSize, {180, 180, 180, 255}, y);
    y += lineH * 2;
    drawTextCentered("Based on Aircraft-War by zccrs", textSize, {150, 150, 150, 255}, y);
    y += lineH;
    drawTextCentered("SDL2 port for Linux / RPi", textSize, {150, 150, 150, 255}, y);
    y += lineH * 2;
    drawTextCentered("Press any button to go back", textSize, {255, 220, 50, 255}, y);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string Game::texNameForEnemy(int type, int frame) const {
    char buf[16];
    const char* prefix = (type == 1) ? "F2" : (type == 2) ? "F3" : "F4";
    snprintf(buf, sizeof(buf), "%s_%02d", prefix, frame + 1);
    return buf;
}

std::string Game::texNameForPlayer(int frame) const {
    char buf[16];
    snprintf(buf, sizeof(buf), "F1_%02d", frame + 1);
    return buf;
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------

static std::string settingsPath() {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    std::string dir = std::string(home) + "/.config/aircraftwar";
    return dir + "/settings.cfg";
}

void Game::loadSettings() {
    std::ifstream f(settingsPath());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "sound") m_soundOn = (val == "1");
        if (key == "music") m_musicOn = (val == "1");
    }
}

void Game::saveSettings() {
    std::string path = settingsPath();
    std::string dir = path.substr(0, path.rfind('/'));

    std::string mkdirCmd = "mkdir -p " + dir;
    system(mkdirCmd.c_str());

    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "sound=" << (m_soundOn ? "1" : "0") << "\n";
    f << "music=" << (m_musicOn ? "1" : "0") << "\n";
}

#pragma once

#include <SDL.h>

struct Player {
    float x = 0, y = 0;
    int w = 0, h = 0;
    float speed = 360.0f;
    bool alive = true;
    bool exploding = false;
    int animFrame = 0;
    float animTimer = 0;
    float explodeTimer = 0;
    int explodeFrame = 0;
    bool visible = true;

    void update(float dt, int screenW, int screenH);
    SDL_Rect rect() const;
    SDL_FRect hitbox() const;
};

struct Bullet {
    float x = 0, y = 0;
    int w = 0, h = 0;
    float speed = 0;
    int type = 1;
    bool alive = true;

    void update(float dt);
    SDL_Rect rect() const;
    SDL_FRect hitbox() const;
};

struct Enemy {
    float x = 0, y = 0;
    int w = 0, h = 0;
    float speed = 0;        // pixels per second
    int type = 1;            // 1, 2, 3
    int hp = 1;
    int score = 1000;
    bool alive = true;
    bool exploding = false;
    int explodeFrame = 0;
    float explodeTimer = 0;
    bool hitFlash = false;
    float hitFlashTimer = 0;
    float flashTimer = 0;    // type-3 head flash
    bool flashState = false;

    void update(float dt, int screenH);
    SDL_Rect rect() const;
    SDL_FRect hitbox() const;
};

struct Prop {
    float x = 0, y = 0;
    int w = 0, h = 0;
    float speed = 0;
    int type = 0;  // 0=double_bullet, 1=bomb
    bool alive = true;

    void update(float dt, int screenH);
    SDL_Rect rect() const;
    SDL_FRect hitbox() const;
};

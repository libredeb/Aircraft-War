#include "entity.h"
#include <algorithm>

// --- Player ---

void Player::update(float dt, int screenW, int screenH) {
    if (exploding) {
        explodeTimer += dt;
        if (explodeTimer >= 0.08f) {
            explodeTimer -= 0.08f;
            explodeFrame++;
            if (explodeFrame <= 2) {
                // frames 0-2: explosion sprites
            } else if (explodeFrame <= 8) {
                visible = (explodeFrame % 2 == 0);
            } else {
                visible = false;
                alive = false;
            }
        }
        return;
    }
    animTimer += dt;
    if (animTimer >= 0.1f) {
        animTimer -= 0.1f;
        animFrame = 1 - animFrame;
    }
    x = std::clamp(x, 0.0f, static_cast<float>(screenW - w));
    y = std::clamp(y, 0.0f, static_cast<float>(screenH - h));
}

SDL_Rect Player::rect() const {
    return { static_cast<int>(x), static_cast<int>(y), w, h };
}

SDL_FRect Player::hitbox() const {
    return { x + w * 0.1f, y + h * 0.2f, w * 0.8f, h * 0.8f };
}

// --- Bullet ---

void Bullet::update(float dt) {
    y -= speed * dt;
    if (y + h < 0) alive = false;
}

SDL_Rect Bullet::rect() const {
    return { static_cast<int>(x), static_cast<int>(y), w, h };
}

SDL_FRect Bullet::hitbox() const {
    if (type == 1) {
        float margin = w * 0.25f;
        return { x + margin, y, w - margin * 2, static_cast<float>(h) };
    }
    return { x, y, static_cast<float>(w), static_cast<float>(h) };
}

// --- Enemy ---

void Enemy::update(float dt, int screenH) {
    if (exploding) {
        explodeTimer += dt;
        if (explodeTimer >= 0.08f) {
            explodeTimer -= 0.08f;
            explodeFrame++;
            int maxFrames = (type == 1) ? 4 : (type == 2) ? 4 : 6;
            if (explodeFrame >= maxFrames) alive = false;
        }
        return;
    }

    y += speed * dt;
    if (y > screenH) alive = false;

    if (hitFlash) {
        hitFlashTimer += dt;
        if (hitFlashTimer >= 0.05f) hitFlash = false;
    }

    if (type == 3 && !hitFlash) {
        flashTimer += dt;
        if (flashTimer >= 0.05f) {
            flashTimer -= 0.05f;
            flashState = !flashState;
        }
    }
}

SDL_Rect Enemy::rect() const {
    return { static_cast<int>(x), static_cast<int>(y), w, h };
}

SDL_FRect Enemy::hitbox() const {
    float fw = static_cast<float>(w), fh = static_cast<float>(h);
    switch (type) {
    case 1: return { x + fw / 4, y + fh / 3, fw / 2, fh * 5 / 12 };
    case 2: return { x + fw / 6, y + fh / 8, fw * 2 / 3, fh * 3 / 4 };
    case 3: return { x + fw * 7 / 30, y + fh / 4, fw * 16 / 30, fh / 2 };
    default: return { x, y, fw, fh };
    }
}

// --- Prop ---

void Prop::update(float dt, int screenH) {
    y += speed * dt;
    if (y > screenH) alive = false;
}

SDL_Rect Prop::rect() const {
    return { static_cast<int>(x), static_cast<int>(y), w, h };
}

SDL_FRect Prop::hitbox() const {
    return { x, y, static_cast<float>(w), static_cast<float>(h) };
}

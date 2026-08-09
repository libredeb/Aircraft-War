#include "resources.h"
#include <cstdio>

bool Resources::init(SDL_Renderer* renderer, const std::string& dataPath) {
    m_renderer = renderer;
    m_dataPath = dataPath;

    const char* images[] = {
        "F1_01", "F1_02", "F1_03", "F1_04", "F1_05",
        "F2_01", "F2_02", "F2_03", "F2_04", "F2_05",
        "F3_01", "F3_02", "F3_03", "F3_04", "F3_05", "F3_06",
        "F4_01", "F4_02", "F4_03", "F4_04", "F4_05", "F4_06",
        "F4_07", "F4_08", "F4_09",
        "Bullets_01", "Bullets_02", "Bullets_Bomb", "Bullets_Dual",
        "Blackground_MeeGo", "Blackground_Symbian",
        "LOGO", "Bomb",
        "Pause_01", "Pause_02",
        "button_1", "button_2_1", "button_2_2", "button_3_1", "button_3_2",
        "loading_01", "loading_02", "loading_03",
        "Button_rank", "Button_setting", "Button_info", "Button_back", "Button_back_2",
        "true", "false", "Rank", "Setting",
        "Rank_01", "Setting_01", "Setting_02", "About_01", "About_02",
    };
    for (auto& name : images) {
        if (!loadTexture(name, m_dataPath + "/Image/" + name + ".png"))
            fprintf(stderr, "Warning: failed to load texture %s from %s/Image/\n",
                    name, m_dataPath.c_str());
    }

    // Critical UI assets — loud failure if missing
    const char* uiRequired[] = { "button_2_1", "button_2_2", "Pause_01", "Bomb", "LOGO" };
    for (auto& name : uiRequired) {
        if (!tex(name))
            fprintf(stderr, "ERROR: UI texture '%s' missing — check -d / data path\n", name);
    }

    TTF_Font* testFont = font(24);
    if (!testFont)
        fprintf(stderr, "ERROR: font fzmw.ttf missing under %s/\n", m_dataPath.c_str());
    else
        fprintf(stderr, "UI assets OK (buttons/pause/bomb/font)\n");

    const char* sounds[] = {
        "achievement", "bomb", "bullet", "button",
        "double_laser", "enemy1_down", "enemy2_down", "enemy3_down",
        "flying", "game_music", "game_over", "out_porp", "use_bomb",
    };
    for (auto& name : sounds) {
        if (!loadSound(name, m_dataPath + "/sound/" + name + ".wav"))
            fprintf(stderr, "Warning: failed to load sound %s\n", name);
    }

    return true;
}

void Resources::shutdown() {
    for (auto& [k, t] : m_textures) SDL_DestroyTexture(t);
    m_textures.clear();
    for (auto& [k, c] : m_sounds) Mix_FreeChunk(c);
    m_sounds.clear();
    for (auto& [k, f] : m_fonts) TTF_CloseFont(f);
    m_fonts.clear();
}

bool Resources::loadTexture(const std::string& name, const std::string& path) {
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) return false;
    SDL_Texture* t = SDL_CreateTextureFromSurface(m_renderer, surf);
    SDL_FreeSurface(surf);
    if (!t) return false;
    m_textures[name] = t;
    return true;
}

bool Resources::loadSound(const std::string& name, const std::string& path) {
    Mix_Chunk* c = Mix_LoadWAV(path.c_str());
    if (!c) return false;
    m_sounds[name] = c;
    return true;
}

SDL_Texture* Resources::tex(const std::string& name) const {
    auto it = m_textures.find(name);
    return it != m_textures.end() ? it->second : nullptr;
}

void Resources::texSize(const std::string& name, int* w, int* h) const {
    auto t = tex(name);
    if (t) SDL_QueryTexture(t, nullptr, nullptr, w, h);
    else { *w = 0; *h = 0; }
}

void Resources::texSizeScaled(const std::string& name, float scale, int* w, int* h) const {
    texSize(name, w, h);
    *w = static_cast<int>(*w * scale);
    *h = static_cast<int>(*h * scale);
}

void Resources::playSound(const std::string& name, int loops) {
    if (name == "game_music") {
        if (!m_musicEnabled) return;
    } else {
        if (!m_soundEnabled) return;
    }
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return;
    int ch = Mix_PlayChannel(-1, it->second, loops);
    m_soundChannels[name] = ch;
}

void Resources::stopSound(const std::string& name) {
    auto it = m_soundChannels.find(name);
    if (it != m_soundChannels.end() && it->second >= 0) {
        Mix_HaltChannel(it->second);
        it->second = -1;
    }
}

TTF_Font* Resources::font(int size) {
    auto it = m_fonts.find(size);
    if (it != m_fonts.end()) return it->second;
    std::string path = m_dataPath + "/fzmw.ttf";
    TTF_Font* f = TTF_OpenFont(path.c_str(), size);
    if (!f) {
        fprintf(stderr, "Failed to load font at size %d: %s\n", size, TTF_GetError());
        return nullptr;
    }
    m_fonts[size] = f;
    return f;
}

SDL_Texture* Resources::renderText(const std::string& text, int size, SDL_Color color, int* w, int* h,
                                   bool bold) {
    TTF_Font* f = font(size);
    if (!f) { *w = *h = 0; return nullptr; }
    int prevStyle = TTF_GetFontStyle(f);
    TTF_SetFontStyle(f, bold ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, text.c_str(), color);
    TTF_SetFontStyle(f, prevStyle);
    if (!surf) { *w = *h = 0; return nullptr; }
    SDL_Texture* t = SDL_CreateTextureFromSurface(m_renderer, surf);
    *w = surf->w;
    *h = surf->h;
    SDL_FreeSurface(surf);
    return t;
}

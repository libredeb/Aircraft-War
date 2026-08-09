#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <string>
#include <unordered_map>

class Resources {
public:
    bool init(SDL_Renderer* renderer, const std::string& dataPath);
    void shutdown();

    SDL_Texture* tex(const std::string& name) const;
    void texSize(const std::string& name, int* w, int* h) const;
    void texSizeScaled(const std::string& name, float scale, int* w, int* h) const;

    void playSound(const std::string& name, int loops = 0);
    void stopSound(const std::string& name);
    void setSoundEnabled(bool enabled) { m_soundEnabled = enabled; }
    void setMusicEnabled(bool enabled) { m_musicEnabled = enabled; }
    bool soundEnabled() const { return m_soundEnabled; }
    bool musicEnabled() const { return m_musicEnabled; }

    TTF_Font* font(int size);
    SDL_Texture* renderText(const std::string& text, int size, SDL_Color color, int* w, int* h,
                            bool bold = false);

private:
    SDL_Renderer* m_renderer = nullptr;
    std::string m_dataPath;
    std::unordered_map<std::string, SDL_Texture*> m_textures;
    std::unordered_map<std::string, Mix_Chunk*> m_sounds;
    std::unordered_map<std::string, int> m_soundChannels;
    std::unordered_map<int, TTF_Font*> m_fonts;
    bool m_soundEnabled = true;
    bool m_musicEnabled = true;

    bool loadTexture(const std::string& name, const std::string& path);
    bool loadSound(const std::string& name, const std::string& path);
};

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <vector>

#include "GameInfo.h"

class Launcher
{
public:
    Launcher(SDL_Window* window, SDL_Renderer* renderer);

    void run();

private:
    void handleEvents();
    void update();
    void render();

    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;

    std::vector<GameInfo> games;

    int selectedGame;
    bool running;
};

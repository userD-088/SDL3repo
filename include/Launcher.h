#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <vector>

#include "GameInfo.h"
#include "Utils.h"

class Launcher
{
public:
    Launcher(SDL_Window* window, SDL_Renderer* renderer);
    ~Launcher();

    void run();

private:
    struct Star {
        float x;
        float y;
        float speed;
    };

    void handleEvents();
    void update();
    void render();

    void startGame(int index);

    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;

    std::vector<GameInfo> games;
    std::vector<Star> stars;

    Timer timer;
    float dt = 0.0f;
    float time = 0.0f;

    int selectedGame;
    bool running;
};

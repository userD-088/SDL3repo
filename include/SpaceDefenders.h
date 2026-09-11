#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>

#include "Utils.h"

class SpaceDefenders 
{
public:
    SpaceDefenders(SDL_Window* window, SDL_Renderer* renderer);
    ~SpaceDefenders();

    void run();

private:
    struct Point { float x; float y; };

    struct Planet {
        Point position;
        float radius;
    };

    struct Player {
        Point figure[3];

        Point position;
        Point velocity;
        Point accelaration;

        // More
    };

    void handleEvents();
    void update();
    void render();

    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;

    void renderText(
        const char* text,
        float x,
        float y,
        SDL_Color color
    );

    // <-- Game Objects

    bool running = true;
};

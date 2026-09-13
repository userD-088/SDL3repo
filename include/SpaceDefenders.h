#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>

#include "Utils.h"
#include "const.h"

class SpaceDefenders 
{
public:
    SpaceDefenders(SDL_Window* window, SDL_Renderer* renderer);
    ~SpaceDefenders();

    void run();

private:
    struct Planet {
        SDL_FPoint position;
        float radius;
        float gravityPull;
    };

    struct Player {
        Polygon polygon;

        SDL_FPoint velocity;
        SDL_FPoint accelaration;

        // More
    };

    struct Asteroid {
        std::vector<SDL_FPoint> Body;

        SDL_FPoint position;
        SDL_FPoint velocity;
        SDL_FPoint acceleration;

        int hp;
        int damage;
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

    // Game Objects
    Planet earth = {{SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}, 55.0f, 0.1f};

    bool running = true;
};

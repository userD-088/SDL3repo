#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <cmath>

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
        float mass;
    };

    struct Player {
        Polygon polygon;

        float direction;
        float rotationSpeed;

        SDL_FPoint velocity;
        SDL_FPoint accelaration;

        float thrust = 100.0f;

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

    SDL_FPoint calcGravPullEarth(const SDL_FPoint& pos, const Planet& earth);

    Timer timer;
    float dt; 

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

    // Game Objects and Variables
    float gravConstant = 6.674E-11f;

    Planet earth = {{SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}, 55.0f, 5.9722E24f};

    Player player = {{{100.0f, 100.0f}, {{0, -16.0f},{0, 16.0f}, {36.0f, 0}}}, 0, SDL_PI_F, {0, 0}, {0, 0}, 100.0f};

    bool running = true;
};

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

class Pong
{
public:
    Pong(SDL_Window* window, SDL_Renderer* renderer);
    ~Pong();

    void run();

private:
    struct Ball {
        SDL_FRect rect;
        float vx;
        float vy;
        float speed;
    };

    struct Paddle {
        SDL_FRect rect;
        float speed;
        int score;
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

    Paddle player1;
    Paddle player2;
    Ball ball;

    float txtScore1Buffer;

    bool displayTxtControlls = true;

    bool running;
};

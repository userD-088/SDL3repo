#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

class SpaceDefenders 
{
public:
    SpaceDefenders(SDL_Window* window, SDL_Renderer* renderer);
    ~SpaceDefenders();

    void run();

private:

    // <-- Game Object Structs

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

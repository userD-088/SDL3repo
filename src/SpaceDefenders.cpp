#include "SpaceDefenders.h"
#include "const.h"

#include <array>
#include <vector>
#include <iostream>

SpaceDefenders::SpaceDefenders(SDL_Window* window, SDL_Renderer* renderer) {
    this->window = window;
    this->renderer = renderer;

    font = TTF_OpenFont(FONT_PATH, 48);
}

SpaceDefenders::~SpaceDefenders() {
    TTF_CloseFont(font);
}

void SpaceDefenders::run() {
    while(running) {
        handleEvents();
        update();
        render();
        
        SDL_Delay(16);
    }
}

void SpaceDefenders::handleEvents() {
        SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            running = false;
        }

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_ESCAPE)
            {
                running = false;
            }
        }
    }
}

void SpaceDefenders::update() {
    // Get Key
    const bool* keyboard = SDL_GetKeyboardState(nullptr);

    // Game

}

void SpaceDefenders::renderText(const char* text, float x, float y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Blended(
        font,
        text,
        0,
        color
    );

    if (!surface)
        return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(
        renderer,
        surface
    );

    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    SDL_FRect dst{
        x,
        y,
        static_cast<float>(surface->w),
        static_cast<float>(surface->h)
    };

    SDL_RenderTexture(
        renderer,
        texture,
        nullptr,
        &dst
    );

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void SpaceDefenders::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // Render Objects

    drawCircle(renderer, earth.position.x, earth.position.y, earth.radius);

    drawPolygon(renderer, {100.0f, 200.0f}, {{{0, 30.0f}, {15.0f, 0}, {-15.0f, 0}}});

    drawComplexBody(renderer, {160.0f, 256.0f}, std::vector<Point>{{0, 30.0f}, {15.0f, 0},{20.0f, 5.0f} , {-15.0f, 0}});

    SDL_RenderPresent(renderer);
}

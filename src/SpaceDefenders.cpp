#include "SpaceDefenders.h"
#include "const.h"

#include <array>
#include <vector>
#include <iostream>
#include <SDL3/SDL.h>

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
        dt = timer.deltaTime();

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

void SpaceDefenders::update()
{
    const bool* keyboard = SDL_GetKeyboardState(nullptr);

    if (keyboard[SDL_SCANCODE_LEFT])
    {
        player.direction -= player.rotationSpeed * dt;
    }

    if (keyboard[SDL_SCANCODE_RIGHT])
    {
        player.direction += player.rotationSpeed * dt;
    }

    player.accelaration = {0.0f, 0.0f};

    if (keyboard[SDL_SCANCODE_UP])
    {
        player.accelaration.x = std::cos(player.direction) * player.thrust;

        player.accelaration.y = std::sin(player.direction) * player.thrust;
    }

    player.velocity.x += (player.accelaration.x + SpaceDefenders::calcGravPullEarth(player.polygon.pos, earth).x) * dt;
    player.velocity.y += (player.accelaration.y + SpaceDefenders::calcGravPullEarth(player.polygon.pos, earth).y) * dt;

    player.polygon.pos.x += player.velocity.x * dt;
    player.polygon.pos.y += player.velocity.y * dt;
}

void SpaceDefenders::renderText(const char* text, float x, float y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, 0, color);

    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    SDL_FRect dst{
        x, y,
        static_cast<float>(surface->w),
        static_cast<float>(surface->h)
    };

    SDL_RenderTexture(renderer, texture, nullptr, &dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void SpaceDefenders::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // Render Objects

    drawCircle(renderer, earth.position, earth.radius);

    drawPolygon(renderer, player.polygon, player.direction);

    SDL_RenderPresent(renderer);
}

SDL_FPoint SpaceDefenders::calcGravPullEarth(const SDL_FPoint& pos, const Planet& earth) {
    float dx = earth.position.x - pos.x;
    float dy = earth.position.y - pos.y;

    float g;

    float r_squared = (dx * dx + dy * dy);
    float r = std::sqrt(r_squared);
    
    if (r_squared < 3025.0f) {
        return {0, 0};
    }

    float r_scaledSquared = r_squared * 8.0E8f;
    g = (gravConstant * earth.mass) / r_scaledSquared;
    
    float angle = std::atan2(dy, dx);

    return { std::cos(angle) * g, std::sin(angle) * g };
}

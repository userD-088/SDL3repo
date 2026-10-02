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
    // Update Lasers
    for (auto it = lasers.begin(); it != lasers.end(); ) {
        it->x = it->x + laserSpeed * cosf(it->angle) * dt;
        it->y = it->y + laserSpeed * sinf(it->angle) * dt;

        float tipX = it->x + it->len * cosf(it->angle);
        float tipY = it->y + it->len * sin(it->angle);

        it->lifetime++;
        
        bool eraseFlag = false;
        eraseFlag = it->x < 0 || it->x > SCREEN_WIDTH || it->y < 0 || it->y > SCREEN_HEIGHT;
        eraseFlag = sqrt((tipX - earth.position.x)*(tipX - earth.position.x) + (tipY - earth.position.y)*(tipY - earth.position.y)) < 55.0f;

        if (eraseFlag) {
            it = lasers.erase(it);
        }
        else {
            ++it;
        }
    }

    // Check if inside of the Planet
    if(isPlayerPlanetIntersect(player, earth)) {
        player.polygon.pos = {100.0f, 100.0f};
        player.velocity = {0.0f, 0.0f};
        player.accelaration = {0.0f, 0.0f};
        
        player.hp--;
    }

    // Inputs
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

    if (keyboard[SDL_SCANCODE_DOWN] ) 
    {
        if (lasers.empty() || lasers.back().lifetime > 20) {
            lasers.push_back({player.polygon.pos.x + 36.0f * cosf(player.direction), player.polygon.pos.y + 36.0f * sinf(player.direction), player.direction, 12.0f, 0});
        }
    }
}

void SpaceDefenders::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // Render Objects
    for (auto& laser : lasers) {
        DrawPolarLine(renderer, laser.x, laser.y, laser.angle, laser.len);
    }

    drawCircle(renderer, earth.position, earth.radius);

    drawPolygon(renderer, player.polygon, player.direction);

    // Render Texts
    std::string textPlayerX = std::to_string(player.polygon.pos.x);
    renderText(renderer, font, textPlayerX.c_str(), 10, 10, white, 0.3f);

    std::string textPlayerY = std::to_string(player.polygon.pos.y);
    renderText(renderer, font, textPlayerY.c_str(), 10, 30, white, 0.3f);

    std::string hearts;
    if (player.hp > 0) {
        for (int i = 0; i < player.hp; ++i) { hearts += "♥ "; }
        renderText(renderer, font, hearts.c_str(), 680, 20, white, 0.8f);
    }

    // Render all
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

bool SpaceDefenders::isPlayerPlanetIntersect(const Player& player, const Planet& planet)
{
    const float radiusSquared = planet.radius * planet.radius;

    auto isInsidePlanet = [&](const SDL_FPoint& point)
    {
        float dx = point.x - planet.position.x;
        float dy = point.y - planet.position.y;

        return dx * dx + dy * dy < radiusSquared;
    };

    if (isInsidePlanet(player.polygon.pos)) return true;

    for (const SDL_FPoint& point : player.polygon.body)
    {
        float rotatedX = point.x * std::cos(player.direction) - point.y * std::sin(player.direction);

        float rotatedY = point.x * std::sin(player.direction) + point.y * std::cos(player.direction);

        SDL_FPoint worldPoint = {
            player.polygon.pos.x + rotatedX,
            player.polygon.pos.y + rotatedY
        };

        if (isInsidePlanet(worldPoint)) return true;
    }

    return false;
}

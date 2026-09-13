#include "Launcher.h"
#include "const.h"
#include "Utils.h"
#include "Pong.h"
#include "SpaceDefenders.h"

#include <iostream>


Launcher::Launcher(SDL_Window* window, SDL_Renderer* renderer)
    : window(window),
      renderer(renderer),
      font(nullptr),
      selectedGame(0),
      running(true)
{
    games = {
        {"Pong"},
        {"Space Defenders"},
        {"..."}
    };

    font = TTF_OpenFont(FONT_PATH, 24);

    if (!font)
    {
        std::cout << "Font Error: "
                  << SDL_GetError()
                  << '\n';
    }
}

void Launcher::run()
{
    while (running)
    {
        handleEvents();
        update();
        render();

        SDL_Delay(16);
    }
}

void Launcher::handleEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            running = false;
        }

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            switch (event.key.key)
            {
                case SDLK_ESCAPE:
                    running = false;
                    break;

                case SDLK_DOWN:
                    selectedGame++;

                    if (selectedGame >= static_cast<int>(games.size()))
                    {
                        selectedGame = 0;
                    }

                    break;

                case SDLK_UP:
                    selectedGame--;

                    if (selectedGame < 0)
                    {
                        selectedGame = static_cast<int>(games.size()) - 1;
                    }

                    break;

                case SDLK_RETURN:
                    std::cout << "Selected: " << selectedGame + 1 << " " << games[selectedGame].name << '\n';

                    if (games[selectedGame].name == "Pong") { Pong pong(window, renderer); pong.run(); }
                    if (games[selectedGame].name == "Space Defenders") { SpaceDefenders spaceDefenders(window, renderer); spaceDefenders.run(); }

                    break;
            }
        }
    }
}

void Launcher::update()
{
    // Later: animations, mouse, start game
}

void Launcher::render()
{
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color selected = {50, 200, 255, 255};

    renderText(
        "GAME LAUNCHER",
        SCREEN_WIDTH / 2 - textWidth("GAME LAUNCHER", font) / 2,
        70,
        white
    );

    renderText(
        "Game List:",
        50,
        100,
        white
    );

    for (int i = 0; i < static_cast<int>(games.size()); i++)
    {
        float y = 140.0f + i * 30.0f;

        if (i == selectedGame)
        {
            std::string text = "> " + std::to_string(i+1) + " " + games[i].name;

            renderText(
                text.c_str(),
                80,
                y,
                selected
            );
        }
        else
        {
            std::string gameText = std::to_string(i+1) + " " + games[i].name;

            renderText(
                gameText.c_str(),
                100,
                y,
                white
            );
        }
    }

    SDL_Color help = {150, 150, 150, 255};

    renderText(
        "UP / DOWN = Select    ENTER = Start    ESC = Exit",
        SCREEN_WIDTH / 2 - textWidth("UP / DOWN = Select    ENTER = Start    ESC = Exit", font) / 2,
        530,
        help
    );

    SDL_RenderPresent(renderer);
}

void Launcher::renderText(
    const char* text,
    float x,
    float y,
    SDL_Color color
)
{
    if (!font)
    {
        return;
    }

    SDL_Surface* surface =
        TTF_RenderText_Blended(
            font,
            text,
            0,
            color
        );

    if (!surface)
    {
        return;
    }

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface
        );

    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    SDL_FRect destination;

    destination.x = x;
    destination.y = y;
    destination.w = static_cast<float>(surface->w);
    destination.h = static_cast<float>(surface->h);

    SDL_RenderTexture(
        renderer,
        texture,
        nullptr,
        &destination
    );

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

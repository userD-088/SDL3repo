#include "Launcher.h"
#include "const.h"
#include "Utils.h"
#include "Pong.h"
#include "SpaceDefenders.h"
#include "VectorRush.h"

#include <iostream>


Launcher::Launcher(SDL_Window* window, SDL_Renderer* renderer)
    : window(window),
      renderer(renderer),
      font(nullptr),
      selectedGame(0),
      running(true)
{
    games = {
        {"Pong",
         "The classic. Play against the CPU or a friend. First to 7 wins.",
         "W/S  -  UP/DOWN  -  P = Pause"},
        {"Space Defenders",
         "Defend Earth from asteroid waves. Mind the gravity!",
         "LEFT/RIGHT = Turn  UP = Thrust  SPACE = Shoot"},
        {"Vector Rush 3D",
         "Fly through a wireframe world. Rings, crystals, drones and bosses.",
         "WASD/Arrows = Fly  SPACE = Shoot  SHIFT = Boost"}
    };

    font = openFont(24);

    for (int i = 0; i < 90; ++i)
    {
        stars.push_back({randomFloat(0, SCREEN_WIDTH), randomFloat(0, SCREEN_HEIGHT), randomFloat(10.0f, 60.0f)});
    }
}

Launcher::~Launcher()
{
    if (font) TTF_CloseFont(font);
}

void Launcher::run()
{
    while (running)
    {
        dt = timer.deltaTime();

        handleEvents();
        update();
        render();

        SDL_Delay(16);
    }
}

void Launcher::startGame(int index)
{
    std::cout << "Selected: " << index + 1 << " " << games[index].name << '\n';

    if (index == 0) { Pong pong(window, renderer); pong.run(); }
    if (index == 1) { SpaceDefenders spaceDefenders(window, renderer); spaceDefenders.run(); }
    if (index == 2) { VectorRush vectorRush(window, renderer); vectorRush.run(); }

    // Games leave blend mode / draw color in any state -> reset
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Skip the long frame of the game session
    timer.deltaTime();
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

        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
        {
            switch (event.key.key)
            {
                case SDLK_ESCAPE:
                    running = false;
                    break;

                case SDLK_DOWN:
                case SDLK_S:
                    selectedGame++;

                    if (selectedGame >= static_cast<int>(games.size()))
                    {
                        selectedGame = 0;
                    }

                    break;

                case SDLK_UP:
                case SDLK_W:
                    selectedGame--;

                    if (selectedGame < 0)
                    {
                        selectedGame = static_cast<int>(games.size()) - 1;
                    }

                    break;

                case SDLK_RETURN:
                case SDLK_SPACE:
                    startGame(selectedGame);
                    break;

                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                {
                    int index = static_cast<int>(event.key.key - SDLK_1);

                    if (index < static_cast<int>(games.size()))
                    {
                        selectedGame = index;
                        startGame(selectedGame);
                    }

                    break;
                }

                default:
                    break;
            }
        }
    }
}

void Launcher::update()
{
    time += dt;

    // Slowly drifting stars in the background
    for (Star& star : stars)
    {
        star.y += star.speed * dt;

        if (star.y > SCREEN_HEIGHT)
        {
            star.y = 0;
            star.x = randomFloat(0, SCREEN_WIDTH);
        }
    }
}

void Launcher::render()
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    // Stars
    for (const Star& star : stars)
    {
        Uint8 brightness = static_cast<Uint8>(40 + star.speed * 2.0f);
        SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
        SDL_RenderPoint(renderer, star.x, star.y);
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color selected = {50, 200, 255, 255};
    SDL_Color help = {150, 150, 150, 255};

    renderTextCentered(renderer, font, "GAME LAUNCHER", SCREEN_WIDTH / 2.0f, 50, white, 1.4f);

    // Underline
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderLine(renderer, 250, 92, 550, 92);

    renderText(renderer, font, "Game List:", 50, 120, white);

    for (int i = 0; i < static_cast<int>(games.size()); i++)
    {
        float y = 165.0f + i * 40.0f;

        if (i == selectedGame)
        {
            // Pulsing selection frame
            float pulse = 0.5f + 0.5f * SDL_sinf(time * 4.0f);
            SDL_SetRenderDrawColor(renderer, 50, 200, 255, static_cast<Uint8>(80 + pulse * 120));
            SDL_FRect frame = {70, y - 4, 660, 36};
            SDL_RenderRect(renderer, &frame);

            std::string text = "> " + std::to_string(i+1) + " " + games[i].name;

            renderText(renderer, font, text.c_str(), 80, y, selected);
        }
        else
        {
            std::string gameText = std::to_string(i+1) + " " + games[i].name;

            renderText(renderer, font, gameText.c_str(), 100, y, white);
        }
    }

    // Info box for the selected game
    SDL_FRect infoBox = {50, 330, 700, 140};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &infoBox);

    const GameInfo& game = games[selectedGame];
    renderText(renderer, font, game.name.c_str(), 70, 345, selected);
    renderText(renderer, font, game.description.c_str(), 70, 385, white, 0.75f);
    renderText(renderer, font, game.controls.c_str(), 70, 425, help, 0.75f);

    renderTextCentered(renderer, font, "UP / DOWN = Select    ENTER = Start    ESC = Exit", SCREEN_WIDTH / 2.0f, 530, help);

    SDL_RenderPresent(renderer);
}

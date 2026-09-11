#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <iostream>

#include "const.h"
#include "Launcher.h"

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout << "SDL Error: "
                  << SDL_GetError()
                  << '\n';

        return 1;
    }

    if (!TTF_Init())
    {
        std::cout << "TTF Error: "
                  << SDL_GetError()
                  << '\n';

        SDL_Quit();

        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Game Launcher",
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0
    );

    if (!window)
    {
        std::cout << "Window Error: "
                  << SDL_GetError()
                  << '\n';

        TTF_Quit();
        SDL_Quit();

        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, nullptr);

    if (!renderer)
    {
        std::cout << "Renderer Error: "
                  << SDL_GetError()
                  << '\n';

        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();

        return 1;
    }

    Launcher launcher(window, renderer);

    launcher.run();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}

#include "Pong.h"
#include "const.h"
#include "Utils.h"

Pong::Pong(SDL_Window* window, SDL_Renderer* renderer)
{
    this->window = window;
    this->renderer = renderer;

    font = TTF_OpenFont(FONT_PATH, 48);

    // Left
    player1.rect = {
        710,
        250,
        20,
        100
    };
    player1.speed = 12.0f;
    player1.score = 0;
    
    // Right
    player2.rect = {
        70,
        250,
        20,
        100
    };
    player2.speed = 12.0f;
    player2.score = 0;

    // Ball
    ball.rect = {
        390,
        290,
        20,
        20
    };

    ball.vx = 1.0f;
    ball.vy = 0.6f;
    ball.speed = 8.0f;

    running = true;
}

Pong::~Pong()
{
    TTF_CloseFont(font);
}

void Pong::run()
{
    while (running)
    {
        handleEvents();
        update();
        render();

        SDL_Delay(16);
    }
}

void Pong::handleEvents()
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
            if (event.key.key == SDLK_ESCAPE)
            {
                running = false;
            }
        }
    }
}

void Pong::update()
{
    // Movement
    const bool* keyboard = SDL_GetKeyboardState(nullptr);

    // Move Left
    if (keyboard[SDL_SCANCODE_W])
    {
        displayTxtControlls = false;
        player2.rect.y -= player2.speed;
    }

    if (keyboard[SDL_SCANCODE_S])
    {
        displayTxtControlls = false;
        player2.rect.y += player2.speed;
    }
    
    // Move Right
    if (keyboard[SDL_SCANCODE_UP])
    {
        displayTxtControlls = false;
        player1.rect.y -= player1.speed;
    }

    if (keyboard[SDL_SCANCODE_DOWN])
    {
        displayTxtControlls = false;
        player1.rect.y += player1.speed;
    }

    // Block Movement
    if (player2.rect.y < 50.0f)
    {
        player2.rect.y = 50.0f;
    }

    if (player2.rect.y + player2.rect.h > 550.0f)
    {
        player2.rect.y = 550.0f - player2.rect.h;
    }


    if (player1.rect.y < 50.0f)
    {
        player1.rect.y = 50.0f;
    }

    if (player1.rect.y + player1.rect.h > 550.0f)
    {
        player1.rect.y = 550.0f - player1.rect.h;
    }

    // Ball Movement
    ball.rect.x += ball.vx * ball.speed;
    ball.rect.y += ball.vy * ball.speed;

    // Ball Collision
    if (ball.rect.y <= 50.0f)
    {
        ball.rect.y = 50.0f;
        ball.vy = -ball.vy;
    }

    if (ball.rect.y + ball.rect.h >= 550.0f)
    {
        ball.rect.y = 550.0f - ball.rect.h;
        ball.vy = -ball.vy;
    }

    // Ball-Paddle Collision
    if (SDL_HasRectIntersectionFloat(
            &ball.rect,
            &player1.rect))
    {
        ball.rect.x = player1.rect.x - ball.rect.w;
        ball.vx = -ball.vx;
    }

    if (SDL_HasRectIntersectionFloat(
            &ball.rect,
            &player2.rect))
    {
        ball.rect.x = player2.rect.x + player2.rect.w;
        ball.vx = -ball.vx;
    }

    // Ball Scoring
    if (ball.rect.x + ball.rect.w < 50.0f)
    {
        ball.rect.x = 390.0f;
        ball.rect.y = 290.0f;

        ball.vx = 1.0f;
        ball.vy = 0.6f;
        player1.score++;
    }

    if (ball.rect.x > 750.0f)
    {
        ball.rect.x = 390.0f;
        ball.rect.y = 290.0f;

        ball.vx = -1.0f;
        ball.vy = 0.6f;
        player2.score++;
    }
}

void Pong::renderText(const char* text, float x, float y, SDL_Color color)
{
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

void Pong::render()
{
    SDL_SetRenderDrawColor(
        renderer,
        0,
        0,
        0,
        255
    );

    SDL_RenderClear(renderer);

    // Gamefield
    SDL_FRect field{
        50,
        50,
        700,
        500
    };

    SDL_SetRenderDrawColor(
        renderer,
        255,
        255,
        255,
        255
    );

    SDL_RenderRect(renderer, &field);

    // Left Player
    SDL_RenderFillRect(
        renderer,
        &player1.rect
    );

    // Right Player
    SDL_RenderFillRect(
        renderer,
        &player2.rect
    );

    // Ball
    SDL_RenderFillRect(
        renderer,
        &ball.rect
    );

    // Score Text
    SDL_Color white = { 255, 255, 255, 255 };

    std::string txtScore1 = std::to_string(player1.score);
    std::string txtScore2 = std::to_string(player2.score);
    std::string txtColon = ":";

    renderText(
        txtColon.c_str(),
        SCREEN_WIDTH / 2.0f - static_cast<float>(textWidth(txtColon, font)) / 2.0f,
        70,
        white
    );
    
    txtScore1Buffer = getDigits(player1.score) - 1;

    renderText(
        txtScore1.c_str(),
        SCREEN_WIDTH / 2.0f + static_cast<float>(textWidth(txtScore1, font))*0.9f - txtScore1Buffer * 24.0f,
        70,
        white
    );

    renderText(
        txtScore2.c_str(),
        SCREEN_WIDTH / 2.0f - static_cast<float>(textWidth(txtScore2, font))*0.9f - 25.0f,
        70,
        white
    );

    // Controlls Infotext
    if (displayTxtControlls) {
        SDL_Color gray = { 255, 255, 255, 100 };
        std::string txtControlls = "Left Player: W/S, Right Player: Arrows, Quit: ESC";
        font = TTF_OpenFont(FONT_PATH, 24);

        renderText(
            txtControlls.c_str(),
            SCREEN_WIDTH / 2.0f - static_cast<float>(textWidth(txtControlls, font)) / 2.0f,
            460,
            gray
        );

        font = TTF_OpenFont(FONT_PATH, 48);
    }

    SDL_RenderPresent(renderer);
}

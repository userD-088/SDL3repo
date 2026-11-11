#include "Pong.h"
#include "const.h"
#include "Utils.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float FIELD_LEFT = 50.0f;
    constexpr float FIELD_TOP = 50.0f;
    constexpr float FIELD_RIGHT = 750.0f;
    constexpr float FIELD_BOTTOM = 550.0f;

    constexpr float BALL_START_SPEED = 420.0f;
    constexpr float BALL_SPEED_UP = 28.0f;
    constexpr float BALL_MAX_SPEED = 950.0f;
    constexpr float PADDLE_SPEED = 520.0f;
    constexpr float MAX_BOUNCE_ANGLE = SDL_PI_F / 3.0f;

    const char* MENU_ENTRIES[] = {
        "1 PLAYER  -  EASY",
        "1 PLAYER  -  NORMAL",
        "1 PLAYER  -  HARD",
        "2 PLAYERS"
    };
}

Pong::Pong(SDL_Window* window, SDL_Renderer* renderer)
{
    this->window = window;
    this->renderer = renderer;

    font = openFont(48);
    smallFont = openFont(24);

    // Right
    player1.rect = {710, 250, 20, 100};
    player1.speed = PADDLE_SPEED;
    player1.score = 0;

    // Left
    player2.rect = {70, 250, 20, 100};
    player2.speed = PADDLE_SPEED;
    player2.score = 0;

    // Ball
    ball.rect = {390, 290, 20, 20};
    ball.vx = 1.0f;
    ball.vy = 0.0f;
    ball.speed = BALL_START_SPEED;

    running = true;
}

Pong::~Pong()
{
    if (font) TTF_CloseFont(font);
    if (smallFont) TTF_CloseFont(smallFont);
}

void Pong::run()
{
    while (running)
    {
        // Clamp to avoid huge jumps (e.g. window dragging)
        dt = std::min(timer.deltaTime(), 0.05f);

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

            // Let the launcher close as well
            SDL_PushEvent(&event);
            return;
        }

        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
        {
            continue;
        }

        SDL_Keycode key = event.key.key;

        switch (state)
        {
            case State::Menu:
                handleMenuKey(key);
                break;

            case State::Serve:
            case State::Playing:
                if (key == SDLK_ESCAPE) state = State::Menu;
                if (key == SDLK_P) state = State::Paused;
                break;

            case State::Paused:
                if (key == SDLK_ESCAPE) state = State::Menu;
                if (key == SDLK_P || key == SDLK_RETURN) state = State::Playing;
                break;

            case State::GameOver:
                if (key == SDLK_RETURN || key == SDLK_SPACE) startMatch();
                if (key == SDLK_ESCAPE) state = State::Menu;
                break;
        }
    }
}

void Pong::handleMenuKey(SDL_Keycode key)
{
    switch (key)
    {
        case SDLK_ESCAPE:
            running = false;
            break;

        case SDLK_UP:
        case SDLK_W:
            menuSelection = (menuSelection + 4) % 5;
            break;

        case SDLK_DOWN:
        case SDLK_S:
            menuSelection = (menuSelection + 1) % 5;
            break;

        case SDLK_LEFT:
        case SDLK_A:
            if (menuSelection == 4) winningScore = std::max(3, winningScore - 2);
            break;

        case SDLK_RIGHT:
        case SDLK_D:
            if (menuSelection == 4) winningScore = std::min(21, winningScore + 2);
            break;

        case SDLK_RETURN:
        case SDLK_SPACE:
            if (menuSelection < 3)
            {
                singlePlayer = true;
                difficulty = menuSelection;
                startMatch();
            }
            else if (menuSelection == 3)
            {
                singlePlayer = false;
                startMatch();
            }
            break;

        default:
            break;
    }
}

void Pong::startMatch()
{
    player1.score = 0;
    player2.score = 0;
    player1.rect.y = 250;
    player2.rect.y = 250;

    rally = 0;
    longestRally = 0;
    particles.clear();
    displayTxtControlls = true;

    resetBall(randomInt(0, 1) == 0 ? -1 : 1);
}

void Pong::resetBall(int direction)
{
    ball.rect.x = 390.0f;
    ball.rect.y = 290.0f;
    ball.speed = BALL_START_SPEED;

    float angle = randomFloat(-0.5f, 0.5f);
    ball.vx = std::cos(angle) * direction;
    ball.vy = std::sin(angle);

    serveDirection = direction;
    serveTimer = 1.2f;
    rally = 0;
    trail.clear();

    state = State::Serve;
}

void Pong::update()
{
    time += dt;

    // Effects are animated in every state
    for (auto it = particles.begin(); it != particles.end(); )
    {
        it->x += it->vx * dt;
        it->y += it->vy * dt;
        it->vx *= 0.96f;
        it->vy *= 0.96f;
        it->life -= dt;

        if (it->life <= 0.0f) it = particles.erase(it);
        else ++it;
    }

    shake = std::max(0.0f, shake - dt * 30.0f);
    player1.flash = std::max(0.0f, player1.flash - dt * 4.0f);
    player2.flash = std::max(0.0f, player2.flash - dt * 4.0f);

    if (state == State::Serve)
    {
        updatePaddles();

        serveTimer -= dt;
        if (serveTimer <= 0.0f) state = State::Playing;

        return;
    }

    if (state != State::Playing)
    {
        return;
    }

    updatePaddles();
    updateBall();
}

void Pong::updatePaddles()
{
    const bool* keyboard = SDL_GetKeyboardState(nullptr);

    // Left Player (W/S, in single player also the arrows)
    float leftInput = 0.0f;
    if (keyboard[SDL_SCANCODE_W]) leftInput -= 1.0f;
    if (keyboard[SDL_SCANCODE_S]) leftInput += 1.0f;

    if (singlePlayer)
    {
        if (keyboard[SDL_SCANCODE_UP]) leftInput -= 1.0f;
        if (keyboard[SDL_SCANCODE_DOWN]) leftInput += 1.0f;
        leftInput = std::clamp(leftInput, -1.0f, 1.0f);
    }

    player2.rect.y += leftInput * player2.speed * dt;

    // Right Player
    if (singlePlayer)
    {
        updateCpu(player1);
    }
    else
    {
        float rightInput = 0.0f;
        if (keyboard[SDL_SCANCODE_UP]) rightInput -= 1.0f;
        if (keyboard[SDL_SCANCODE_DOWN]) rightInput += 1.0f;
        player1.rect.y += rightInput * player1.speed * dt;

        if (rightInput != 0.0f) displayTxtControlls = false;
    }

    if (leftInput != 0.0f) displayTxtControlls = false;

    // Block Movement
    for (Paddle* paddle : {&player1, &player2})
    {
        paddle->rect.y = std::clamp(paddle->rect.y, FIELD_TOP, FIELD_BOTTOM - paddle->rect.h);
    }
}

void Pong::updateCpu(Paddle& paddle)
{
    // Difficulty: reaction delay, max speed and aiming error
    const float speedFactor[] = {0.55f, 0.8f, 1.05f};
    const float reactionTime[] = {0.35f, 0.18f, 0.06f};

    float targetY = 300.0f;

    bool ballIncoming = ball.vx > 0.0f && state == State::Playing;

    if (ballIncoming)
    {
        cpuReaction -= dt;

        if (cpuReaction <= 0.0f)
        {
            // Predict where the ball will cross the paddle, including wall bounces
            float distance = paddle.rect.x - (ball.rect.x + ball.rect.w);
            float travelTime = distance / std::max(1.0f, ball.vx * ball.speed);
            float predicted = ball.rect.y + ball.rect.h / 2.0f + ball.vy * ball.speed * travelTime;

            float top = FIELD_TOP + ball.rect.h / 2.0f;
            float bottom = FIELD_BOTTOM - ball.rect.h / 2.0f;
            float range = bottom - top;

            float local = std::fmod(predicted - top, 2.0f * range);
            if (local < 0) local += 2.0f * range;
            if (local > range) local = 2.0f * range - local;

            // Easy CPU only looks at the current ball position
            targetY = (difficulty == 0 ? ball.rect.y + ball.rect.h / 2.0f : top + local) + cpuError;
        }
        else
        {
            targetY = paddle.rect.y + paddle.rect.h / 2.0f;
        }
    }
    else
    {
        cpuReaction = reactionTime[difficulty];
    }

    float center = paddle.rect.y + paddle.rect.h / 2.0f;
    float diff = targetY - center;
    float maxStep = paddle.speed * speedFactor[difficulty] * dt;

    if (std::fabs(diff) > 6.0f)
    {
        paddle.rect.y += std::clamp(diff, -maxStep, maxStep);
    }
}

void Pong::updateBall()
{
    // Trail
    trail.push_front({ball.rect.x, ball.rect.y});
    if (trail.size() > 12) trail.pop_back();

    // Substeps against tunneling at high speeds
    float totalDistance = ball.speed * dt;
    int steps = std::max(1, static_cast<int>(std::ceil(totalDistance / 8.0f)));
    float stepTime = dt / steps;

    for (int i = 0; i < steps; ++i)
    {
        ball.rect.x += ball.vx * ball.speed * stepTime;
        ball.rect.y += ball.vy * ball.speed * stepTime;

        // Ball Collision
        if (ball.rect.y <= FIELD_TOP)
        {
            ball.rect.y = FIELD_TOP;
            ball.vy = std::fabs(ball.vy);
            spawnParticles(ball.rect.x + 10, FIELD_TOP, 4, 0.0f);
        }

        if (ball.rect.y + ball.rect.h >= FIELD_BOTTOM)
        {
            ball.rect.y = FIELD_BOTTOM - ball.rect.h;
            ball.vy = -std::fabs(ball.vy);
            spawnParticles(ball.rect.x + 10, FIELD_BOTTOM, 4, 0.0f);
        }

        // Ball-Paddle Collision
        if (ball.vx > 0.0f && SDL_HasRectIntersectionFloat(&ball.rect, &player1.rect))
        {
            ball.rect.x = player1.rect.x - ball.rect.w;
            bounceOffPaddle(player1, -1);
        }

        if (ball.vx < 0.0f && SDL_HasRectIntersectionFloat(&ball.rect, &player2.rect))
        {
            ball.rect.x = player2.rect.x + player2.rect.w;
            bounceOffPaddle(player2, 1);
        }

        // Ball Scoring
        if (ball.rect.x + ball.rect.w < FIELD_LEFT)
        {
            scorePoint(player1, -1);
            return;
        }

        if (ball.rect.x > FIELD_RIGHT)
        {
            scorePoint(player2, 1);
            return;
        }
    }
}

void Pong::bounceOffPaddle(Paddle& paddle, int direction)
{
    // Where did the ball hit? -1 = top edge, 1 = bottom edge
    float paddleCenter = paddle.rect.y + paddle.rect.h / 2.0f;
    float ballCenter = ball.rect.y + ball.rect.h / 2.0f;
    float offset = std::clamp((ballCenter - paddleCenter) / (paddle.rect.h / 2.0f), -1.0f, 1.0f);

    float angle = offset * MAX_BOUNCE_ANGLE;

    ball.vx = std::cos(angle) * direction;
    ball.vy = std::sin(angle);
    ball.speed = std::min(BALL_MAX_SPEED, ball.speed + BALL_SPEED_UP);

    paddle.flash = 1.0f;
    rally++;
    longestRally = std::max(longestRally, rally);

    // New aiming error for the CPU after every hit
    const float errorRange[] = {55.0f, 30.0f, 12.0f};
    cpuError = randomFloat(-errorRange[difficulty], errorRange[difficulty]);

    spawnParticles(ball.rect.x + ball.rect.w / 2.0f, ballCenter, 10, static_cast<float>(direction));
}

void Pong::scorePoint(Paddle& scorer, int nextServeDirection)
{
    scorer.score++;
    shake = 10.0f;

    spawnParticles(ball.rect.x + 10, ball.rect.y + 10, 30, static_cast<float>(-nextServeDirection));

    if (scorer.score >= winningScore)
    {
        bool leftWon = &scorer == &player2;

        if (singlePlayer) winnerText = leftWon ? "YOU WIN!" : "CPU WINS!";
        else winnerText = leftWon ? "LEFT PLAYER WINS!" : "RIGHT PLAYER WINS!";

        state = State::GameOver;
        return;
    }

    // The player who conceded gets the serve
    resetBall(nextServeDirection);
}

void Pong::spawnParticles(float x, float y, int count, float dirX)
{
    for (int i = 0; i < count; ++i)
    {
        float angle = randomFloat(0.0f, 2.0f * SDL_PI_F);
        float speed = randomFloat(60.0f, 260.0f);

        particles.push_back({
            x, y,
            std::cos(angle) * speed + dirX * 150.0f,
            std::sin(angle) * speed,
            randomFloat(0.3f, 0.8f)
        });
    }
}

void Pong::renderField()
{
    float offsetX = shake > 0.0f ? randomFloat(-shake, shake) : 0.0f;
    float offsetY = shake > 0.0f ? randomFloat(-shake, shake) : 0.0f;

    // Gamefield
    SDL_FRect field{FIELD_LEFT + offsetX, FIELD_TOP + offsetY, 700, 500};

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &field);

    // Dashed center line
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 90);
    for (float y = FIELD_TOP + 10.0f; y < FIELD_BOTTOM - 10.0f; y += 30.0f)
    {
        SDL_FRect dash = {SCREEN_WIDTH / 2.0f - 2.0f + offsetX, y + offsetY, 4.0f, 15.0f};
        SDL_RenderFillRect(renderer, &dash);
    }

    drawCircle(renderer, {SCREEN_WIDTH / 2.0f + offsetX, SCREEN_HEIGHT / 2.0f + offsetY}, 50.0f);

    // Paddles (flash when hit)
    for (Paddle* paddle : {&player1, &player2})
    {
        SDL_FRect rect = paddle->rect;
        rect.x += offsetX;
        rect.y += offsetY;

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &rect);

        if (paddle->flash > 0.0f)
        {
            float grow = (1.0f - paddle->flash) * 14.0f;
            SDL_FRect glow = {rect.x - grow, rect.y - grow, rect.w + grow * 2, rect.h + grow * 2};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(paddle->flash * 200));
            SDL_RenderRect(renderer, &glow);
        }
    }

    // Ball trail
    for (size_t i = 0; i < trail.size(); ++i)
    {
        float t = 1.0f - static_cast<float>(i) / trail.size();
        float size = ball.rect.w * t;

        SDL_FRect part = {
            trail[i].x + (ball.rect.w - size) / 2.0f + offsetX,
            trail[i].y + (ball.rect.h - size) / 2.0f + offsetY,
            size, size
        };

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(t * 90));
        SDL_RenderFillRect(renderer, &part);
    }

    // Ball (blinks while waiting for the serve)
    bool showBall = state != State::Serve || std::fmod(time, 0.3f) < 0.15f;

    if (showBall && state != State::GameOver)
    {
        SDL_FRect ballRect = ball.rect;
        ballRect.x += offsetX;
        ballRect.y += offsetY;

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &ballRect);
    }

    // Particles
    for (const Particle& p : particles)
    {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(std::min(1.0f, p.life * 2.0f) * 255));
        SDL_FRect rect = {p.x - 1.5f + offsetX, p.y - 1.5f + offsetY, 3.0f, 3.0f};
        SDL_RenderFillRect(renderer, &rect);
    }

    // Score Text
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {255, 255, 255, 110};

    std::string txtScore1 = std::to_string(player1.score);
    std::string txtScore2 = std::to_string(player2.score);

    renderTextCentered(renderer, font, ":", SCREEN_WIDTH / 2.0f, 70, white);
    renderText(renderer, font, txtScore1.c_str(), SCREEN_WIDTH / 2.0f + 25.0f, 70, white);
    renderText(renderer, font, txtScore2.c_str(), SCREEN_WIDTH / 2.0f - 25.0f - textWidth(txtScore2, font), 70, white);

    // Rally counter
    if (rally >= 3)
    {
        renderTextCentered(renderer, smallFont, "Rally " + std::to_string(rally), SCREEN_WIDTH / 2.0f, 515, gray);
    }

    renderText(renderer, smallFont, singlePlayer ? "YOU" : "LEFT", 60, 15, gray, 0.8f);
    std::string rightLabel = singlePlayer ? "CPU" : "RIGHT";
    renderText(renderer, smallFont, rightLabel.c_str(), 740 - textWidth(rightLabel, smallFont, 0.8f), 15, gray, 0.8f);
    renderTextCentered(renderer, smallFont, "First to " + std::to_string(winningScore), SCREEN_WIDTH / 2.0f, 15, gray, 0.8f);

    // Controlls Infotext
    if (displayTxtControlls && state != State::GameOver)
    {
        std::string txtControlls = singlePlayer
            ? "Move: W/S or Arrows   Pause: P   Menu: ESC"
            : "Left Player: W/S, Right Player: Arrows, Pause: P";

        renderTextCentered(renderer, smallFont, txtControlls, SCREEN_WIDTH / 2.0f, 460, gray);
    }

    if (state == State::Serve)
    {
        renderTextCentered(renderer, smallFont, "READY", SCREEN_WIDTH / 2.0f, 240, white);
    }

    if (state == State::Paused)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
        SDL_FRect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &overlay);

        renderTextCentered(renderer, font, "PAUSED", SCREEN_WIDTH / 2.0f, 230, white);
        renderTextCentered(renderer, smallFont, "P = Continue   ESC = Menu", SCREEN_WIDTH / 2.0f, 300, gray);
    }

    if (state == State::GameOver)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
        SDL_FRect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &overlay);

        renderTextCentered(renderer, font, winnerText, SCREEN_WIDTH / 2.0f, 200, white);
        renderTextCentered(renderer, smallFont, "Longest rally: " + std::to_string(longestRally), SCREEN_WIDTH / 2.0f, 280, gray);
        renderTextCentered(renderer, smallFont, "ENTER = Rematch   ESC = Menu", SCREEN_WIDTH / 2.0f, 330, gray);
    }
}

void Pong::renderMenu()
{
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {255, 255, 255, 110};

    // Animated demo ball bouncing in the background
    float bx = 400.0f + std::sin(time * 1.3f) * 330.0f;
    float by = 300.0f + std::sin(time * 2.1f) * 230.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 40);
    SDL_FRect demo = {bx - 10, by - 10, 20, 20};
    SDL_RenderFillRect(renderer, &demo);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_FRect field{FIELD_LEFT, FIELD_TOP, 700, 500};
    SDL_RenderRect(renderer, &field);

    renderTextCentered(renderer, font, "P O N G", SCREEN_WIDTH / 2.0f, 90, white, 1.4f);

    for (int i = 0; i < 5; ++i)
    {
        std::string text;

        if (i < 4) text = MENU_ENTRIES[i];
        else text = "WINNING SCORE:  < " + std::to_string(winningScore) + " >";

        float y = 230.0f + i * 45.0f + (i == 4 ? 15.0f : 0.0f);

        if (i == menuSelection)
        {
            SDL_FRect frame = {220, y - 6, 360, 40};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderRect(renderer, &frame);

            renderTextCentered(renderer, smallFont, text, SCREEN_WIDTH / 2.0f, y, white);
        }
        else
        {
            renderTextCentered(renderer, smallFont, text, SCREEN_WIDTH / 2.0f, y, gray);
        }
    }

    renderTextCentered(renderer, smallFont, "ENTER = Start   LEFT/RIGHT = Change   ESC = Back", SCREEN_WIDTH / 2.0f, 500, gray, 0.8f);
}

void Pong::render()
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (state == State::Menu) renderMenu();
    else renderField();

    SDL_RenderPresent(renderer);
}

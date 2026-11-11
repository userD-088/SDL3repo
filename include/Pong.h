#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <deque>

#include "Utils.h"

class Pong
{
public:
    Pong(SDL_Window* window, SDL_Renderer* renderer);
    ~Pong();

    void run();

private:
    enum class State {
        Menu,
        Serve,
        Playing,
        Paused,
        GameOver
    };

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
        float flash = 0.0f;
    };

    struct Particle {
        float x;
        float y;
        float vx;
        float vy;
        float life;
    };

    void handleEvents();
    void handleMenuKey(SDL_Keycode key);
    void update();
    void render();

    void renderMenu();
    void renderField();

    void startMatch();
    void resetBall(int direction);
    void updatePaddles();
    void updateCpu(Paddle& paddle);
    void updateBall();
    void bounceOffPaddle(Paddle& paddle, int direction);
    void spawnParticles(float x, float y, int count, float dirX);
    void scorePoint(Paddle& scorer, int nextServeDirection);

    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* smallFont;

    Timer timer;
    float dt = 0.0f;
    float time = 0.0f;

    State state = State::Menu;

    // Menu: 0..2 = 1 Player (Easy/Normal/Hard), 3 = 2 Players, 4 = winning score
    int menuSelection = 1;
    int difficulty = 1;
    bool singlePlayer = true;
    int winningScore = 7;

    Paddle player1; // Right paddle (Arrows / CPU)
    Paddle player2; // Left paddle (W/S)
    Ball ball;

    std::deque<SDL_FPoint> trail;
    std::vector<Particle> particles;

    float serveTimer = 0.0f;
    int serveDirection = 1;

    int rally = 0;
    int longestRally = 0;

    float shake = 0.0f;
    float cpuError = 0.0f;
    float cpuReaction = 0.0f;

    std::string winnerText;

    bool displayTxtControlls = true;

    bool running;
};

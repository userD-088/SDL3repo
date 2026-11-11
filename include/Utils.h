#pragma once

#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <cmath>

// Utility
// Opens FONT_PATH and falls back to common system fonts (Linux / macOS) if it is missing
TTF_Font* openFont(float size);

int textWidth(const std::string& text, TTF_Font* font, float textSize = 1.0f);

int getDigits(int number);

void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, float x, float y, SDL_Color color, float textSize = 1.0f);
void renderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, float centerX, float y, SDL_Color color, float textSize = 1.0f);

// Highscores (stored in the SDL preference folder)
int loadHighscore(const std::string& game);
void saveHighscore(const std::string& game, int score);

// Random helpers
float randomFloat(float min, float max);
int randomInt(int min, int max);

// Delta Time
class Timer {
    public:
        Timer();

        float deltaTime();

    private:
        std::chrono::steady_clock::time_point lastTime;
};

// Geometry Tools and Structs
struct Polygon {
    SDL_FPoint pos;
    std::vector<SDL_FPoint> body;
};

enum class CoordinateMode {
    Absolute,
    Relative,
    Polar
};

// 2D Polygon
void drawPolygon(SDL_Renderer* renderer, const Polygon& polygon, float direction = 0);
void drawFilledPolygon(SDL_Renderer* renderer, const Polygon& polygon);

// 2D Round Object
void drawCircle(SDL_Renderer* renderer, const SDL_FPoint& pos, float radius, int segments = 64);
void drawFilledCircle(SDL_Renderer* renderer, const SDL_FPoint& pos, float radius, int segments = 64);

void drawEllipse(SDL_Renderer* renderer, const SDL_FPoint& pos, const SDL_FPoint& body, int segments = 64);
void drawFilledEllipse(SDL_Renderer* renderer, const SDL_FPoint& pos, const SDL_FPoint& body, int segments = 64);

void drawArc(SDL_Renderer* renderer, const SDL_FPoint& pos, float radius, float startAngle, float endAngle, int segments = 32);
void drawSector(SDL_Renderer* renderer, const SDL_FPoint& pos, float radius, float startAngle, float endAngle, int segments = 32);

// Special 1D Objects
void drawCurve(SDL_Renderer* renderer, const SDL_FPoint& start, const SDL_FPoint& control, const SDL_FPoint& end, CoordinateMode mode, int segments = 32);

void drawArrow(SDL_Renderer* renderer, const SDL_FPoint& start, const SDL_FPoint& value, CoordinateMode mode, float headLength = 10.0f, float headAngle = SDL_PI_F / 6.0f);

void DrawPolarLine(SDL_Renderer* renderer, float x1, float y1, float angle, float length);

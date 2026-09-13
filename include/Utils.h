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
int textWidth(const std::string& text, TTF_Font* font);

int getDigits(int number);

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
void drawPolygon(SDL_Renderer* renderer, const Polygon& polygon);
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

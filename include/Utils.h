#pragma once

#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <string>
#include <cmath>
#include <vector>

struct Point { float x; float y; };

int textWidth(const std::string& text, TTF_Font* font);

int getDigits(int number);

void drawCircle(SDL_Renderer* renderer, float centerX, float centerY, float radius);

void drawPolygon(SDL_Renderer* renderer, Point rel_pos, const std::array<Point, 3>& body);

void drawComplexBody(SDL_Renderer* renderer, Point rel_pos, const std::vector<Point>& body);

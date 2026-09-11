#pragma once

#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <string>
#include <cmath>

int textWidth(const std::string& text, TTF_Font* font);

int getDigits(int number);

void drawCircle(SDL_Renderer* renderer, float centerX, float centerY, float radius);

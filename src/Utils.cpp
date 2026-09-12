#include "Utils.h"

#include <iostream>
#include <array>

int textWidth(const std::string& text, TTF_Font* font)
{
    int width;
    int height;

    TTF_GetStringSize(
        font,
        text.c_str(),
        0,
        &width,
        &height
    );

    return width;
}

int getDigits(int number)
{
    if (number == 0)
        return 1;

    return static_cast<int>(std::log10(std::abs(number))) + 1;
}

void drawCircle(SDL_Renderer* renderer, float centerX, float centerY, float radius)
{
    const int segments = 64;

    for (int i = 0; i < segments; i++)
    {
        float angle1 = (2.0f * SDL_PI_F * i) / segments;
        float angle2 = (2.0f * SDL_PI_F * (i + 1)) / segments;

        float x1 = centerX + SDL_cosf(angle1) * radius;
        float y1 = centerY + SDL_sinf(angle1) * radius;

        float x2 = centerX + SDL_cosf(angle2) * radius;
        float y2 = centerY + SDL_sinf(angle2) * radius;

        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }
}

void drawPolygon(SDL_Renderer* renderer, Point rel_pos, const std::array<Point, 3>& body) {
    SDL_RenderLine(renderer,  
        rel_pos.x + body[0].x, rel_pos.y + body[0].y, 
        rel_pos.x + body[1].x, rel_pos.y + body[1].y);

    SDL_RenderLine(renderer,  
        rel_pos.x + body[1].x, rel_pos.y + body[1].y, 
        rel_pos.x + body[2].x, rel_pos.y + body[2].y);

    SDL_RenderLine(renderer,  
        rel_pos.x + body[2].x, rel_pos.y + body[2].y, 
        rel_pos.x + body[0].x, rel_pos.y + body[0].y);
}

void drawComplexBody(SDL_Renderer* renderer, Point rel_pos, const std::vector<Point>& body) {
    if (body.size() < 2) {
        std::cout << "Failed to Draw Complex Body: Not Enough Points";
        return;
    }

    for (size_t i = 0; i < body.size(); i++) {
        const Point& a = body[i];
        const Point& b = body[(i + 1) % body.size()];

        SDL_RenderLine(
            renderer,
            rel_pos.x + a.x,
            rel_pos.y + a.y,
            rel_pos.x + b.x,
            rel_pos.y + b.y
        );
    }
}

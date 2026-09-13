#include "Utils.h"

// Utility
int textWidth(const std::string& text, TTF_Font* font)
{
    int width;
    int height;

    TTF_GetStringSize(font, text.c_str(), 0, &width, &height);

    return width;
}

int getDigits(int number)
{
    if (number == 0)
        return 1;

    return static_cast<int>(std::log10(std::abs(number))) + 1;
}

// Coordinate Conversions
static SDL_FPoint toAbsolute(
    const SDL_FPoint& origin,
    const SDL_FPoint& value,
    CoordinateMode mode
) {
    switch (mode)
    {
        case CoordinateMode::Absolute:
        {
            return value;
        }

        case CoordinateMode::Relative:
        {
            return {
                origin.x + value.x,
                origin.y + value.y
            };
        }

        case CoordinateMode::Polar:
        {
            const float angle = value.x;
            const float length = value.y;

            return {
                origin.x + SDL_cosf(angle) * length,
                origin.y + SDL_sinf(angle) * length
            };
        }
    }

    return value;
}

// 2D Polygon
void drawPolygon(SDL_Renderer* renderer, const Polygon& polygon) {
    if (polygon.body.size() < 3) {
        std::cout << "Invalid Polygon size: too small" << std::endl;
        return;
    }

    std::vector<SDL_FPoint> points;

    points.reserve(polygon.body.size() + 1);

    for (const SDL_FPoint& p : polygon.body) {
        points.push_back({
            polygon.pos.x + p.x,
            polygon.pos.y + p.y
        });
    }

    points.push_back(points[0]);

    SDL_RenderLines(renderer, points.data(), static_cast<int>(points.size()));
}

// 2D Round Object
void drawCircle(SDL_Renderer* renderer, const SDL_FPoint& pos, float radius, int segments) {
    constexpr int MAX_SEGMENTS = 256;

    segments = std::clamp(segments, 3, MAX_SEGMENTS);

    SDL_FPoint points[MAX_SEGMENTS + 1];

    const float angleStep = 2.0f * SDL_PI_F / segments;

    const float cosStep = SDL_cosf(angleStep);
    const float sinStep = SDL_sinf(angleStep);

    float cosAngle = 1.0f;
    float sinAngle = 0.0f;

    for (int i = 0; i <= segments; ++i) {
        points[i] = {
            pos.x + cosAngle * radius,
            pos.y + sinAngle * radius
        };

        const float newCos = cosAngle * cosStep - sinAngle * sinStep;
        const float newSin = sinAngle * cosStep + cosAngle * sinStep;

        cosAngle = newCos;
        sinAngle = newSin;
    }

    SDL_RenderLines(
        renderer,
        points,
        segments + 1
    );
}

void drawEllipse(SDL_Renderer* renderer, const SDL_FPoint& pos, float radiusX, float radiusY, int segments) {
    constexpr int MAX_SEGMENTS = 256;

    segments = std::clamp(segments, 3, MAX_SEGMENTS);

    SDL_FPoint points[MAX_SEGMENTS + 1];

    const float angleStep = 2.0f * SDL_PI_F / segments;

    const float cosStep = SDL_cosf(angleStep);
    const float sinStep = SDL_sinf(angleStep);

    float cosAngle = 1.0f;
    float sinAngle = 0.0f;

    for (int i = 0; i <= segments; ++i) {
        points[i] = {
            pos.x + cosAngle * radiusX,
            pos.y + sinAngle * radiusY
        };

        const float newCos = cosAngle * cosStep - sinAngle * sinStep;

        const float newSin = sinAngle * cosStep + cosAngle * sinStep;

        cosAngle = newCos;
        sinAngle = newSin;
    }

    SDL_RenderLines(renderer, points, segments + 1);
}

void drawArc(SDL_Renderer* renderer, const SDL_FPoint& pos, float radius, float startAngle, float endAngle, int segments) {
    constexpr int MAX_SEGMENTS = 256;

    segments = std::clamp(segments, 2, MAX_SEGMENTS);

    SDL_FPoint points[MAX_SEGMENTS + 1];

    const float angleStep = (endAngle - startAngle) / segments;

    const float cosStep = SDL_cosf(angleStep);
    const float sinStep = SDL_sinf(angleStep);

    float cosAngle = SDL_cosf(startAngle);
    float sinAngle = SDL_sinf(startAngle);

    for (int i = 0; i <= segments; ++i) {
        points[i] = {
            pos.x + cosAngle * radius,
            pos.y + sinAngle * radius
        };

        const float newCos = cosAngle * cosStep - sinAngle * sinStep;

        const float newSin = sinAngle * cosStep + cosAngle * sinStep;

        cosAngle = newCos;
        sinAngle = newSin;
    }

    SDL_RenderLines(renderer, points, segments + 1);
}

// Special 1D Objects
void drawCurve(SDL_Renderer* renderer, const SDL_FPoint& start, const SDL_FPoint& control, const SDL_FPoint& end, CoordinateMode mode, int segments) {
    constexpr int MAX_SEGMENTS = 256;

    segments = std::clamp(segments, 2, MAX_SEGMENTS);

    // Convert control and end into absolute coordinates
    const SDL_FPoint absoluteControl = toAbsolute(start, control, mode);

    const SDL_FPoint absoluteEnd = toAbsolute(start, end, mode);

    SDL_FPoint points[MAX_SEGMENTS + 1];

    const float step = 1.0f / segments;

    for (int i = 0; i <= segments; ++i)
    {
        const float t = i * step;

        const float invT = 1.0f - t;

        points[i] = {
            invT * invT * start.x + 2.0f * invT * t * absoluteControl.x + t * t * absoluteEnd.x,

            invT * invT * start.y + 2.0f * invT * t * absoluteControl.y + t * t * absoluteEnd.y
        };
    }

    SDL_RenderLines(renderer, points, segments + 1);
}

void drawArrow(SDL_Renderer* renderer, const SDL_FPoint& start, const SDL_FPoint& value, CoordinateMode mode, float headLength, float headAngle) {
    // Convert input into absolute end position
    const SDL_FPoint end = toAbsolute(start, value, mode);

    // Direction vector
    const SDL_FPoint direction = {
        end.x - start.x,
        end.y - start.y
    };

    // Main line
    SDL_RenderLine(renderer, start.x, start.y, end.x, end.y);

    // Direction angle
    const float angle =
        SDL_atan2f(direction.y, direction.x);

    // Arrow head
    const SDL_FPoint left = {
        end.x - SDL_cosf(angle - headAngle) * headLength,

        end.y - SDL_sinf(angle - headAngle) * headLength
    };

    const SDL_FPoint right = {
        end.x - SDL_cosf(angle + headAngle) * headLength,

        end.y - SDL_sinf(angle + headAngle) * headLength
    };

    SDL_RenderLine(renderer, end.x, end.y, left.x, left.y);

    SDL_RenderLine(renderer, end.x, end.y, right.x, right.y);
}

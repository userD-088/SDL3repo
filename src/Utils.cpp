#include "Utils.h"

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

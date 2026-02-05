#pragma once

#include <cstdint>

namespace konflikt {

/// A rectangle with position and dimensions
struct Rect
{
    int32_t x {};
    int32_t y {};
    int32_t width {};
    int32_t height {};

    Rect() = default;

    Rect(int32_t x, int32_t y, int32_t width, int32_t height)
        : x(x)
        , y(y)
        , width(width)
        , height(height)
    {
    }

    /// Check if a point is inside this rectangle
    bool contains(int32_t px, int32_t py) const
    {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    /// Get the right edge x coordinate
    int32_t right() const { return x + width; }

    /// Get the bottom edge y coordinate
    int32_t bottom() const { return y + height; }

    /// Check if two rectangles intersect
    bool intersects(const Rect &other) const
    {
        return x < other.right() && right() > other.x &&
            y < other.bottom() && bottom() > other.y;
    }

    bool operator==(const Rect &other) const
    {
        return x == other.x && y == other.y &&
            width == other.width && height == other.height;
    }

    bool operator!=(const Rect &other) const
    {
        return !(*this == other);
    }
};

} // namespace konflikt

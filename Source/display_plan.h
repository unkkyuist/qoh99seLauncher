#pragma once
#include <algorithm>
#include <stdexcept>

namespace qoh_display {
struct Size { int width; int height; };

// All launcher presets are 4:3. available excludes the title bar and borders.
inline Size FitWindow(Size requested, Size available) {
    const int units = std::min({requested.width / 4, requested.height / 3,
                                available.width / 4, available.height / 3});
    if (units < 160)
        throw std::runtime_error("The desktop is too small for a 640x480 game window. Select borderless fullscreen.");
    return {units * 4, units * 3};
}
}

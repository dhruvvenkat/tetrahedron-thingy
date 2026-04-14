#include "animations.h"

#include <algorithm>
#include <cmath>
#include <string>

void draw_waves(Canvas& canvas, double t) {
    const std::string ramp = " .:-=+*#%@";

    for (int y = 1; y < canvas.height() - 1; ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const double nx = static_cast<double>(x) / canvas.width();
            const double ny = static_cast<double>(y) / canvas.height();
            const double wave =
                std::sin(nx * 16.0 + t * 2.1) +
                std::sin((nx + ny) * 11.0 - t * 1.7) +
                std::cos(ny * 14.0 + t * 1.3);
            const double normalized = (wave + 3.0) / 6.0;
            const auto index = static_cast<std::size_t>(
                std::clamp(normalized, 0.0, 0.999) * ramp.size());
            const int color = 36 + static_cast<int>(std::floor(normalized * 2.0));
            canvas.plot(x, y, ramp[index], color);
        }
    }
}

#ifndef ANIMATION_LIBRARY
#include "standalone.h"

int main(int argc, char** argv) {
    return run_standalone_animation(argc, argv, "waves", draw_waves, AnimationClock::elapsed);
}
#endif

#include "animations.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double pi = 3.14159265358979323846;

} // namespace

void draw_orbits(Canvas& canvas, double t) {
    const double cx = (canvas.width() - 1) / 2.0;
    const double cy = (canvas.height() - 1) / 2.0;
    const double radius = std::min(canvas.width(), canvas.height()) * 0.32;

    for (int ring = 0; ring < 5; ++ring) {
        const double ring_radius = radius * (0.35 + ring * 0.16);
        const int color = 31 + ring;
        const int steps = 18 + ring * 7;
        for (int i = 0; i < steps; ++i) {
            const double a = (2.0 * pi * i / steps) + t * (0.35 + ring * 0.1);
            const double pulse = std::sin(t * 2.0 + ring + i * 0.4) * 0.18;
            const int x = static_cast<int>(std::round(cx + std::cos(a) * ring_radius * (1.0 + pulse)));
            const int y = static_cast<int>(std::round(cy + std::sin(a) * ring_radius * 0.55));
            const char glyph = (i + ring) % 3 == 0 ? '*' : '.';
            canvas.plot(x, y, glyph, color);
        }
    }

    for (int i = 0; i < 10; ++i) {
        const double a = t * (0.9 + i * 0.05) + i * 2.0 * pi / 10.0;
        const int x = static_cast<int>(std::round(cx + std::cos(a) * radius * 0.22));
        const int y = static_cast<int>(std::round(cy + std::sin(a) * radius * 0.12));
        canvas.plot(x, y, '@', 97);
    }
}

#ifndef ANIMATION_LIBRARY
#include "standalone.h"

int main(int argc, char** argv) {
    return run_standalone_animation(argc, argv, "orbits", draw_orbits, AnimationClock::elapsed);
}
#endif

#include "animations.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

struct Star {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double speed = 0.0;
};

std::mt19937& rng() {
    static std::mt19937 generator{std::random_device{}()};
    return generator;
}

double random_range(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng());
}

void reset_star(Star& star) {
    star.x = random_range(-1.0, 1.0);
    star.y = random_range(-1.0, 1.0) * 0.55;
    star.z = random_range(0.2, 1.0);
    star.speed = 0.45 + random_range(0.0, 1.0) * 0.75;
}

std::vector<Star>& stars_for(Canvas& canvas) {
    static std::vector<Star> stars;
    static int width = 0;
    static int height = 0;

    if (stars.empty() || width != canvas.width() || height != canvas.height()) {
        width = canvas.width();
        height = canvas.height();
        const int count = std::max(80, canvas.width() * canvas.height() / 18);
        stars.resize(static_cast<std::size_t>(count));
        for (Star& star : stars) {
            reset_star(star);
        }
    }

    return stars;
}

} // namespace

void draw_starfield(Canvas& canvas, double dt) {
    const double cx = (canvas.width() - 1) / 2.0;
    const double cy = (canvas.height() - 1) / 2.0;
    std::vector<Star>& stars = stars_for(canvas);

    for (Star& star : stars) {
        star.z -= dt * star.speed;
        if (star.z <= 0.05) {
            reset_star(star);
            star.z = 1.0;
        }

        const int x = static_cast<int>(std::round(cx + star.x * cx / star.z));
        const int y = static_cast<int>(std::round(cy + star.y * cy / star.z));
        if (x < 0 || y < 0 || x >= canvas.width() || y >= canvas.height()) {
            reset_star(star);
            continue;
        }

        const char glyph = star.z < 0.25 ? '#' : star.z < 0.55 ? '*' : '.';
        const int color = star.z < 0.35 ? 97 : 37;
        canvas.plot(x, y, glyph, color);
    }
}

#ifndef ANIMATION_LIBRARY
#include "standalone.h"

int main(int argc, char** argv) {
    return run_standalone_animation(argc, argv, "starfield", draw_starfield, AnimationClock::delta);
}
#endif

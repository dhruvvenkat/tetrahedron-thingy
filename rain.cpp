#include "animations.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

struct Drop {
    double x = 0.0;
    double y = 0.0;
    double speed = 0.0;
    int length = 0;
};

std::mt19937& rng() {
    static std::mt19937 generator{std::random_device{}()};
    return generator;
}

double random_unit() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng());
}

void reset_drop(Drop& drop, const Canvas& canvas) {
    drop.x = std::floor(random_unit() * canvas.width());
    drop.y = 0.0;
    drop.speed = 12.0 + random_unit() * 28.0;
    drop.length = 4 + static_cast<int>(random_unit() * 9.0);
}

std::vector<Drop>& drops_for(Canvas& canvas) {
    static std::vector<Drop> drops;
    static int width = 0;
    static int height = 0;

    if (drops.empty() || width != canvas.width() || height != canvas.height()) {
        width = canvas.width();
        height = canvas.height();
        const int count = std::max(40, canvas.width() / 2);
        drops.resize(static_cast<std::size_t>(count));
        for (Drop& drop : drops) {
            reset_drop(drop, canvas);
            drop.y = random_unit() * canvas.height();
        }
    }

    return drops;
}

} // namespace

void draw_rain(Canvas& canvas, double dt) {
    std::vector<Drop>& drops = drops_for(canvas);

    for (Drop& drop : drops) {
        drop.y += dt * drop.speed;
        if (drop.y - drop.length > canvas.height()) {
            reset_drop(drop, canvas);
            drop.y = -drop.length;
        }

        for (int i = 0; i < drop.length; ++i) {
            const int y = static_cast<int>(std::round(drop.y)) - i;
            const int color = i == 0 ? 97 : (i < 3 ? 92 : 32);
            canvas.plot(static_cast<int>(drop.x), y, i == 0 ? '|' : '.', color);
        }
    }
}

#ifndef ANIMATION_LIBRARY
#include "standalone.h"

int main(int argc, char** argv) {
    return run_standalone_animation(argc, argv, "rain", draw_rain, AnimationClock::delta);
}
#endif

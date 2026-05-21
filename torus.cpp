#include "animations.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

constexpr double pi = 3.14159265358979323846;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Vec3 rotate(Vec3 point, double ax, double ay, double az) {
    double c = std::cos(ax);
    double s = std::sin(ax);
    double y = point.y * c - point.z * s;
    double z = point.y * s + point.z * c;
    point.y = y;
    point.z = z;

    c = std::cos(ay);
    s = std::sin(ay);
    double x = point.x * c + point.z * s;
    z = -point.x * s + point.z * c;
    point.x = x;
    point.z = z;

    c = std::cos(az);
    s = std::sin(az);
    x = point.x * c - point.y * s;
    y = point.x * s + point.y * c;
    point.x = x;
    point.y = y;

    return point;
}

Vec3 normalize(Vec3 point) {
    const double length = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
    if (length == 0.0) {
        return {0.0, 1.0, 0.0};
    }
    return {point.x / length, point.y / length, point.z / length};
}

double dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace

void draw_torus(Canvas& canvas, double t) {
    const double cx = (canvas.width() - 1) / 2.0;
    const double cy = (canvas.height() - 1) / 2.0;
    const double major_radius = 1.0;
    const double minor_radius = 0.36;
    const double max_radius = major_radius + minor_radius;
    const double perspective_padding = 1.45;
    const double usable_width = std::max(8.0, static_cast<double>(canvas.width() - 4));
    const double usable_height = std::max(6.0, static_cast<double>(canvas.height() - 4));
    const double scale = std::min(
        usable_width / (2.0 * max_radius * perspective_padding),
        usable_height / (2.0 * max_radius * 0.54 * perspective_padding));
    const double camera_distance = 4.2;
    const int major_steps = std::clamp(canvas.width() * 2, 96, 260);
    const int minor_steps = std::clamp(canvas.height() * 3, 36, 120);
    const double ax = 0.55 + t * 0.72;
    const double ay = t * 1.04;
    const double az = t * 0.28;
    const Vec3 light = normalize({-0.45, 0.68, 1.0});
    const std::string ramp = ".:-=+*#%@";

    for (int i = 0; i < major_steps; ++i) {
        const double u = 2.0 * pi * static_cast<double>(i) / major_steps;
        const double cu = std::cos(u);
        const double su = std::sin(u);

        for (int j = 0; j < minor_steps; ++j) {
            const double v = 2.0 * pi * static_cast<double>(j) / minor_steps;
            const double cv = std::cos(v);
            const double sv = std::sin(v);
            const Vec3 point = {
                (major_radius + minor_radius * cv) * cu,
                (major_radius + minor_radius * cv) * su,
                minor_radius * sv,
            };
            const Vec3 normal = {cv * cu, cv * su, sv};
            const Vec3 rotated = rotate(point, ax, ay, az);
            const Vec3 rotated_normal = rotate(normal, ax, ay, az);
            const double perspective = camera_distance / (camera_distance - rotated.z);
            const int x = static_cast<int>(std::round(cx + rotated.x * perspective * scale));
            const int y = static_cast<int>(std::round(cy - rotated.y * perspective * scale * 0.54));
            const double light_level = std::clamp(dot(normalize(rotated_normal), light), 0.0, 1.0);
            const double depth_level = std::clamp((rotated.z + max_radius) / (2.0 * max_radius), 0.0, 1.0);
            const double intensity = std::clamp(0.20 + light_level * 0.68 + depth_level * 0.12, 0.0, 1.0);
            const std::size_t shade = static_cast<std::size_t>(
                std::round(intensity * static_cast<double>(ramp.size() - 1)));
            const int color = intensity > 0.78 ? 97 : (depth_level > 0.56 ? 37 : 90);
            canvas.plot_depth(x, y, rotated.z, ramp[shade], color);
        }
    }
}

#ifndef ANIMATION_LIBRARY
#include "standalone.h"

int main(int argc, char** argv) {
    return run_standalone_animation(argc, argv, "torus", draw_torus, AnimationClock::elapsed);
}
#endif

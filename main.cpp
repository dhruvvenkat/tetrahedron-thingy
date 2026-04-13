#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <limits>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr int color_reset = 0;
constexpr int min_hedron_faces = 8;
constexpr int max_hedron_faces = 1200;

volatile std::sig_atomic_t keep_running = 1;

void handle_interrupt(int) {
    keep_running = 0;
}

struct TerminalGuard {
    TerminalGuard() {
        std::cout << "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H";
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &original_input_) == 0) {
            termios raw = original_input_;
            raw.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            input_is_raw_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
        }
    }

    ~TerminalGuard() {
        if (input_is_raw_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original_input_);
        }
        std::cout << "\x1b[0m\x1b[?25h\x1b[?1049l" << std::flush;
    }

private:
    termios original_input_{};
    bool input_is_raw_ = false;
};

struct Options {
    int width = 0;
    int height = 0;
    int fps = 60;
    int frames = 0;
    int initial_faces = 96;
    std::string scene = "hedron";
};

struct ProjectedPoint {
    int x = 0;
    int y = 0;
    double depth = 0.0;
};

struct Cell {
    char glyph = ' ';
    int color = color_reset;
};

class Canvas {
public:
    Canvas(int width, int height)
        : width_(width),
          height_(height),
          cells_(static_cast<std::size_t>(width * height)),
          depths_(static_cast<std::size_t>(width * height), -std::numeric_limits<double>::infinity()) {}

    void clear(char fill = ' ') {
        std::fill(cells_.begin(), cells_.end(), Cell{fill, color_reset});
        std::fill(depths_.begin(), depths_.end(), -std::numeric_limits<double>::infinity());
    }

    void plot(int x, int y, char glyph, int color) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            return;
        }
        cells_[static_cast<std::size_t>(y * width_ + x)] = Cell{glyph, color};
    }

    void plot_depth(int x, int y, double depth, char glyph, int color) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(y * width_ + x);
        if (depth < depths_[index]) {
            return;
        }

        depths_[index] = depth;
        cells_[index] = Cell{glyph, color};
    }

    void text(int x, int y, std::string_view value, int color) {
        for (std::size_t i = 0; i < value.size(); ++i) {
            plot(x + static_cast<int>(i), y, value[i], color);
        }
    }

    void line(int x0, int y0, int x1, int y1, char glyph, int color) {
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            plot(x0, y0, glyph, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void line_depth(ProjectedPoint from, ProjectedPoint to, char glyph, int color) {
        const int dx = to.x - from.x;
        const int dy = to.y - from.y;
        const int steps = std::max(std::abs(dx), std::abs(dy));
        if (steps == 0) {
            plot_depth(from.x, from.y, from.depth, glyph, color);
            return;
        }

        for (int i = 0; i <= steps; ++i) {
            const double progress = static_cast<double>(i) / steps;
            const int x = static_cast<int>(std::round(from.x + dx * progress));
            const int y = static_cast<int>(std::round(from.y + dy * progress));
            const double depth = from.depth + (to.depth - from.depth) * progress;
            plot_depth(x, y, depth + 0.002, glyph, color);
        }
    }

    void triangle(ProjectedPoint a, ProjectedPoint b, ProjectedPoint c, char glyph, int color) {
        const int min_x = std::max(0, std::min({a.x, b.x, c.x}) - 1);
        const int max_x = std::min(width_ - 1, std::max({a.x, b.x, c.x}) + 1);
        const int min_y = std::max(0, std::min({a.y, b.y, c.y}) - 1);
        const int max_y = std::min(height_ - 1, std::max({a.y, b.y, c.y}) + 1);
        const double area = edge_value(a, b, c.x, c.y);

        if (std::abs(area) < 0.001) {
            line_depth(a, b, glyph, color);
            line_depth(b, c, glyph, color);
            line_depth(c, a, glyph, color);
            return;
        }

        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const double w0 = edge_value(b, c, x, y);
                const double w1 = edge_value(c, a, x, y);
                const double w2 = edge_value(a, b, x, y);
                const bool inside = area > 0.0
                    ? (w0 >= 0.0 && w1 >= 0.0 && w2 >= 0.0)
                    : (w0 <= 0.0 && w1 <= 0.0 && w2 <= 0.0);
                if (!inside) {
                    continue;
                }

                const double alpha = w0 / area;
                const double beta = w1 / area;
                const double gamma = w2 / area;
                const double depth = a.depth * alpha + b.depth * beta + c.depth * gamma;
                plot_depth(x, y, depth, glyph, color);
            }
        }
    }

    void present() const {
        std::string frame;
        frame.reserve(static_cast<std::size_t>(width_ * height_) + 1024);
        frame += "\x1b[H";
        int active_color = -1;
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const Cell& cell = cells_[static_cast<std::size_t>(y * width_ + x)];
                if (cell.color != active_color) {
                    frame += "\x1b[";
                    frame += std::to_string(cell.color);
                    frame += "m";
                    active_color = cell.color;
                }
                frame += cell.glyph;
            }
            frame += "\x1b[0m";
            if (y + 1 < height_) {
                frame += '\n';
            }
            active_color = -1;
        }
        std::cout << frame << std::flush;
    }

    int width() const {
        return width_;
    }

    int height() const {
        return height_;
    }

private:
    static double edge_value(ProjectedPoint a, ProjectedPoint b, double x, double y) {
        return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
    }

    int width_ = 0;
    int height_ = 0;
    std::vector<Cell> cells_;
    std::vector<double> depths_;
};

struct Star {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double speed = 0.0;
};

struct Drop {
    double x = 0.0;
    double y = 0.0;
    double speed = 0.0;
    int length = 0;
};

struct Point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct MeshFace {
    int a = 0;
    int b = 0;
    int c = 0;
};

struct InputEvents {
    int up_presses = 0;
    int down_presses = 0;
    bool quit = false;
};

char line_glyph(int x0, int y0, int x1, int y1) {
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    if (dx > dy * 2) {
        return '-';
    }
    if (dy > dx * 2) {
        return '|';
    }
    return ((x1 - x0) > 0) == ((y1 - y0) > 0) ? '\\' : '/';
}

Point3 rotate_point(Point3 point, double t) {
    const double ax = t * 1.18;
    const double ay = t * 0.86;
    const double az = t * 0.42;

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

Point3 normalize(Point3 point) {
    const double length = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
    if (length == 0.0) {
        return {0.0, 1.0, 0.0};
    }
    return {point.x / length, point.y / length, point.z / length};
}

Point3 midpoint(Point3 a, Point3 b) {
    return normalize({
        (a.x + b.x) * 0.5,
        (a.y + b.y) * 0.5,
        (a.z + b.z) * 0.5,
    });
}

double squared_distance(Point3 a, Point3 b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

Point3 subtract(Point3 a, Point3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Point3 add(Point3 a, Point3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Point3 cross(Point3 a, Point3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

double dot(Point3 a, Point3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

InputEvents read_input_events() {
    InputEvents events;
    static std::string pending;

    while (true) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(STDIN_FILENO, &read_set);

        timeval timeout{};
        const int ready = select(STDIN_FILENO + 1, &read_set, nullptr, nullptr, &timeout);
        if (ready <= 0 || !FD_ISSET(STDIN_FILENO, &read_set)) {
            break;
        }

        char buffer[64];
        const ssize_t read_count = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (read_count <= 0) {
            break;
        }
        pending.append(buffer, static_cast<std::size_t>(read_count));
    }

    while (!pending.empty()) {
        const char key = pending.front();
        if (key == 'q' || key == 'Q') {
            events.quit = true;
            pending.erase(0, 1);
        } else if (key == '\x1b') {
            if (pending.size() < 3) {
                break;
            }
            if ((pending[1] == '[' || pending[1] == 'O') && (pending[2] == 'A' || pending[2] == 'B')) {
                if (pending[2] == 'A') {
                    ++events.up_presses;
                } else {
                    ++events.down_presses;
                }
                pending.erase(0, 3);
            } else {
                pending.erase(0, 1);
            }
        } else {
            pending.erase(0, 1);
        }
    }

    return events;
}

class AnimationState {
public:
    explicit AnimationState(const Canvas& canvas)
        : rng_(std::random_device{}()),
          x_dist_(-1.0, 1.0),
          y_dist_(-1.0, 1.0),
          z_dist_(0.2, 1.0),
          unit_dist_(0.0, 1.0) {
        reset_hedron();
        reset_stars(canvas);
        reset_drops(canvas);
    }

    void add_hedron_faces(int count) {
        for (int i = 0; i < count && static_cast<int>(hedron_faces_.size()) < max_hedron_faces; ++i) {
            split_largest_hedron_face();
        }
    }

    void set_hedron_faces(int target) {
        target = std::clamp(target, min_hedron_faces, max_hedron_faces);
        if (target < hedron_face_count()) {
            reset_hedron();
        }
        add_hedron_faces(std::max(0, target - hedron_face_count()));
    }

    int hedron_face_count() const {
        return static_cast<int>(hedron_faces_.size());
    }

    void draw_hedron(Canvas& canvas, double t) {
        std::vector<ProjectedPoint> projected(hedron_vertices_.size());
        std::vector<Point3> rotated_vertices(hedron_vertices_.size());
        const double cx = (canvas.width() - 1) / 2.0;
        const double cy = std::max(4.0, (canvas.height() - 2) / 2.0);
        const double scale = std::min(canvas.width() * 0.22, canvas.height() * 0.72);
        const double camera_distance = 4.0;

        for (std::size_t i = 0; i < hedron_vertices_.size(); ++i) {
            const Point3 rotated = rotate_point(hedron_vertices_[i], t);
            rotated_vertices[i] = rotated;
            const double perspective = camera_distance / (camera_distance - rotated.z);
            projected[i] = {
                static_cast<int>(std::round(cx + rotated.x * perspective * scale)),
                static_cast<int>(std::round(cy - rotated.y * perspective * scale * 0.52)),
                rotated.z,
            };
        }

        std::vector<int> draw_order;
        draw_order.reserve(hedron_faces_.size());
        for (std::size_t i = 0; i < hedron_faces_.size(); ++i) {
            draw_order.push_back(static_cast<int>(i));
        }
        std::sort(draw_order.begin(), draw_order.end(), [&](int left, int right) {
            const MeshFace& a = hedron_faces_[static_cast<std::size_t>(left)];
            const MeshFace& b = hedron_faces_[static_cast<std::size_t>(right)];
            const double left_depth =
                projected[static_cast<std::size_t>(a.a)].depth +
                projected[static_cast<std::size_t>(a.b)].depth +
                projected[static_cast<std::size_t>(a.c)].depth;
            const double right_depth =
                projected[static_cast<std::size_t>(b.a)].depth +
                projected[static_cast<std::size_t>(b.b)].depth +
                projected[static_cast<std::size_t>(b.c)].depth;
            return left_depth < right_depth;
        });

        const Point3 light = normalize({-0.85, 0.18, 0.5});
        const std::string shade_ramp = ".:-=+*#%@";

        auto face_shade = [&](const MeshFace& face) {
            const Point3 a = rotated_vertices[static_cast<std::size_t>(face.a)];
            const Point3 b = rotated_vertices[static_cast<std::size_t>(face.b)];
            const Point3 c = rotated_vertices[static_cast<std::size_t>(face.c)];
            Point3 normal = normalize(cross(subtract(b, a), subtract(c, a)));
            const Point3 centroid = normalize(add(add(a, b), c));
            if (dot(normal, centroid) < 0.0) {
                normal = {-normal.x, -normal.y, -normal.z};
            }

            const double lambert = std::max(0.0, dot(normal, light));
            const double facing = std::max(0.0, normal.z) * 0.18;
            const double intensity = std::clamp(0.14 + lambert * 0.82 + facing, 0.0, 1.0);
            const std::size_t shade_index = static_cast<std::size_t>(
                std::round(intensity * static_cast<double>(shade_ramp.size() - 1)));
            const int color = intensity > 0.78 ? 97 : (intensity > 0.42 ? 37 : 90);
            return Cell{shade_ramp[shade_index], color};
        };

        auto draw_edge = [&](int from, int to) {
            const ProjectedPoint& a = projected[static_cast<std::size_t>(from)];
            const ProjectedPoint& b = projected[static_cast<std::size_t>(to)];
            const double depth = (a.depth + b.depth) * 0.5;
            const char glyph = hedron_faces_.size() <= 24 ? line_glyph(a.x, a.y, b.x, b.y) : '.';
            const int color = depth > 0.65 && hedron_faces_.size() <= 24 ? 37 : 90;
            canvas.line_depth(a, b, glyph, color);
        };

        for (int index : draw_order) {
            const MeshFace& face = hedron_faces_[static_cast<std::size_t>(index)];
            const Cell shade = face_shade(face);
            canvas.triangle(
                projected[static_cast<std::size_t>(face.a)],
                projected[static_cast<std::size_t>(face.b)],
                projected[static_cast<std::size_t>(face.c)],
                shade.glyph,
                shade.color);
        }

        for (int index : draw_order) {
            const MeshFace& face = hedron_faces_[static_cast<std::size_t>(index)];
            draw_edge(face.a, face.b);
            draw_edge(face.b, face.c);
            draw_edge(face.c, face.a);
        }

        if (hedron_faces_.size() <= 48) {
            for (const ProjectedPoint& point : projected) {
                canvas.plot_depth(point.x, point.y, point.depth + 0.004, 'o', point.depth > 0.0 ? 97 : 37);
            }
        }

        const std::string status =
            "hedron faces: " + std::to_string(hedron_face_count()) + "  light: left  Up/Down: +/-1 face  q/Ctrl-C: quit";
        canvas.text(2, canvas.height() - 1, status.substr(0, static_cast<std::size_t>(std::max(0, canvas.width() - 4))), 37);
    }

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

    void draw_starfield(Canvas& canvas, double dt) {
        const double cx = (canvas.width() - 1) / 2.0;
        const double cy = (canvas.height() - 1) / 2.0;

        for (Star& star : stars_) {
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

    void draw_rain(Canvas& canvas, double dt) {
        for (Drop& drop : drops_) {
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

private:
    void reset_hedron() {
        hedron_vertices_ = {
            {0.0, 1.0, 0.0},
            {1.0, 0.0, 0.0},
            {0.0, 0.0, 1.0},
            {-1.0, 0.0, 0.0},
            {0.0, 0.0, -1.0},
            {0.0, -1.0, 0.0},
        };
        hedron_faces_ = {
            {0, 1, 2},
            {0, 2, 3},
            {0, 3, 4},
            {0, 4, 1},
            {5, 2, 1},
            {5, 3, 2},
            {5, 4, 3},
            {5, 1, 4},
        };
    }

    void split_largest_hedron_face() {
        std::size_t best_index = 0;
        double best_length = -1.0;
        int best_from = 0;
        int best_to = 0;
        int best_opposite = 0;

        for (std::size_t i = 0; i < hedron_faces_.size(); ++i) {
            const MeshFace& face = hedron_faces_[i];
            auto consider = [&](int from, int to, int opposite) {
                const double length = squared_distance(
                    hedron_vertices_[static_cast<std::size_t>(from)],
                    hedron_vertices_[static_cast<std::size_t>(to)]);
                if (length > best_length) {
                    best_length = length;
                    best_index = i;
                    best_from = from;
                    best_to = to;
                    best_opposite = opposite;
                }
            };

            consider(face.a, face.b, face.c);
            consider(face.b, face.c, face.a);
            consider(face.c, face.a, face.b);
        }

        const int mid = static_cast<int>(hedron_vertices_.size());
        hedron_vertices_.push_back(midpoint(
            hedron_vertices_[static_cast<std::size_t>(best_from)],
            hedron_vertices_[static_cast<std::size_t>(best_to)]));
        hedron_faces_[best_index] = {best_from, mid, best_opposite};
        hedron_faces_.push_back({mid, best_to, best_opposite});
    }

    void reset_stars(const Canvas& canvas) {
        const int count = std::max(80, canvas.width() * canvas.height() / 18);
        stars_.resize(static_cast<std::size_t>(count));
        for (Star& star : stars_) {
            reset_star(star);
        }
    }

    void reset_star(Star& star) {
        star.x = x_dist_(rng_);
        star.y = y_dist_(rng_) * 0.55;
        star.z = z_dist_(rng_);
        star.speed = 0.45 + unit_dist_(rng_) * 0.75;
    }

    void reset_drops(const Canvas& canvas) {
        const int count = std::max(40, canvas.width() / 2);
        drops_.resize(static_cast<std::size_t>(count));
        for (Drop& drop : drops_) {
            reset_drop(drop, canvas);
            drop.y = unit_dist_(rng_) * canvas.height();
        }
    }

    void reset_drop(Drop& drop, const Canvas& canvas) {
        drop.x = std::floor(unit_dist_(rng_) * canvas.width());
        drop.y = 0.0;
        drop.speed = 12.0 + unit_dist_(rng_) * 28.0;
        drop.length = 4 + static_cast<int>(unit_dist_(rng_) * 9.0);
    }

    std::vector<Point3> hedron_vertices_;
    std::vector<MeshFace> hedron_faces_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> x_dist_;
    std::uniform_real_distribution<double> y_dist_;
    std::uniform_real_distribution<double> z_dist_;
    std::uniform_real_distribution<double> unit_dist_;
    std::vector<Star> stars_;
    std::vector<Drop> drops_;
};

int parse_int(std::string_view value, int fallback) {
    try {
        return std::stoi(std::string(value));
    } catch (...) {
        return fallback;
    }
}

struct TerminalSize {
    int width = 140;
    int height = 44;
};

TerminalSize detect_terminal_size() {
    TerminalSize size;

    winsize window{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 0 && window.ws_row > 0) {
        size.width = static_cast<int>(window.ws_col) - 1;
        size.height = static_cast<int>(window.ws_row) - 1;
        return size;
    }

    const int columns = std::atoi(std::getenv("COLUMNS") == nullptr ? "" : std::getenv("COLUMNS"));
    const int lines = std::atoi(std::getenv("LINES") == nullptr ? "" : std::getenv("LINES"));
    if (columns > 0) {
        size.width = columns - 1;
    }
    if (lines > 0) {
        size.height = lines - 1;
    }

    return size;
}

Options parse_args(int argc, char** argv) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                return {};
            }
            return argv[++i];
        };

        if (arg == "--scene") {
            options.scene = next();
        } else if (arg == "--width") {
            options.width = parse_int(next(), options.width);
        } else if (arg == "--height") {
            options.height = parse_int(next(), options.height);
        } else if (arg == "--fps") {
            options.fps = parse_int(next(), options.fps);
        } else if (arg == "--frames") {
            options.frames = parse_int(next(), options.frames);
        } else if (arg == "--faces") {
            options.initial_faces = parse_int(next(), options.initial_faces);
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: ./anim [--scene hedron|cube|cycle|orbits|waves|starfield|rain]\n"
                << "              [--width auto] [--height auto] [--fps 60] [--faces 96] [--frames 0]\n"
                << "\n"
                << "In hedron mode, press Up Arrow to add one face and Down Arrow to remove one. Press q or Ctrl-C to quit.\n"
                << "Set --frames 0 to loop until Ctrl-C.\n";
            std::exit(0);
        }
    }

    const TerminalSize terminal_size = detect_terminal_size();
    if (options.width <= 0) {
        options.width = terminal_size.width;
    }
    if (options.height <= 0) {
        options.height = terminal_size.height;
    }

    options.width = std::clamp(options.width, 20, 480);
    options.height = std::clamp(options.height, 10, 160);
    options.fps = std::clamp(options.fps, 1, 240);
    options.frames = std::max(0, options.frames);
    options.initial_faces = std::clamp(options.initial_faces, min_hedron_faces, max_hedron_faces);
    if (options.scene.empty()) {
        options.scene = "hedron";
    }

    return options;
}

std::string active_scene(const std::string& requested, int frame, int fps) {
    if (requested != "cycle") {
        return requested;
    }

    const int scene_index = (frame / (fps * 6)) % 5;
    if (scene_index == 0) {
        return "hedron";
    }
    if (scene_index == 1) {
        return "orbits";
    }
    if (scene_index == 2) {
        return "waves";
    }
    if (scene_index == 3) {
        return "starfield";
    }
    return "rain";
}

void draw_title(Canvas& canvas, std::string_view scene) {
    const std::string label = "C++ terminal animation: " + std::string(scene) + "  (Ctrl-C to quit)";
    canvas.text(2, 0, label.substr(0, static_cast<std::size_t>(std::max(0, canvas.width() - 4))), 97);
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_interrupt);
    const Options options = parse_args(argc, argv);
    TerminalGuard terminal;
    Canvas canvas(options.width, options.height);
    AnimationState state(canvas);
    state.set_hedron_faces(options.initial_faces);

    using clock = std::chrono::steady_clock;
    const auto frame_time = std::chrono::duration<double>(1.0 / options.fps);
    const auto start = clock::now();
    auto previous = clock::now();

    for (int frame = 0; keep_running != 0 && (options.frames == 0 || frame < options.frames); ++frame) {
        const InputEvents input = read_input_events();
        if (input.quit) {
            keep_running = 0;
        }
        state.set_hedron_faces(state.hedron_face_count() + input.up_presses - input.down_presses);

        const auto now = clock::now();
        const double dt = std::chrono::duration<double>(now - previous).count();
        previous = now;

        const double t = std::chrono::duration<double>(now - start).count();
        const std::string scene = active_scene(options.scene, frame, options.fps);
        canvas.clear();

        if (scene == "hedron" || scene == "cube") {
            state.draw_hedron(canvas, t);
        } else if (scene == "orbits") {
            state.draw_orbits(canvas, t);
        } else if (scene == "waves") {
            state.draw_waves(canvas, t);
        } else if (scene == "starfield") {
            state.draw_starfield(canvas, dt);
        } else if (scene == "rain") {
            state.draw_rain(canvas, dt);
        } else {
            canvas.text(2, options.height / 2, "Unknown scene. Try --scene hedron|cube|cycle|orbits|waves|starfield|rain.", 31);
        }

        if (scene != "hedron" && scene != "cube") {
            draw_title(canvas, scene);
        }
        canvas.present();
        std::this_thread::sleep_until(now + frame_time);
    }

    return 0;
}

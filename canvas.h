#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

inline constexpr int color_reset = 0;

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

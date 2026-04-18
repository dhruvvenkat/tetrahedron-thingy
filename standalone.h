#pragma once

#include "canvas.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

enum class AnimationClock {
    elapsed,
    delta,
};

inline volatile std::sig_atomic_t standalone_keep_running = 1;

inline void handle_standalone_interrupt(int) {
    standalone_keep_running = 0;
}

struct StandaloneTerminalGuard {
    StandaloneTerminalGuard() {
        std::cout << "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H";
#ifdef _WIN32
        output_handle_ = GetStdHandle(STD_OUTPUT_HANDLE);
        if (output_handle_ != INVALID_HANDLE_VALUE && GetConsoleMode(output_handle_, &original_output_mode_)) {
            output_mode_is_valid_ = true;
            SetConsoleMode(output_handle_, original_output_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
#endif
    }

    ~StandaloneTerminalGuard() {
#ifdef _WIN32
        if (output_mode_is_valid_) {
            SetConsoleMode(output_handle_, original_output_mode_);
        }
#endif
        std::cout << "\x1b[0m\x1b[?25h\x1b[?1049l" << std::flush;
    }

private:
#ifdef _WIN32
    HANDLE output_handle_ = INVALID_HANDLE_VALUE;
    DWORD original_output_mode_ = 0;
    bool output_mode_is_valid_ = false;
#endif
};

struct StandaloneOptions {
    int width = 140;
    int height = 44;
    int fps = 60;
    int frames = 0;
};

inline int parse_standalone_int(std::string_view value, int fallback) {
    try {
        return std::stoi(std::string(value));
    } catch (...) {
        return fallback;
    }
}

inline StandaloneOptions detect_standalone_terminal_size() {
    StandaloneOptions options;

#ifdef _WIN32
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (output != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(output, &info)) {
        options.width = static_cast<int>(info.srWindow.Right - info.srWindow.Left);
        options.height = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top);
        return options;
    }
#else
    winsize window{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 0 && window.ws_row > 0) {
        options.width = static_cast<int>(window.ws_col) - 1;
        options.height = static_cast<int>(window.ws_row) - 1;
        return options;
    }
#endif

    const int columns = std::atoi(std::getenv("COLUMNS") == nullptr ? "" : std::getenv("COLUMNS"));
    const int lines = std::atoi(std::getenv("LINES") == nullptr ? "" : std::getenv("LINES"));
    if (columns > 0) {
        options.width = columns - 1;
    }
    if (lines > 0) {
        options.height = lines - 1;
    }

    return options;
}

inline StandaloneOptions parse_standalone_args(int argc, char** argv, std::string_view name) {
    StandaloneOptions options = detect_standalone_terminal_size();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                return {};
            }
            return argv[++i];
        };

        if (arg == "--width") {
            options.width = parse_standalone_int(next(), options.width);
        } else if (arg == "--height") {
            options.height = parse_standalone_int(next(), options.height);
        } else if (arg == "--fps") {
            options.fps = parse_standalone_int(next(), options.fps);
        } else if (arg == "--frames") {
            options.frames = parse_standalone_int(next(), options.frames);
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: ./" << name << " [--width auto] [--height auto] [--fps 60] [--frames 0]\n"
                << "\n"
                << "Set --frames 0 to loop until Ctrl-C.\n";
            std::exit(0);
        }
    }

    options.width = std::clamp(options.width, 20, 480);
    options.height = std::clamp(options.height, 10, 160);
    options.fps = std::clamp(options.fps, 1, 240);
    options.frames = std::max(0, options.frames);
    return options;
}

inline int run_standalone_animation(
    int argc,
    char** argv,
    std::string_view name,
    void (*draw)(Canvas&, double),
    AnimationClock clock_mode) {
    standalone_keep_running = 1;
    std::signal(SIGINT, handle_standalone_interrupt);
    const StandaloneOptions options = parse_standalone_args(argc, argv, name);
    StandaloneTerminalGuard terminal;
    Canvas canvas(options.width, options.height);

    using clock = std::chrono::steady_clock;
    const auto frame_time = std::chrono::duration<double>(1.0 / options.fps);
    const auto start = clock::now();
    auto previous = clock::now();

    for (int frame = 0; standalone_keep_running != 0 && (options.frames == 0 || frame < options.frames); ++frame) {
        const auto now = clock::now();
        const double dt = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const double elapsed = std::chrono::duration<double>(now - start).count();

        canvas.clear();
        draw(canvas, clock_mode == AnimationClock::elapsed ? elapsed : dt);
        const std::string label = std::string(name) + "  Ctrl-C to quit";
        canvas.text(2, 0, label.substr(0, static_cast<std::size_t>(std::max(0, canvas.width() - 4))), 97);
        canvas.present();
        std::this_thread::sleep_until(now + frame_time);
    }

    return 0;
}

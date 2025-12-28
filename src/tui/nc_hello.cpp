#include <atomic>
#include <csignal>
#include <functional>
#include <memory>
#include <ctime>

#include <ncpp/NotCurses.hh>
#include <ncpp/Plane.hh>
#include <notcurses/notcurses.h>

// Flag flipped on Ctrl-C so the main loop can exit cleanly.
static std::atomic<bool> g_stop{false};
static void OnSigint(int) { g_stop.store(true, std::memory_order_relaxed); }

int main() {
    // Install a minimal SIGINT handler that only flips the flag.
    struct sigaction action {};
    action.sa_handler = OnSigint;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, nullptr);

    // Start NotCurses with banners suppressed; this owns the terminal session.
    notcurses_options nc_options = ncpp::NotCurses::default_notcurses_options;
    nc_options.flags |= NCOPTION_SUPPRESS_BANNERS;
    ncpp::NotCurses nc(nc_options);
    // Standard plane is the root drawing surface that matches the terminal size.
    std::unique_ptr<ncpp::Plane> stdplane{nc.get_stdplane()};

    // Draws the current frame: clear, center text, and render.
    std::function<void()> draw = [&nc, &stdplane]() {
        stdplane->erase();
        unsigned rows = 0;
        unsigned cols = 0;
        stdplane->get_dim(rows, cols);
        const int midrow = static_cast<int>(rows) / 2;
        stdplane->putstr(midrow - 1, ncpp::NCAlign::Center, "Welcome to the audio converter");
        stdplane->putstr(midrow + 1, ncpp::NCAlign::Center, "Press 'q' or Ctrl-C to exit.");
        nc.render();
    };

    draw();

    // Input loop: poll with a short timeout so we still react to Ctrl-C.
    while (!g_stop.load(std::memory_order_relaxed)) {
        ncinput input_details{};
        timespec timeout{0, 100'000'000}; // 100ms
        uint32_t ch = notcurses_get(nc, &timeout, &input_details);
        if (ch == 0) {
            continue;                  // timeout: nothing pressed
        }
        if (static_cast<int32_t>(ch) == -1) {
            break;                     // error from notcurses_get
        }
        if (ch == 'q' || ch == 'Q') {
            break;                     // user asked to quit
        }
        if (ch == NCKEY_RESIZE) {
            draw();                    // terminal resized: redraw
        }
    }

    // NotCurses destructor restores the terminal state.
    return 0;
}

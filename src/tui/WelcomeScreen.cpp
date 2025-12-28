#include "tui/WelcomeScreen.hpp"

#include <vector>
#include <notcurses/notcurses.h>

#include "tui/Signal.hpp"
#include "tui/StateMachine.hpp"

WelcomeScreen::WelcomeScreen() : exit_requested_(false) {}

void WelcomeScreen::Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    // Reset the local exit flag every time we land on this screen.
    (void)machine;
    (void)nc;
    (void)stdplane;
    exit_requested_ = false;
}

void WelcomeScreen::Exit(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    // Nothing to clean up yet, but the hook is here for future screens.
    (void)machine;
    (void)nc;
    (void)stdplane;
}

void WelcomeScreen::Draw(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    // Clear the screen, draw a rounded border, and center the welcome text.
    stdplane.erase();
    stdplane.perimeter_rounded(0, 0, 0);

    unsigned rows = 0;
    unsigned cols = 0;
    stdplane.get_dim(rows, cols);
    const int mid_row = static_cast<int>(rows) / 2;

    stdplane.putstr(mid_row - 1, ncpp::NCAlign::Center, "Welcome to the audio converter");
    stdplane.putstr(mid_row + 1, ncpp::NCAlign::Center, "Press D for the test screen, or Q/Ctrl-C to exit.");
}

void WelcomeScreen::Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)nc;
    (void)stdplane;
    // Exit if the user asked or a SIGINT arrived.
    if (exit_requested_ || g_sigint_received.load(std::memory_order_relaxed)) {
        machine.SetRunning(false);
    }
}

void WelcomeScreen::HandleInput(StateMachine& machine,
                                ncpp::NotCurses& nc,
                                ncpp::Plane& stdplane,
                                uint32_t input,
                                const ncinput& details) {
    (void)stdplane;
    (void)details;

    // Keep behavior minimal for now: D jumps to the test screen; Q quits globally via the loop.
    if (input == 'd' || input == 'D') {
        machine.TransitionTo("test", nc, stdplane);
        return;
    }

    if (input == 'q' || input == 'Q' || input == NCKEY_ENTER) {
        exit_requested_ = true;
    }
    if (input == NCKEY_RESIZE) {
        // Let Draw handle the new dimensions on the next iteration.
    }
}

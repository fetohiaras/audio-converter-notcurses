#include <memory>

#include <ncpp/NotCurses.hh>
#include <ncpp/Plane.hh>
#include <notcurses/notcurses.h>

#include "tui/Signal.hpp"
#include "tui/StateMachine.hpp"
#include "tui/WelcomeScreen.hpp"
#include "tui/TestScreen.hpp"

int main() {
    // Install SIGINT handler early so Ctrl-C can cleanly exit the loop.
    InitSigintHandler();

    // Configure NotCurses and suppress the startup banner.
    notcurses_options nc_options = ncpp::NotCurses::default_notcurses_options;
    nc_options.flags |= NCOPTION_SUPPRESS_BANNERS;
    ncpp::NotCurses nc(nc_options);

    // Grab the root plane; it tracks the terminal size automatically.
    std::unique_ptr<ncpp::Plane> stdplane{nc.get_stdplane()};

    // Wire up the state machine with the initial welcome screen.
    StateMachine machine;
    std::shared_ptr<WelcomeScreen> welcome_state = std::make_shared<WelcomeScreen>();
    std::shared_ptr<TestScreen> test_state = std::make_shared<TestScreen>();
    machine.AddState("welcome", welcome_state);
    machine.AddState("test", test_state);
    machine.TransitionTo("welcome", nc, *stdplane);

    // Enter the main loop: draw, poll, and dispatch to the active state.
    machine.Run(nc, *stdplane);
    return 0;
}

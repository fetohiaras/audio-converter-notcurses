#include <memory>
#include <filesystem>
#include <iostream>

#include <ncpp/NotCurses.hh>
#include <ncpp/Plane.hh>
#include <notcurses/notcurses.h>

#include "tui/Signal.hpp"
#include "tui/Config.hpp"
#include "tui/StateMachine.hpp"
#include "tui/WelcomeScreen.hpp"
#include "tui/TestScreen.hpp"

int main() {
    // Install SIGINT handler early so Ctrl-C can cleanly exit the loop.
    InitSigintHandler();

    // Load converter configuration if present.
    ConverterConfig config;
    std::filesystem::path config_path = std::filesystem::current_path() / "config" / "converter.yml";
    if (!config.LoadFromFile(config_path)) {
        std::cerr << "Warning: could not load config from " << config_path << "; using defaults.\n";
    }

    // Configure NotCurses and suppress the startup banner.
    notcurses_options nc_options = ncpp::NotCurses::default_notcurses_options;
    nc_options.flags |= NCOPTION_SUPPRESS_BANNERS;
    ncpp::NotCurses nc(nc_options);

    // Grab the root plane; it tracks the terminal size automatically.
    std::unique_ptr<ncpp::Plane> stdplane{nc.get_stdplane()};

    bool config_changed = false;

    // Wire up the state machine with the initial welcome screen.
    StateMachine machine;
    std::shared_ptr<WelcomeScreen> welcome_state = std::make_shared<WelcomeScreen>();
    std::shared_ptr<TestScreen> test_state = std::make_shared<TestScreen>(config, config_changed);
    machine.AddState("welcome", welcome_state);
    machine.AddState("test", test_state);
    machine.TransitionTo("welcome", nc, *stdplane);

    // Enter the main loop: draw, poll, and dispatch to the active state.
    machine.Run(nc, *stdplane);

    // If configuration changed, prompt to save.
    if (config_changed) {
        unsigned rows = 0;
        unsigned cols = 0;
        stdplane->get_dim(rows, cols);
        bool save = true;

        while (true) {
            stdplane->erase();
            stdplane->perimeter_rounded(0, 0, 0);
            const int choice_row = static_cast<int>(rows) - 1;
            const std::string prompt = "Save configuration changes to file?";
            stdplane->putstr(choice_row, 2, prompt.c_str());
            const int yes_col = 2 + static_cast<int>(prompt.size()) + 4;
            const int no_col = yes_col + 8;
            if (save) {
                stdplane->set_bg_rgb8(255, 255, 255);
                stdplane->set_fg_rgb8(0, 0, 0);
                stdplane->putstr(choice_row, yes_col, "Yes");
                stdplane->set_bg_default();
                stdplane->set_fg_default();
                stdplane->putstr(choice_row, no_col, "No");
            } else {
                stdplane->putstr(choice_row, yes_col, "Yes");
                stdplane->set_bg_rgb8(255, 255, 255);
                stdplane->set_fg_rgb8(0, 0, 0);
                stdplane->putstr(choice_row, no_col, "No");
                stdplane->set_bg_default();
                stdplane->set_fg_default();
            }
            nc.render();

            ncinput ni{};
            timespec ts{0, 500'000'000};
            uint32_t ch = notcurses_get(nc, &ts, &ni);
            if (ch == 0) {
                continue;
            }
            if (ch == 'q' || ch == 'Q' || static_cast<int32_t>(ch) == -1) {
                break;
            }
            if (ch == NCKEY_LEFT || ch == NCKEY_RIGHT) {
                save = !save;
            } else if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
                if (save) {
                    if (!config.SaveToFile(config_path)) {
                        std::cerr << "Warning: failed to save config to " << config_path << "\n";
                    }
                }
                break;
            }
        }
    }
    return 0;
}

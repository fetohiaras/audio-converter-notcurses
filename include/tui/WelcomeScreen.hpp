#ifndef TUI_WELCOMESCREEN_HPP
#define TUI_WELCOMESCREEN_HPP

#include "tui/BaseScreen.hpp"

// First screen shown to the user; offers a basic exit path for now.
class WelcomeScreen : public BaseScreen {
public:
    WelcomeScreen();

    void Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void Exit(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void Draw(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override;
    void HandleInput(StateMachine& machine,
                     ncpp::NotCurses& nc,
                     ncpp::Plane& stdplane,
                     uint32_t input,
                     const ncinput& details) override;

private:
    bool exit_requested_;
};

#endif // TUI_WELCOMESCREEN_HPP

#ifndef TUI_BASESCREEN_HPP
#define TUI_BASESCREEN_HPP

#include <string>
#include <vector>

#include <ncpp/Plane.hh>

#include "tui/State.hpp"

// Base class that provides helpers for drawing common UI elements.
class BaseScreen : public State {
public:
    void Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override {}
    void Exit(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override {}
    void Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override {}

    // Still requires derived classes to implement frame drawing and input handling.
    virtual void Draw(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) override = 0;
    virtual void HandleInput(StateMachine& machine,
                             ncpp::NotCurses& nc,
                             ncpp::Plane& stdplane,
                             uint32_t input,
                             const ncinput& details) override = 0;

protected:
    // Clears the plane and writes each line centered vertically around mid-row.
    void ClearAndCenterLines(ncpp::Plane& plane, const std::vector<std::string>& lines);
    // Convenience for a single centered line.
    void PutCentered(ncpp::Plane& plane, int row, const std::string& text);
};

#endif // TUI_BASESCREEN_HPP

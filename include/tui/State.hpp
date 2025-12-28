#ifndef TUI_STATE_HPP
#define TUI_STATE_HPP

#include <cstdint>
#include <memory>

#include <ncpp/NotCurses.hh>
#include <ncpp/Plane.hh>

class StateMachine;

// Interface for a screen/state driven by the state machine.
class State {
public:
    virtual ~State() = default;

    // Called when the state becomes active.
    virtual void Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) = 0;
    // Called when transitioning away from the state.
    virtual void Exit(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) = 0;
    // Paints the current frame onto the provided plane.
    virtual void Draw(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) = 0;
    // Polled regularly even when there is no user input.
    virtual void Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) = 0;
    // Handles a single input event.
    virtual void HandleInput(StateMachine& machine,
                             ncpp::NotCurses& nc,
                             ncpp::Plane& stdplane,
                             uint32_t input,
                             const ncinput& details) = 0;
};

#endif // TUI_STATE_HPP

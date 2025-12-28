#ifndef TUI_STATEMACHINE_HPP
#define TUI_STATEMACHINE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <ncpp/NotCurses.hh>
#include <ncpp/Plane.hh>

class State;

// Lightweight state machine to swap between TUI screens.
class StateMachine {
public:
    StateMachine();
    ~StateMachine();

    void AddState(const std::string& name, std::shared_ptr<State> state);
    void TransitionTo(const std::string& name, ncpp::NotCurses& nc, ncpp::Plane& stdplane);
    std::shared_ptr<State> GetCurrentState() const;

    void SetRunning(bool running);
    void RequestStop();

    // Runs the main loop: draw, poll input, and dispatch to the active state.
    void Run(ncpp::NotCurses& nc, ncpp::Plane& stdplane);

private:
    std::unordered_map<std::string, std::shared_ptr<State>> states_;
    std::shared_ptr<State> current_state_;
    bool running_;
};

#endif // TUI_STATEMACHINE_HPP

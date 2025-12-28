#include "tui/StateMachine.hpp"

#include <csignal>
#include <cstdint>
#include <ctime>
#include <unordered_map>
#include <notcurses/notcurses.h>

#include "tui/Signal.hpp"
#include "tui/State.hpp"

StateMachine::StateMachine() : current_state_(nullptr), running_(true) {}

StateMachine::~StateMachine() = default;

void StateMachine::AddState(const std::string& name, std::shared_ptr<State> state) {
    states_[name] = state;
}

void StateMachine::TransitionTo(const std::string& name, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    // Swap the active state, honoring Exit/Enter hooks on the way out/in.
    std::unordered_map<std::string, std::shared_ptr<State>>::const_iterator it = states_.find(name);
    if (it == states_.cend()) {
        return;
    }

    if (current_state_ != nullptr) {
        current_state_->Exit(*this, nc, stdplane);
    }

    current_state_ = it->second;
    current_state_->Enter(*this, nc, stdplane);
}

std::shared_ptr<State> StateMachine::GetCurrentState() const {
    return current_state_;
}

void StateMachine::SetRunning(bool running) {
    running_ = running;
}

void StateMachine::RequestStop() {
    running_ = false;
}

void StateMachine::Run(ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    running_ = true;

    const timespec poll_timeout{0, 100'000'000}; // 100ms

    while (running_) {
        if (current_state_ == nullptr) {
            break;
        }

        // Always redraw before polling for input so the frame stays fresh.
        current_state_->Draw(*this, nc, stdplane);
        nc.render();

        ncinput input_details{};
        uint32_t ch = notcurses_get(nc, &poll_timeout, &input_details);

        // Allow Ctrl-C to break out promptly.
        if (g_sigint_received.load(std::memory_order_relaxed)) {
            running_ = false;
            break;
        }

        if (ch == 0) {
            // Timeout: give the state a chance to advance timers or async work.
            current_state_->Update(*this, nc, stdplane);
            continue;
        }

        if (static_cast<int32_t>(ch) == -1) {
            // Input error from notcurses_get; bail out to restore the terminal.
            running_ = false;
            break;
        }

        if (ch == 'q' || ch == 'Q') {
            // Global escape hatch; states can also call RequestStop().
            running_ = false;
            break;
        }

        // Dispatch the input to the active state, then tick its update hook.
        current_state_->HandleInput(*this, nc, stdplane, ch, input_details);
        current_state_->Update(*this, nc, stdplane);
    }

    // Give the active state a final chance to clean up.
    if (current_state_ != nullptr) {
        current_state_->Exit(*this, nc, stdplane);
    }
}

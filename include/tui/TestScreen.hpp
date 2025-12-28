#ifndef TUI_TESTSCREEN_HPP
#define TUI_TESTSCREEN_HPP

#include <memory>

#include "tui/BaseScreen.hpp"

// Minimal test screen: just a framed title for layout experiments.
class TestScreen : public BaseScreen {
public:
    TestScreen() = default;
    ~TestScreen() override = default;

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
    void EnsureSubframe(ncpp::Plane& stdplane, unsigned rows, unsigned cols);

    std::unique_ptr<ncpp::Plane> subframe_;
    unsigned cached_rows_ = 0;
    unsigned cached_cols_ = 0;
};

#endif // TUI_TESTSCREEN_HPP

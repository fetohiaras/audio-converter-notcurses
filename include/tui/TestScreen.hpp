#ifndef TUI_TESTSCREEN_HPP
#define TUI_TESTSCREEN_HPP

#include <memory>

#include "tui/BaseScreen.hpp"
#include "tui/Subframe.hpp"

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
    class TestSubframe : public Subframe {
    public:
        void HandleInputPublic(uint32_t input, const ncinput& details) { HandleInput(input, details); }

    protected:
        void ComputeGeometry(unsigned parent_rows,
                             unsigned parent_cols,
                             int& y,
                             int& x,
                             int& rows,
                             int& cols) override;
        void DrawContents() override;
        void HandleInput(uint32_t input, const ncinput& details) override;

    private:
        void DrawList();

        std::vector<std::string> items_{"test string 1", "test string 2", "test string 3", "test string 4"};
        int selected_index_ = 0;
    };

    TestSubframe subframe_;
};

#endif // TUI_TESTSCREEN_HPP

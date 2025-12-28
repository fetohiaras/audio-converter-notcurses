#include "tui/TestScreen.hpp"

#include <notcurses/notcurses.h>

#include "tui/StateMachine.hpp"

void TestScreen::Enter(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
}

void TestScreen::Exit(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
}

void TestScreen::Draw(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    stdplane.erase();
    stdplane.perimeter_rounded(0, 0, 0);
    // Center the main title inside the outer frame.
    unsigned rows = 0;
    unsigned cols = 0;
    stdplane.get_dim(rows, cols);
    stdplane.putstr(0, ncpp::NCAlign::Center, "Test Screen");

    subframe_.Resize(stdplane, rows, cols);
    subframe_.Draw();
}

void TestScreen::Update(StateMachine& machine, ncpp::NotCurses& nc, ncpp::Plane& stdplane) {
    (void)machine;
    (void)nc;
    (void)stdplane;
}

void TestScreen::HandleInput(StateMachine& machine,
                             ncpp::NotCurses& nc,
                             ncpp::Plane& stdplane,
                             uint32_t input,
                             const ncinput& details) {
    (void)machine;
    (void)nc;
    (void)stdplane;
    (void)details;
    // No interactions yet; this is a static layout placeholder.
}

void TestScreen::TestSubframe::ComputeGeometry(unsigned parent_rows,
                                               unsigned parent_cols,
                                               int& y,
                                               int& x,
                                               int& rows,
                                               int& cols) {
    rows = static_cast<int>(parent_rows) / 2;
    cols = static_cast<int>(parent_cols) / 2;
    y = static_cast<int>(parent_rows) / 4;
    x = static_cast<int>(parent_cols) / 4;
}

void TestScreen::TestSubframe::DrawContents() {
    plane_->perimeter_rounded(0, 0, 0);
    plane_->putstr(0, ncpp::NCAlign::Center, "Subframe 1");
}

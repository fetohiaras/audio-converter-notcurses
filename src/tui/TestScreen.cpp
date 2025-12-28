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

    EnsureSubframe(stdplane, rows, cols);
    if (subframe_ != nullptr) {
        subframe_->erase();
        subframe_->perimeter_rounded(0, 0, 0);
        subframe_->putstr(0, ncpp::NCAlign::Center, "Subframe 1");
    }
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

void TestScreen::EnsureSubframe(ncpp::Plane& stdplane, unsigned rows, unsigned cols) {
    // Avoid creating subframes on tiny terminals.
    if (rows < 4 || cols < 10) {
        subframe_.reset();
        return;
    }

    const unsigned inner_rows = rows / 2;
    const unsigned inner_cols = cols / 2;
    const int inner_y = static_cast<int>(rows) / 4;
    const int inner_x = static_cast<int>(cols) / 4;

    // Recreate the subframe whenever the outer dimensions change.
    if (subframe_ == nullptr || cached_rows_ != inner_rows || cached_cols_ != inner_cols) {
        subframe_ = std::make_unique<ncpp::Plane>(&stdplane,
                                                  static_cast<int>(inner_rows),
                                                  static_cast<int>(inner_cols),
                                                  inner_y,
                                                  inner_x);
        cached_rows_ = inner_rows;
        cached_cols_ = inner_cols;
    }
}
